#include "compiler.h"
#include "memory.h"
#include "processor.h"
#include "os.h"
#include <stdio.h>

int main(void){
    os_init();

    printf("Mini-computer OS starting (1 processor)...\n");
    printf("Type a program filename (e.g. prog1.txt) and press Enter\n");
    printf("Type 'exit' to stop accepting new programs; already-running tasks finish first.\n\n");

    while(shell_is_active() || os_active_task_count() > 0){
        scheduler();
    }

    printf("\nAll tasks complete, shell closed. End of simulation.\n");
    return 0;
}
