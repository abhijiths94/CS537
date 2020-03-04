/* any write in the proctected area should be killed  */
#include "types.h"
#include "stat.h"
#include "user.h"


#define PGSIZE 4096
int main() {
    const uint numframes = 100;
    sbrk(numframes * PGSIZE);
    int frames[numframes];

    int k;

    if (dump_allocated(frames, numframes) != 0) {
        printf(1, "Error: dump_allocated return non-zero value");
        printf(1, "TEST FAILED\n");
        exit();
    }

    for (int i = 0; i< numframes; i++) {
        for (int j = 0; j < numframes; j++) {
            if (i != j && (frames[j] == frames[i] 
                || frames[j] == frames[i] + PGSIZE || frames[j] == frames[i] - PGSIZE)) {
                for(k = 0; k < numframes; k++)
                {
                    printf(1,"%x ", frames[k]);
                }
                printf(1, "\nTEST FAILED: %d %d \n", i,j );
                exit();
            }
        }
    } 

    printf(1, "TEST PASSED\n");
    exit();
}
