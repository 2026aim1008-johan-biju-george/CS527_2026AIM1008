#ifndef MEMORY_H
#define MEMORY_H

/* Number of processors in the system (compile-time constant) */
#define NP 1

#define MEMSIZE 32768 // total physical memory, shared across all processors (64 frames, usable = 63)
#define PAGESIZE 512 // bytes per page/frame

#define NUM_PHYSICAL_PAGES (MEMSIZE / PAGESIZE) // 16 frames total - frame 0 reserved, never allocated
#define NUM_LOGICAL_PAGES (1024 / PAGESIZE + 4096 / PAGESIZE)

extern unsigned char memory[MEMSIZE];

#define MAX_MEM_SLOTS 64   // max tasks
extern char pageTable[MAX_MEM_SLOTS][NUM_LOGICAL_PAGES];
extern int current_mem_slot;   // which task's page table is active
extern char freePages[NUM_PHYSICAL_PAGES];

int getFreePage(void);
void freePage(int frame);
int free_page_count(void);

int getPhysicalAddress(int mem_slot, int isFetch, int address);

#define INSTR_MEM_SIZE 256
#define DATA_MEM_SIZE 4096

int initialize(int mem_slot, const char* instruction_filename, const char* memory_filename);
int count_pages_needed(const char* instruction_filename, const char* memory_filename);
void finalize(int mem_slot, const char* memory_filename);

int read_word(int mem_slot, int address);
void write_word(int mem_slot, int address, int value);

#endif

