#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char memory[MEMSIZE];
int pageTableFrame[NP];
char freePages[NUM_PHYSICAL_PAGES];

static int mmu_initialized = 0;

/* memory.c — after the other globals */
TLBEntry tlb[NP][TLB_SIZE]; /* zero-initialized → all invalid */
unsigned long tlb_hits[NP];
unsigned long tlb_misses[NP];

/* Invalidates every TLB entry for this processor AND resets its hit/miss
   counters. Called on task load and unload: on load, to discard the
   previous task's translations; on unload, to forget frames that are
   about to be returned to the free pool. */
void flush_tlb(int proc_id){
    for(int i = 0; i < TLB_SIZE; i++){
        tlb[proc_id][i].valid = 0;
    }
    tlb_hits[proc_id] = 0;
    tlb_misses[proc_id] = 0;
}

void tlb_stats(int proc_id, unsigned long *hits, unsigned long *misses){
    *hits = tlb_hits[proc_id];
    *misses = tlb_misses[proc_id];
}

/* Direct-mapped lookup: slot = vpn % TLB_SIZE. 
   Two VPNs that differ by a multiple of TLB_SIZE collide in the same slot - the
   second access evicts the first. That's the classic direct-mapped conflict miss. */
static int tlb_lookup(int proc_id, int vpn, int *frame_out){
    int idx = vpn % TLB_SIZE;
    TLBEntry *e = &tlb[proc_id][idx];
    if(e->valid && e->vpn == vpn){
        *frame_out = e->frame;
        return 1;
    }
    return 0;
}

/* Installs a translation into the direct-mapped slot, overwriting whatever
   was there. Called only for pages that are actually mapped (frame != 0). */
static void tlb_insert(int proc_id, int vpn, int frame){
    int idx = vpn % TLB_SIZE;
    tlb[proc_id][idx].valid = 1;
    tlb[proc_id][idx].vpn   = vpn;
    tlb[proc_id][idx].frame = frame;
}

/* Frame 0 is reserved and must never be handed out - mark it allocated
   at program start so getFreePage() skips it forever. Each task's page
   table lives in a physical frame allocated by initialize(), and is
   zeroed there before any PTEs are written. */
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

/* Returns the frame that (proc_id, page_index) maps to, or 0 if unmapped.
   Consults the TLB first; on a miss, walks the page table and caches the
   result. Does NOT allocate a frame for an unmapped page. */
static int translate_page(int proc_id, int page_index){
    int frame;
    if(tlb_lookup(proc_id, page_index, &frame)){
        tlb_hits[proc_id]++;
        return frame;
    }
    tlb_misses[proc_id]++;

    int pt_frame = pageTableFrame[proc_id];
    if(pt_frame == 0){
        printf("ERROR: proc %d has no page table loaded\n", proc_id);
        exit(1);
    }
    frame = memory[pt_frame * PAGESIZE + page_index * PTE_SIZE];
    if(frame != 0){
        tlb_insert(proc_id, page_index, frame);   /* only cache real mappings */
    }
    return frame;
}

/* Same as translate_page, but lazily allocates a fresh zeroed frame the
   first time a given page is touched. Used by write_word(). */
static int translate_page_alloc(int proc_id, int page_index){
    int frame;
    if(tlb_lookup(proc_id, page_index, &frame)){
        tlb_hits[proc_id]++;
        return frame;
    }
    tlb_misses[proc_id]++;

    int pt_frame = pageTableFrame[proc_id];
    if(pt_frame == 0){
        printf("ERROR: proc %d has no page table loaded\n", proc_id);
        exit(1);
    }

    frame = memory[pt_frame * PAGESIZE + page_index * PTE_SIZE];
    if(frame == 0){
        frame = getFreePage();
        memory[pt_frame * PAGESIZE + page_index * PTE_SIZE] = (unsigned char)frame;
        memset(&memory[frame * PAGESIZE], 0, PAGESIZE);
    }
    tlb_insert(proc_id, page_index, frame);
    return frame;
}

/* Returns the frame that (proc_id, page_index) maps to, or 0 if unmapped.
   Consults the TLB first (fast path, no memory access). On a miss, walks
   the page table by reading the PTE out of the PT frame in physical
   memory - the extra access a TLB exists to hide - and caches the result.
   Unmapped pages are NOT cached, so a later lazy allocation of the same
   page will still miss and find the PTE == 0. */
