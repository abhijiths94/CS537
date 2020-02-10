#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>

#define MAX_PAR_INST 100
#define MAX_SEQ_INST 100
#define MAX_INST_SIZE 500

#define MEMCHECK(ptr) if(ptr == NULL) exit(1);


bool exit_flag = false;



void throw_err()
{
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message)); 
}

char* trim_space(char *str)
{
    while(isspace(*str)) // Trim leading space
    {
        str++;
    }

    long len = strlen(str);
    len --;

    while(*(str+len) == '\n' || *(str+len) == ' ')
    {
        *(str + len) = '\0';
    }
    return str;
}

void tokenize(char *inp, int * cnt, char** inst, char * delim)
{
    char * pch = NULL; 
    
    *cnt = 0;

    pch = strsep(&inp, delim);
    while(pch != NULL)
    {
        pch = trim_space(pch);
        if(strlen(pch))
        {

            *cnt = *cnt + 1;
            inst[*cnt-1] = (char *) malloc(strlen(pch));
            MEMCHECK(inst[*cnt-1]); 
            strcpy(inst[*cnt -1], pch);
        }
        pch = strsep(&inp, delim);

    }
    printf("No of parallel inst = %d\n", *cnt);
}

int execute_inst(char *inst, int len)
{
  // tokenize(inst);
  exit(0);
}

int execute_seq_inst(char * inst)
{
    char ** seq_inst = (char**) malloc(MAX_SEQ_INST * sizeof(char**));
    int seq_cnt = 0;
    int i ;

    tokenize(inst , &seq_cnt, seq_inst, ";");
    
    for(i = 0; i < seq_cnt; i++)
    {
        //execute_inst();
        printf("\tlen : %lu Executing: %s\n",strlen(seq_inst[i]), seq_inst[i]);
    }


    exit(0);
}

int execute_par_inst(char **inst, int count )
{
    int i, waitrc;
    int pid[100];

    printf("Creating %d threads ... \n", count);
    for(i = 0; i < count; i++)
    {
        
        pid[i] = fork();
        if(pid[i] == 0)
        {
            //spawn out child and it will call execv
            printf("Process %d ..\n", i);
            sleep(i);
            execute_seq_inst(inst[i]);

            printf("Error !!!!! Reached point for process %d\n", i);
        }
        else if (pid[i] < 0)
        {
            printf(" !!!!!!!!! Error on %d PID \n", i);
        }
    }
    
    waitrc = 0;

    //parent waits for children to finish
    for(i = 0; i < count; i++)
    {
        waitrc = waitpid(pid[i], NULL, 0);
        printf("------------------------------------- main waitrc %d : %d\n ", i, waitrc);
    }

    printf("All threads completed \n");
    return waitrc;
}

int main(int argc, char** argv)
{
    char * inp = NULL;
    size_t inp_len = 1024;
    int parallel_cnt = 0;
    int i = 0;

    inp = (char *)calloc(inp_len*sizeof(char), 1);
    
    char** par_inst = (char **)malloc(MAX_PAR_INST * sizeof(char**));


    /* Memory allocation failed */
    if(inp == NULL || par_inst == NULL)
    {
        exit(1);
    }

    
    while(1)
    {

        if(exit_flag)
        {
            break;
        }
        printf("smash> ");
        getline(&inp, &inp_len, stdin);
        
        if(strncmp("exit", inp, 4)==0)
        {
            for(i = 0 ; i < parallel_cnt; i++)
            {
                free(par_inst[i]);
            }
            free(inp);
            free(par_inst);
            
            return 0;
        }

        if(strlen(inp) > 1)
        {
            tokenize(inp, &parallel_cnt, par_inst, "&");
            for(i = 0; i < parallel_cnt; i++)
            {
                printf("len : %lu %s\n",strlen(par_inst[i]), par_inst[i]);
            }
            execute_par_inst(par_inst, parallel_cnt);
        }
    }

    free(inp);
    free(par_inst);
    return 0;
}
