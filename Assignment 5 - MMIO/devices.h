#ifndef DEVICES_H
#define DEVICES_H

/* Byte offsets within the MMIO page (word-aligned) */
#define DEV_CONSOLE_OUT 0x00   /* write: low byte printed to stdout */
#define DEV_CONSOLE_IN  0x04   /* read:  0 if no char pending, else last char */
#define DEV_TIMER       0x08   /* read:  instruction tick count */
#define DEV_RANDOM      0x0C   /* read:  next pseudorandom 15-bit number */

void dev_reset(void);                /* called from os_init() */
void dev_tick(void);                 /* called once per executed instruction */
void dev_feed_input(int ch);         /* shell pushes typed chars here */
int  dev_read(int offset);           /* 32-bit read from device register */
void dev_write(int offset, int val); /* 32-bit write to device register */

#endif