/* devices.c */
#include "devices.h"
#include <stdio.h>

static int tick_count = 0;
static unsigned int rand_state = 12345;
static int console_in_char = 0;

void dev_reset(void){
    tick_count = 0;
    rand_state = 12345;
    console_in_char = 0;
}

void dev_tick(void){
    tick_count++;
}

void dev_feed_input(int ch){
    console_in_char = ch & 0xFF;
}

int dev_read(int offset){
    switch(offset & ~3){
        case DEV_CONSOLE_IN: {
            int c = console_in_char;
            console_in_char = 0;      /* consuming the char */
            return c;
        }
        case DEV_TIMER:
            return tick_count;
        case DEV_RANDOM:
            rand_state = rand_state * 1103515245u + 12345u;
            return (int)((rand_state >> 16) & 0x7FFF);
        default:
            return 0;                 /* reads from write-only regs */
    }
}

void dev_write(int offset, int val){
    switch(offset & ~3){
        case DEV_CONSOLE_OUT:
            putchar(val & 0xFF);
            fflush(stdout);
            break;
        default:
            break;                    /* writes to read-only regs ignored */
    }
}