int getPhysicalAddress(int proc_id, int isFetch, int address){
    mmu_init_once();
    int page_index = isFetch ? (address / PAGESIZE) : (address / PAGESIZE + 1024 / PAGESIZE);

    if(page_index < 0 || page_index >= NUM_LOGICAL_PAGES){
        printf("ERROR: proc %d address %d (isFetch=%d) maps to out-of-range logical page %d\n",
            proc_id, address, isFetch, page_index);
        exit(1);
    }
    
    /* Ask the TLB-aware translator for the frame. On a TLB hit this
       returns immediately; on a miss it walks the page table (reading
       the PTE from the PT frame in physical memory) and caches the
       result. Either way we get back a frame number, or 0 if unmapped. */
    int frame = translate_page(proc_id, page_index);
    if(frame == 0){
        printf("ERROR: proc %d address %d (isFetch=%d) is on unmapped logical page %d\n",
            proc_id, address, isFetch, page_index);
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
   data), writes PTEs into proc_id's page table (which lives in the
   physical frame recorded in pageTableFrame[proc_id]), and copies
   buf[0..n_bytes) into physical memory through those frames. Always
   allocates at least one page, so accessing logical address 0 works
   even for an empty region. Caller must have set pageTableFrame[proc_id]
   (initialize() does this before calling us). Returns the number of
   pages allocated. */
