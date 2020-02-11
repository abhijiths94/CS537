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
#define MAX_INST_SIZE 100
#define MAX_PATH_CNT 100
#define MAX_PATH_LEN 200

#define MEMCHECK(ptr) if(ptr == NULL) exit(1);


volatile bool exit_flag = false;
char ** PATH = NULL;
int path_cnt = 0;

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

            printf("Tokenize : %s : %s : %ld : %d  \n", delim, pch, strlen(pch), *cnt);
        }
        pch = strsep(&inp, delim);

    }
    printf("No of %s inst = %d\n",delim, *cnt);
}

int execute_inst(char *inst)
{
    int argc = 0;
    char ** argv  = (char**) malloc(MAX_INST_SIZE * sizeof(char**));
    tokenize(inst, &argc, argv, " ");

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
        execute_inst(seq_inst[i]);
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

int handle_inbuilt_cmd( char * inp)
{
    // return values
    // -1 : error
    //  0 : successful
    //  1 : multiline command

    int ret = 0;
    int i, argc = 0;
    char ** argv  = NULL;
    int delete_indx = -1;
    if(
            strchr(inp, '&') != NULL||
            strchr(inp, ';') != NULL
      )
    {
        /* This is a possible multiline command */
        ret = 1;
    }
    else
    {
        argv = (char**) malloc(MAX_INST_SIZE * sizeof(char**));
        tokenize(inp, &argc, argv, " ");
        if (argc >= 1)
        {
            if(strcmp(argv[0], "exit") == 0)
            {
                if(argc != 1)
                {
                    ret = -1;
                }
                else
                {
                    exit_flag = true;
                }
            }
            else if(strcmp(argv[0], "cd") == 0)
            {
                if(argc != 2)
                {
                    ret = -1;
                }
                else
                {
                    ret = chdir(argv[1]);
                }
            }
            else if(strcmp(argv[0], "path") == 0)
            {
                if(argc < 2)
                {
                    ret = -1;
                }
                else
                {
                    if(strcmp(argv[1], "add") == 0)
                    {
                        if(argc != 3)
                            ret = -1;
                        else
                        {
                            PATH[path_cnt] = (char*) malloc(strlen(argv[2]));
                            MEMCHECK(PATH[path_cnt]);
                            strcpy(PATH[path_cnt], argv[2]);
                            path_cnt ++;
                        }
                    }
                    else if(strcmp(argv[1], "remove") == 0)
                    {
                        if(argc != 3)
                            ret = -1;
                        else
                        {
                            delete_indx = -1;

                            for(i = path_cnt -1 ; i >= 0; i--)
                            {
                                if(strcmp(PATH[i], argv[2]) == 0)
                                {
                                    delete_indx = i;
                                }

                                if( delete_indx >= 0)
                                {
                                    /* Remove entry at delete_indx and shift elements */
                                    free(PATH[delete_indx]);
                                    PATH[delete_indx] = NULL;
                                    for(i = delete_indx; i < path_cnt-1; i++)
                                    {
                                        PATH[i] = PATH[i+1];
                                    }
                                    path_cnt --;
                                    break;
                                }
                            }
                        }
                    }
                    else if(strcmp(argv[1], "clear") == 0)
                    {
                        if(argc != 2)
                            ret = -1;
                        else
                        {
                            for(i = 0; i < path_cnt; i++)
                                free(PATH[i]);
                            path_cnt = 0;
                        }
                    }
                    else
                    {
                        ret = -1;
                    }
                }
            }
            else
            {
                ret = 1;
            }
        }
        /* Free up memory */
        for(i = 0; i < argc; i++)
        {
            free(argv[i]);
        }
        free(argv);
    }
    return ret;
}


int main(int argc, char** argv)
{
    char * inp = NULL;
    size_t inp_len = 1024;
    int parallel_cnt = 0;
    int i = 0;
    int is_multi_cmd = 0;
    
    inp = (char *)calloc(inp_len*sizeof(char), 1);
    
    char** par_inst = (char **)malloc(MAX_PAR_INST * sizeof(char**));
    char* inp_dup ;
    
    PATH = (char**)malloc(MAX_PATH_CNT * sizeof(char**)); 
    
        /* Memory allocation failed */
    if(inp == NULL || par_inst == NULL || PATH == NULL)
    {
        exit(1);
    }
    

    /* Load default PATH */
    PATH[path_cnt] = strdup("/bin");
    MEMCHECK(PATH[path_cnt]);
    path_cnt ++;
    
    while(1)
    {

        if(exit_flag)
        {
            break;
        }
        printf("smash> ");
        getline(&inp, &inp_len, stdin);

        
        if(strlen(inp) > 1)
        {

            inp_dup = strdup(inp);
            is_multi_cmd = handle_inbuilt_cmd(inp);

            if(is_multi_cmd== 1)
            {
                printf("--------------------------------------------------------\n\n");
                tokenize(inp_dup, &parallel_cnt, par_inst, "&");
                
                printf("Multicommand detected : %d\n", parallel_cnt);
                printf(" Inp : %s \n ", inp_dup);

                for(i = 0; i < parallel_cnt; i++)
                {
                    printf("len : %lu %s\n",strlen(par_inst[i]), par_inst[i]);
                }
                execute_par_inst(par_inst, parallel_cnt);
            }
            else if(is_multi_cmd == -1)
            {
                //Invalid command throw error
                throw_err();
            }
            else if(is_multi_cmd == 0)
            {
                printf("Command success... Path count : %d\n", path_cnt);
                for(i = 0; i < path_cnt; i++)
                {
                    printf("%s\n", PATH[i]);
                }
            }

            free(inp_dup);
        }
    }

    /* Free memory allocation */
    
    for(i = 0 ; i < path_cnt; i++)
    {
        free(PATH[i]);
    }
    free(PATH);
    for(i = 0 ; i < parallel_cnt; i++)
    {
        free(par_inst[i]);
    }
    free(inp);
    free(par_inst);
    return 0;
}
