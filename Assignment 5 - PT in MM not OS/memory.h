#ifndef MEMORY_H
#define MEMORY_H

/* Number of processors in the system (compile-time constant) */
#define NP 4

#define MEMSIZE 8192 // total physical memory, shared across all processors
#define PAGESIZE 512 // bytes per page/frame

#define NUM_PHYSICAL_PAGES (MEMSIZE / PAGESIZE) // 16 frames total - frame 0 reserved, never allocated
#define NUM_LOGICAL_PAGES (1024 / PAGESIZE + 4096 / PAGESIZE)

// extern char Instruction[NP][INSTR_MEM_SIZE];
// extern char Data[NP][DATA_MEM_SIZE];
extern unsigned char memory[MEMSIZE];

/* Page table now lives in a physical frame. This array says which frame
   holds each processor's page table (0 = no page table loaded). */
extern int pageTableFrame[NP];

/* PTE layout: 1 byte per entry, NUM_LOGICAL_PAGES entries, packed at the
   start of the page table's frame. 0 = unmapped (frame 0 is reserved). */
#define PTE_SIZE 1

extern char freePages[NUM_PHYSICAL_PAGES];

int getFreePage(void);
void freePage(int frame);
int free_page_count(void);

int getPhysicalAddress(int proc_id, int isFetch, int address);

#define INSTR_MEM_SIZE 256
#define DATA_MEM_SIZE 4096

int initialize(int proc_id, const char* instruction_filename, const char* memory_filename);
int count_pages_needed(const char* instruction_filename, const char* memory_filename);
void finalize(int proc_id, const char* memory_filename);

int read_word(int proc_id, int address);
void write_word(int proc_id, int address, int value);

#endif