static int load_region(int proc_id, int isFetch, const char *buf, int n_bytes){
    int num_pages = (n_bytes + PAGESIZE - 1) / PAGESIZE;
    if(num_pages == 0) num_pages = 1;

    int base_index = isFetch ? 0 : (1024 / PAGESIZE);
    int max_pages = isFetch ? (1024 / PAGESIZE) : (4096 / PAGESIZE);
    if(num_pages > max_pages) num_pages = max_pages; // clamp - shouldn't happen if caller respected size limits
    
    int pt_frame = pageTableFrame[proc_id]; // set by initialize() before running
    for(int p = 0; p < num_pages; p++){
        int frame = getFreePage();
        int page_index = base_index + p;

        /* Write PTE into the page table's frame in physical memory */
        int pte_addr = pt_frame * PAGESIZE + page_index * PTE_SIZE;
        memory[pte_addr] = (unsigned char)frame;

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
   of 'data.byte', allocating physical frames page-by-page and recording
   the mapping in proc_id's page table - which itself occupies one
   physical frame (allocated here). Returns 1 on success, 0 if either
   file could not be opened. */
int initialize(int proc_id, const char* instruction_filename, const char* memory_filename){
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

    /* allocate a physical frame to hold this processor's page table */
    int pt_frame = getFreePage();
    pageTableFrame[proc_id] = pt_frame;
    memset(&memory[pt_frame * PAGESIZE], 0, PAGESIZE);   /* all PTEs are 0 = unmapped */
    
    /* A previous task on this processor may have left TLB entries pointing
       at frames that have since been freed. Invalidate them before
       loading the new task's page table. */
    flush_tlb(proc_id);

    int instr_pages = load_region(proc_id, 1, instr_buf, n);
    int data_pages = load_region(proc_id, 0, data_buf, m);

    printf("MMU: proc %d loaded %d instruction bytes (%d page%s) + %d data bytes (%d page%s), PT in frame %d\n",
        proc_id, n, instr_pages, instr_pages == 1 ? "" : "s",
        m, data_pages, data_pages == 1 ? "" : "s", pt_frame);

    printf("PT frame %d contents:", pt_frame);
    for(int i = 0; i < NUM_LOGICAL_PAGES; i++){
        printf(" %d", memory[pt_frame * PAGESIZE + i * PTE_SIZE]);
    }
    printf("\n");

    return 1;
}

/* Reads instruction_filename and memory_filename just far enough to learn
   their sizes (same bounds as initialize()), and returns how many physical
   frames loading both regions would require - including the one frame that
   will hold the task's page table. Does not allocate any frames or touch
   any page table. Lets the loader check memory availability before
   committing a task. Returns -1 if either file can't be opened. */
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
    return instr_pages + data_pages + 1; // +1 for PT itself
}

/* Writes the current contents of proc_id's data region back to memory_filename
   (the full 4096-byte logical data region; unmapped pages read back as zero
   via read_word()'s fallback), then frees every physical frame this task
   held - including the frame that held its page table - and clears
   pageTableFrame[proc_id] so the slot can be reused by the next task.
   Write-back must happen first, because read_word() still needs the page
   table to translate addresses. */
void finalize(int proc_id, const char* memory_filename){
    FILE *dptr = fopen(memory_filename, "w");
    if(!dptr){
        printf("Error opening data file %s for write\n", memory_filename);
    }
    else{
        for(int i = 0; i < DATA_MEM_SIZE; i += 4){
            unsigned int word = (unsigned int)read_word(proc_id, i);
            fprintf(dptr, "%02X %02X %02X %02X\n",
                (word >> 24) & 0xFF, (word >> 16) & 0xFF, (word >> 8) & 0xFF, word & 0xFF);
        }
        fclose(dptr);
    }

    /* Freeing happens only after write-back, because read_word() above
       needs the page table to translate addresses. */
    int pt_frame = pageTableFrame[proc_id];
    for(int page_index = 0; page_index < NUM_LOGICAL_PAGES; page_index++){
        int pte_addr = pt_frame * PAGESIZE + page_index * PTE_SIZE;
        int frame = memory[pte_addr];
        if(frame != 0){
            freePage(frame);
            memory[pte_addr] = 0;
        }
    }
    freePage(pt_frame);
    pageTableFrame[proc_id] = 0;

    unsigned long h, m;
    tlb_stats(proc_id, &h, &m);
    printf("TLB: proc %d — hits=%lu misses=%lu (hit rate %.1f%%)\n",
        proc_id, h, m, (h + m) ? 100.0 * h / (h + m) : 0.0);

    /* Frames are now free; drop any TLB entries that still reference them. */
    flush_tlb(proc_id);
}

/* Reads one 32-bit word from proc_id's logical data space at address i.
   Each byte is translated independently, since a 4-byte word can straddle 
   two physical frames near a page boundary. 
   Reads from an unmapped page return 0 rather than treating it as fatal,
   since finalize() always reads the full 4096-byte region even though
   only the pages that were actually used got mapped in initialize().
   (Contrast with getPhysicalAddress(), which treats an unmapped page as
   a fatal error - here the tolerant behavior is required to walk the
   full data region during write-back.) */
int read_word(int proc_id, int i){
    if(i < 0 || i > DATA_MEM_SIZE - 4){
        printf("Memory read error: invalid address %d\n", i);
        return 0;
    }
    
    unsigned int val = 0;
    for(int b = 0; b < 4; b++){
        int logical_addr = i + b;
        int page_index = logical_addr / PAGESIZE + 1024 / PAGESIZE;
        int frame = translate_page(proc_id, page_index); // TLB-aware
        unsigned char byte = 0;
        if(frame != 0){
            int phys = frame * PAGESIZE + (logical_addr % PAGESIZE);
            byte = (unsigned char)memory[phys];
        }
        val = (val << 8) | byte;
    }
    return (int)val;
}

/* Writes one 32-bit word into proc_id's logical data space at address i.
   Translates each byte independently (a word can straddle a page boundary).
   Unlike a raw translation, this lazily allocates a fresh, zeroed page the
   first time a given data page is written to, and records the new mapping
   by writing the frame number into the corresponding PTE of the page
   table's physical frame. */
void write_word(int proc_id, int i, int val){
    if(i < 0 || i > DATA_MEM_SIZE - 4){
        printf("Memory write error: invalid address %d\n", i);
        return;
    }
    
    for(int b = 0; b < 4; b++){
        int logical_addr = i + b;
        int page_index = logical_addr / PAGESIZE + 1024 / PAGESIZE;
        int frame = translate_page_alloc(proc_id, page_index);   /* TLB-aware + lazy alloc */
        int phys = frame * PAGESIZE + (logical_addr % PAGESIZE);
        unsigned char byte = (unsigned char)((val >> (8 * (3 - b))) & 0xFF);
        memory[phys] = (char)byte;
    }
}