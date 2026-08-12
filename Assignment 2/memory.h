#ifndef MEMORY_H
#define MEMORY_H

extern char Instruction[256], Data[4096];

void initialize();
void finalize();

int read_word(int address);
void write_word(int address, int value);

#endif