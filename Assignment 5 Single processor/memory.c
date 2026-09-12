#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char memory[MEMSIZE];
char pageTable[MAX_MEM_SLOTS][NUM_LOGICAL_PAGES];
char freePages[NUM_PHYSICAL_PAGES];

int current_mem_slot = -1;
static int mmu_initialized = 0;

/* Frame 0 is reserved and must never be handed out - mark it allocated
   at program start so getFreePage() skips it forever. Page tables are
   already zero-initialized by C (static/global storage), which
   means "no logical page mapped yet" per processor. */
static void mmu_init_once(void){
    if(mmu_initialized) return;
    freePages[0] = 1; // reserved
    mmu_initialized = 1;
}

/* Returns a free physical page number and marks it allocated. Exits
   with an error if no free page is available. */
int getFreePage(void){
    mmu_init_once();
    for(int i = 1; i < NUM_PHYSICAL_PAGES; i++){
        if(!freePages[i]){
            freePages[i] = 1;
            return i;
        }
    }
    printf("ERROR: no free physical page available - out of memory\n");
    exit(1);
}

/* Frees a physical page, making it available for allocation. */
void freePage(int frame){
    if(frame <= 0 || frame >= NUM_PHYSICAL_PAGES) return;
    freePages[frame] = 0;
}

/* Counts physical frames not currently allocated (frame 0 is always
   excluded - it's permanently reserved and never handed out). Lets a
   caller check availability without actually claiming a frame. */
int free_page_count(void){
    mmu_init_once();
    int count = 0;
    for(int i = 1; i < NUM_PHYSICAL_PAGES; i++){
        if(!freePages[i]) count++;
    }
    return count;
}

/* Returns the physical address corresponding to a logical address, or exits
   with an error if the logical page is not mapped. */
int getPhysicalAddress(int mem_slot, int isFetch, int address){
    mmu_init_once();
    int page_index = isFetch ? (address / PAGESIZE) : (address / PAGESIZE + 1024 / PAGESIZE);

    if(page_index < 0 || page_index >= NUM_LOGICAL_PAGES){
        printf("ERROR: proc %d address %d (isFetch=%d) maps to out-of-range logical page %d\n",
            mem_slot, address, isFetch, page_index);
        exit(1);
    }

    int frame = pageTable[mem_slot][page_index];
    if(frame == 0){
        printf("ERROR: proc %d address %d (isFetch=%d) is on unmapped logical page %d\n",
            mem_slot, address, isFetch, page_index);
        exit(1);
    }

    return frame * PAGESIZE + (address % PAGESIZE);
}

/* Number of pages needed to hold 'n' bytes is always at least one (so logical
   address 0 of an empty region is still valid), clamped to max_pages so a
   region can never claim more pages than its logical space allows. */
static int pages_needed_for(int n_bytes, int max_pages){
    int num_pages = (n_bytes + PAGESIZE - 1) / PAGESIZE;
    if(num_pages == 0) num_pages = 1;
    if(num_pages > max_pages) num_pages = max_pages; // clamp - shouldn't happen if caller respected size limits
    return num_pages;
}

/* Allocates however many physical frames are needed to cover 'n' bytes
   starting at logical address 0 of the given region (instruction or
   data), maps them into mem_slot's page table, and copies buf[0..n_bytes)
   into physical memory through those frames. Always allocates at least
   one page, so accessing logical address 0 works even for an empty
   region. Returns the number of pages allocated. */
static int load_region(int mem_slot, int isFetch, const char *buf, int n_bytes){
    int num_pages = (n_bytes + PAGESIZE - 1) / PAGESIZE;
    if(num_pages == 0) num_pages = 1;

    int base_index = isFetch ? 0 : (1024 / PAGESIZE);
    int max_pages = isFetch ? (1024 / PAGESIZE) : (4096 / PAGESIZE);
    if(num_pages > max_pages) num_pages = max_pages; // clamp - shouldn't happen if caller respected size limits

    for(int p = 0; p < num_pages; p++){
        int frame = getFreePage();
        pageTable[mem_slot][base_index + p] = (char)frame;

        int copy_start = p * PAGESIZE;
        int copy_len = PAGESIZE;
        if(copy_start + copy_len > n_bytes){
            copy_len = n_bytes - copy_start;
            if(copy_len < 0) copy_len = 0;
        }
        memcpy(&memory[frame * PAGESIZE], &buf[copy_start], copy_len);
        if(copy_len < PAGESIZE){
            memset(&memory[frame * PAGESIZE + copy_len], 0, PAGESIZE - copy_len);
        }
    }
    return num_pages;
}

/* Loads the compiled instructions from 'program.byte' and the contents
   of 'data.byte', allocating physical frames page-by-page and
   recording the mapping in mem_slot's page table. Returns 1 on success,
   0 if either file could not be opened. */
