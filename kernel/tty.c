
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                               tty.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

//本次作业修改自chapter7/h
//部分代码添加自chapter7/m chapter7/n
#include "type.h"
#include "const.h"
#include "protect.h"
#include "proto.h"
#include "string.h"
#include "proc.h"
#include "global.h"
#include "keyboard.h"

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
//默认情况下 屏幕下的每个字符占两个字节
#define TEXT_SIZE V_MEM_SIZE / 2
#define LED_CODE 0xED
#define KB_ACK 0xFA
//此处提供三缓存区
//第一个缓冲区 显存buffer区
PRIVATE u8 *vmpointer = V_MEM_BASE;
PRIVATE u8 vmbuf[V_MEM_SIZE];
//因为显存中默认一个字符占用两个字节，低8位为'真正'的char.此处的text中存储的是真正的char

//text没有任何存在的意义 仅仅是方便了大家 
//第二个缓冲区 text缓冲区
PRIVATE u8 text[TEXT_SIZE];
PRIVATE int text_Pointer;
PRIVATE int isInput =1;
PRIVATE int isSearching =0;
PRIVATE int endSearching =0;
//第三个缓冲区 ese后的输入
PRIVATE char input[TEXT_SIZE];
PRIVATE int input_Pointer;
PRIVATE int cursor_pos;
PRIVATE char caps_lock;
PRIVATE char scroll_lock;
PRIVATE char num_lock;
PRIVATE void reflash();

PUBLIC void task_tty()
{
    memset(text, 0, TEXT_SIZE);
    text_Pointer = 0;
    isInput =1;
    isSearching =0;
    endSearching =0;
    caps_lock = 0;
    reflash();
    while (1) {
        keyboard_read();
    }
}

PUBLIC void screen_clear()
{
    while (1) {
        milli_delay(200000);
        if (isInput == 1) {
            memset(text, 0, TEXT_SIZE);
            text_Pointer = 0;
            reflash();
        }
    }
}

PRIVATE char revert_Char(u32 key) {

    u32 ch = key & 0xFF;
    if(caps_lock==1){
        //a --> A
        //A --> a
    if(ch >='A'&&ch <='Z')
    {
        ch +=32;
    }
    else{
        ch -=32;
    }
    }

    return ch;
}
/*======================================================================*
				 kb_wait
 *======================================================================*/
PRIVATE void kb_wait()	/* 等待 8042 的输入缓冲区空 */
{
	u8 kb_stat;

	do {
		kb_stat = in_byte(KB_CMD);
	} while (kb_stat & 0x02);
}


/*======================================================================*
				 kb_ack
 *======================================================================*/
PRIVATE void kb_ack()
{
	u8 kb_read;

	do {
		kb_read = in_byte(KB_DATA);
	} while (kb_read =! KB_ACK);
}

/*======================================================================*
				 set_leds
 *======================================================================*/
PRIVATE void set_leds()
{
	u8 leds = (caps_lock << 2) | (num_lock << 1) | scroll_lock;

	kb_wait();
	out_byte(KB_DATA, LED_CODE);
	kb_ack();

	kb_wait();
	out_byte(KB_DATA, leds);
	kb_ack();
}
PRIVATE void reflash()
{
    // clear vmem buffer
    for (int i = 0; i <TEXT_SIZE; i++) {
        vmbuf[i * 2] = 0x00;
    //修改为了0x0f 等待变化
        vmbuf[i * 2 + 1] = 0x0f;
    }

    int row = 0;
    int col = 0;
    for (int i = 0; i < text_Pointer; i++) {
        switch (text[i]) {
        case '\t':
            if(col+4<=24){
            col =col +4;
            }
            else{
            col = col +4;
            col = col % 4; 
            row =row + 1;   
            }
            
            break;
        case '\n':
            col = 0;
            row = (row + 1);
            if(row>SCREEN_HEIGHT){
                row =row -SCREEN_HEIGHT;
            }
            break;
        default:
//先更新所有char(8位)
            vmbuf[(row * SCREEN_WIDTH + col) * 2] = text[i];
            if(endSearching==1){
               for(int j =0;j<i-input_Pointer;j++){
                   int isMatch =1;
                   for(int k=j;k<input_Pointer;k++){
                   if(text[i+k]!=input[k]){
                   isMatch =0;    
                   }
                   }
                   if(isMatch==1){
                   for (int n=j; n<j+input_Pointer; n++) vmbuf[(row * SCREEN_WIDTH + col + n) * 2 - 1] = 0x0d;
 
                   }
               }
            }

            col++;
        }
    }
    if(isSearching==1||endSearching==1){
    for (int i = 0; i < input_Pointer; i++) {
        vmbuf[(row * SCREEN_WIDTH + col) * 2] = input[i];
        vmbuf[(row * SCREEN_WIDTH + col) * 2 + 1] = 0x0d;
        col++;
    }
    }
//vmbuf -> vm 来自orange代码 中断  
    memcpy(vmpointer, vmbuf, V_MEM_SIZE);
    int position =row *SCREEN_WIDTH +col;


    disable_int();
    out_byte(CRTC_ADDR_REG, CURSOR_H);
    out_byte(CRTC_DATA_REG, (position >> 8) & 0xFF);
    out_byte(CRTC_ADDR_REG, CURSOR_L);
    out_byte(CRTC_DATA_REG, position & 0xFF);
    cursor_pos = position;
    enable_int();
// over
}



PUBLIC void in_process(u32 key)
{      
    int raw_code = key& MASK_RAW;
    if ( (key & MASK_RAW) == CAPS_LOCK) {
        if(caps_lock==0){
            caps_lock =1;
        }
        else{
            caps_lock =0;
        }
        set_leds();
    } else if (isInput == 1) {
        if (!(key & FLAG_EXT)) {
            text[text_Pointer] = revert_Char(key);
            text_Pointer++;
        } else {
            int raw_code =key & MASK_RAW;
            switch(raw_code) {
            case ENTER:
                text[text_Pointer] = '\n';
                text_Pointer++;
                break;
            case TAB:
                text[text_Pointer] = '\t';
                text_Pointer++;
                break;
            case BACKSPACE:
                if (text_Pointer > 0){
                text_Pointer--;
                text[text_Pointer] =0;
                }
                break;
            case ESC:
                memset(input, 0, TEXT_SIZE);
                input_Pointer = 0;
                isInput =0;
                isSearching = 1;
            }
        }
    } else if (isSearching == 1) {
        if (!(key & FLAG_EXT)) {
            input[input_Pointer] = revert_Char(key);
            input_Pointer++;
        } else {
            int raw_code =key & MASK_RAW;
            switch (raw_code) {
            case ENTER:
                isSearching =0;
                endSearching = 1;
                input_Pointer++;
                break;
            case ESC:
                isSearching =0;
                endSearching =0;
                isInput =1;
            }
        }
    } else if (endSearching ==1) {
        endSearching =0;
        isSearching =0;
        isInput =1;
    }
    reflash();
}