int initialize(int mem_slot, const char* instruction_filename, const char* memory_filename){
    FILE *iptr = fopen(instruction_filename, "r");
    if(!iptr){
        printf("Error opening instruction file %s\n", instruction_filename);
        return 0;
    }
    static char instr_buf[INSTR_MEM_SIZE];
    int val, n = 0;
    while(n < INSTR_MEM_SIZE && fscanf(iptr, "%2x", &val) == 1){
        instr_buf[n++] = (char)val;
    }
    fclose(iptr);

    FILE *dptr = fopen(memory_filename, "r");
    if(!dptr){
        printf("Error opening data file %s\n", memory_filename);
        return 0;
    }
    static char data_buf[DATA_MEM_SIZE];
    int m = 0;
    while(m < DATA_MEM_SIZE && fscanf(dptr, "%2x", &val) == 1){
        data_buf[m++] = (char)val;
    }
    fclose(dptr);

    int instr_pages = load_region(mem_slot, 1, instr_buf, n);
    int data_pages = load_region(mem_slot, 0, data_buf, m);
    printf("MMU: slot %d loaded %d instruction bytes (%d page%s) + %d data bytes (%d page%s)\n",
        mem_slot, n, instr_pages, instr_pages == 1 ? "" : "s", m, data_pages, data_pages == 1 ? "" : "s");

    return 1;
}

/* Reads instruction_filename and memory_filename just far enough to learn
   their sizes (same bounds as initialize()), and returns how many physical
   frames loading both regions would require - without allocating any
   frames or touching the page table. Lets the loader check memory
   availability before committing a task to a processor. Returns -1 if
   either file can't be opened. */
int count_pages_needed(const char* instruction_filename, const char* memory_filename){
    FILE *iptr = fopen(instruction_filename, "r");
    if(!iptr) return -1;
    int val, n = 0;
    while(n < INSTR_MEM_SIZE && fscanf(iptr, "%2x", &val) == 1) n++;
    fclose(iptr);

    FILE *dptr = fopen(memory_filename, "r");
    if(!dptr) return -1;
    int m = 0;
    while(m < DATA_MEM_SIZE && fscanf(dptr, "%2x", &val) == 1) m++;
    fclose(dptr);

    int instr_pages = pages_needed_for(n, 1024 / PAGESIZE);
    int data_pages = pages_needed_for(m, 4096 / PAGESIZE);
    return instr_pages + data_pages;
}

/* Writes the current contents of mem_slot's data region back to memory_filename 
   or data.byte (the full 4096-byte logical data region, and unmapped pages 
   read back as zero via read_word()'s fallback), then frees every physical frame 
   this task held and clears its page table so those frames can be reused by the
   next task. */
void finalize(int mem_slot, const char* memory_filename){
    FILE *dptr = fopen(memory_filename, "w");
    if(!dptr){
        printf("Error opening data file %s for write\n", memory_filename);
        return;
    }
    for(int i = 0; i < DATA_MEM_SIZE; i += 4){
        unsigned int word = (unsigned int)read_word(mem_slot, i);
        fprintf(dptr, "%02X %02X %02X %02X\n",
            (word >> 24) & 0xFF, (word >> 16) & 0xFF, (word >> 8) & 0xFF, word & 0xFF);
    }
    fclose(dptr);

    for(int i = 0; i < NUM_LOGICAL_PAGES; i++){
        int frame = pageTable[mem_slot][i];
        if(frame != 0){
            freePage(frame);
            pageTable[mem_slot][i] = 0;
        }
    }
}

/* Reads one 32-bit word from mem_slot's logical data space at address i.
   Each byte is translated independently, since a 4-byte word can straddle 
   two physical frames near a page boundary. 
   Reads from an unmapped page return 0 rather than treating it as fatal,
   since finalize() always reads the full 4096-byte region even though
   only the pages that were actually used got mapped in initialize(). */
int read_word(int mem_slot, int i){
    if(i < 0 || i > DATA_MEM_SIZE - 4){
        printf("Memory read error: invalid address %d\n", i);
        return 0;
    }

    unsigned int val = 0;
    for(int b = 0; b < 4; b++){
        int logical_addr = i + b;
        int page_index = logical_addr / PAGESIZE + 1024 / PAGESIZE;
        int frame = pageTable[mem_slot][page_index];
        unsigned char byte = 0;
        if(frame != 0){
            int phys = frame * PAGESIZE + (logical_addr % PAGESIZE);
            byte = (unsigned char)memory[phys];
        }
        val = (val << 8) | byte;
    }
    return (int)val;
}

/* Writes one 32-bit word into mem_slot's logical data space at address i.
   Translates each byte independently (a word can straddle a page
   boundary). Unlike a raw translation, this lazily allocates a fresh,
   zeroed page the first time a given data page is written to. */
void write_word(int mem_slot, int i, int val){
    if(i < 0 || i > DATA_MEM_SIZE - 4){
        printf("Memory write error: invalid address %d\n", i);
        return;
    }

    for(int b = 0; b < 4; b++){
        int logical_addr = i + b;
        int page_index = logical_addr / PAGESIZE + 1024 / PAGESIZE;

        if(pageTable[mem_slot][page_index] == 0){
            int frame = getFreePage();
            pageTable[mem_slot][page_index] = (char)frame;
            memset(&memory[frame * PAGESIZE], 0, PAGESIZE);
        }

        int phys = getPhysicalAddress(mem_slot, 0, logical_addr); // now guaranteed mapped
        unsigned char byte = (unsigned char)((val >> (8 * (3 - b))) & 0xFF);
        memory[phys] = (char)byte;
    }
}