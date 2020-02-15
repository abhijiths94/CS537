#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_PAR_INST 100
#define MAX_SEQ_INST 100
#define MAX_INST_SIZE 100
#define MAX_PATH_CNT 100
#define MAX_PATH_LEN 300
#define MAX_FILE_LEN 300


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

            //printf("Tokenize : %s : %s : %ld : %d  \n", delim, pch, strlen(pch), *cnt);
        }
        pch = strsep(&inp, delim);

    }
    //printf("No of %s inst = %d\n",delim, *cnt);
}

int check_system_cmd(int argc, char ** argv, char * path_str)
{
    // return values :
    // 0 : success
    // 1 : invalid 
    //
    
    int i, access_ret = -1;

    for(i = path_cnt - 1; i >= 0; i--)
    {
        strcpy(path_str, PATH[i]);
        strcat(path_str, "/");
        strcat(path_str, argv[0]);

        //printf("Searching in %s \n", path_str);
        access_ret  = access(path_str, X_OK);
        if(access_ret == 0)
        {
           // printf("Found !\n");
            break;
        }
    }
    return access_ret;
}

int check_cmd( char * inp, char ** argv, int * argc_p, char * path_str, char * redirect_file, int* redirect)
{
    // return values
    // -1 : error
    //  0 : successful
    //  1 : multiline command

    int ret = 0;
    int argc;
    *redirect = 0;
    char* pos = strchr(inp, '>');
    char * inp_cmd, *inp_file, *inp_file_trm ;

    /* check redirection syntax */
    if(pos != NULL)
    {
       // printf("Found redirect !!! \n");
        if(strchr(pos + 1 , '>') != NULL)
        {
            // 2 redirection in a single command
            return -1;
        }
        
        *redirect = 1;
        
        inp_cmd = (char *)malloc(strlen(inp));
        inp_file = (char *)malloc(strlen(inp));
        MEMCHECK(inp_cmd);
        MEMCHECK(inp_file);

        strncpy(inp_cmd, inp, (pos-inp));
        strcpy(inp_file, pos+1);
        inp_file_trm = trim_space(inp_file);

        /* redirect specific syntax check */
        tokenize(inp_cmd, argc_p, argv, " ");
        argc = *argc_p;
       
        if(strlen(inp_file_trm) == 0 || argc == 0 || (strchr(inp_file_trm, ' ') != NULL))
        {
            ret = -1;
            return ret;
        }

        ret = check_system_cmd(argc, argv, path_str); // call access function to check since it is not an inbuilt command
        //printf("Path_str after check_system_cmd : %s\n", path_str);
       
        strcpy(redirect_file, inp_file_trm);
        
        free(inp_cmd);
        free(inp_file);
        return ret;
    }
    else
    {
        tokenize(inp, argc_p, argv, " ");
        argc = *argc_p;
    
        /* No redirection */
        if (argc >= 1)
        {
            if(strcmp(argv[0], "exit") == 0)
            {
                if(argc != 1)
                {
                    ret = -1;
                }
            }
            else if(strcmp(argv[0], "cd") == 0)
            {
                if(argc != 2)
                {
                    ret = -1;
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
                    }
                    else if(strcmp(argv[1], "remove") == 0)
                    {
                        if(argc != 3)
                            ret = -1;
                    }
                    else if(strcmp(argv[1], "clear") == 0)
                    {
                        if(argc != 2)
                            ret = -1;
                    }
                    else
                    {
                        ret = -1;
                    }
                }
            }
            else
            {
                ret = check_system_cmd(argc, argv, path_str); // call access function to check since it is not an inbuilt command
               // printf("Path_str after check_system_cmd : %s\n", path_str);
            }
        }
    }
    return ret;
}


int execute_cmd(char ** argv, int argc, char * path_str, char * redirect_file, int redirect, int block)
{
    /*
    for(int i = 0; i < argc; i++)
        printf("Arg %d : %s\n", i, argv[i]);
        */

    //printf("path str : %s\n", path_str);

    int fork_ret, ret, i;
    int delete_indx = -1;
    int fd_out; 


    /* Check for inbuilt commands and then system cmnds */

    if(strcmp(argv[0], "exit") == 0)
    {
        exit_flag = true;
    }
    else if(strcmp(argv[0], "cd") == 0)
    {
        ret = chdir(argv[1]);
        return ret;
    }
    else if(strcmp(argv[0], "path") == 0)
    {
            if(strcmp(argv[1], "add") == 0)
            {
                    PATH[path_cnt] = (char*) malloc(strlen(argv[2]));
                    MEMCHECK(PATH[path_cnt]);
                    strcpy(PATH[path_cnt], argv[2]);
                    path_cnt ++;
            }
            else if(strcmp(argv[1], "remove") == 0)
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
            else if(strcmp(argv[1], "clear") == 0)
            {
                for(i = 0; i < path_cnt; i++)
                    free(PATH[i]);
                path_cnt = 0;
            }
    }
    else
    {  
        /* This is a system cmd */
        fork_ret = fork();
        
        argv[argc] = '\0';


        if(fork_ret == 0) // child process
        {
            if(redirect)
            {
               // printf("Printing in file ...\n");
                fd_out = open(redirect_file, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
                if(fd_out>0)
                {
                    dup2(fd_out, STDOUT_FILENO);
                    dup2(fd_out, STDERR_FILENO);
                }
                else
                {
                    throw_err();
                }
            }

            ret = execv(path_str, argv );
            //printf("!!!!!!!!! Execv ret : %d\n", ret);
            exit(0);
            (void) ret;
        }
        else
        {
            if(block)
            {
                // parent waits till finished executing
               // printf("**** Waiting for child to finish \n");
                waitpid(fork_ret, NULL, 0);
                //printf("child completed!!!! \n");
            }
        }
    }
    return fork_ret;
}



int shell(FILE* fp, int print_prompt )
{
    char * inp = NULL;
    size_t inp_len = 1024;
    int i = 0;
    int check_cmd_ret = 0;
    int is_multiline = 0;
    int execute_ret = 0;
    char ** args;
    int args_count;
    char *redirect_file;
    int redirect = 0;
    int j,k;

    int par_pid[100];
    int prev_par_pid_cnt = 0, par_pid_cnt = 0;


    
    inp = (char *)calloc(inp_len*sizeof(char), 1);
    
    char* inp_dup ;
    char * path_str = NULL;


    PATH = (char**)malloc(MAX_PATH_CNT * sizeof(char**));  
    args = (char**) malloc(MAX_INST_SIZE * sizeof(char**));

    path_str = (char*)calloc(MAX_PATH_LEN * sizeof(char) , 1);
    MEMCHECK(path_str);

    char ** seq_path_str = (char**)malloc(MAX_SEQ_INST * sizeof(char*));
    MEMCHECK(seq_path_str);

    char ** seq_inst = (char**) malloc(MAX_SEQ_INST * sizeof(char**));
    int seq_cnt = 0;
    MEMCHECK(seq_inst);

    char *** seq_args = (char***)malloc(MAX_SEQ_INST * sizeof(char**));
    MEMCHECK(seq_args);

    int * seq_args_count = (int*)malloc(MAX_SEQ_INST * sizeof(int));
    MEMCHECK(seq_args_count);

    char ** seq_redirect_file = (char**)malloc(MAX_SEQ_INST * sizeof(char*));
    MEMCHECK(seq_redirect_file);

    int * seq_redirect = (int*)malloc(MAX_SEQ_INST * sizeof(int));
    MEMCHECK(seq_redirect);

//////////////////////////////////////////////////////////////////////////////
//
//
    char * multi_mode = (char*)malloc(MAX_SEQ_INST * 20);
    MEMCHECK(multi_mode);



    char * par_dup = (char*)malloc(MAX_INST_SIZE);
    MEMCHECK(par_dup);

    int par_cnt;

    char ** par_inst =(char**) malloc(MAX_PAR_INST * sizeof(char**));
    MEMCHECK(par_inst);
    



    /* Memory allocation failed */
    if(inp == NULL || PATH == NULL)
    {
        exit(1);
    }
    
    /* Load default PATH */
    PATH[path_cnt] = strdup("/bin");
    MEMCHECK(PATH[path_cnt]);
    path_cnt ++;

    /* Redirect file */
    redirect_file = (char*)malloc(MAX_FILE_LEN);
    MEMCHECK(redirect_file);


    
    while(1)
    {
        if(exit_flag)
        {
            break;
        }

        if(print_prompt)
        {
            printf("smash> ");
            fflush(stdout);
        }


        if(getline(&inp, &inp_len, fp) == -1 || inp[0] == EOF)
        {
            exit(0);
        }

        memset(path_str, 0, strlen(path_str));
        k = 0;

        if(strlen(inp) > 1)
        {
            inp_dup = strdup(inp);
            is_multiline = 0; 
            if(strchr(inp, '&') != NULL || strchr(inp, ';') != NULL)
            {
                /* This is a possible multiline command */
                is_multiline = 1;
            }
            
            if(!is_multiline)
            {
                check_cmd_ret = check_cmd(inp_dup, args, &args_count, path_str, redirect_file, &redirect);
            }
            else
            {
                /* Split Multiline commands by sequential commands */
                tokenize(inp, &seq_cnt, seq_inst, ";");
               // printf("Multicommand detected : %d\n", seq_cnt);
               // printf(" Inp : %s \n ", inp);

                for(i = 0, j= 0; i < seq_cnt; i++, j++)
                {
                    if(strchr(seq_inst[j], '&') != NULL)
                    {
                        par_cnt = 0;
                        /* This is a parallel command */
                        strcpy(par_dup, seq_inst[j]);
                        tokenize(par_dup, &par_cnt, par_inst, "&");

                        for(k= i; k < i + par_cnt; k++)
                        {
                            
                            //printf("-------------------------> Par inst %d : %s\n", k, par_inst[k-i]);
                            seq_args[k] = (char**) malloc(MAX_INST_SIZE * sizeof(char**));
                            MEMCHECK(seq_args[k]);

                            seq_path_str[k] = (char*)calloc(MAX_PATH_LEN * sizeof(char) , 1);
                            MEMCHECK(seq_path_str[k]);

                            seq_redirect_file[k] = (char*)malloc(MAX_FILE_LEN);
                            MEMCHECK(seq_redirect_file[k]);

                            check_cmd_ret = check_cmd(par_inst[k-i], seq_args[k], &seq_args_count[k], seq_path_str[k], seq_redirect_file[k], &seq_redirect[k]);
                            multi_mode[k] = 0;
                            if(check_cmd_ret == -1)
                            {
                                break;
                            }
                        }

                        if(check_cmd_ret == -1)
                        {
                            throw_err();
                            break;
                        }
                        
                        /* Update counters */
                        i = k -1;
                        seq_cnt += par_cnt -1; 
                    }
                    else
                    {
                        seq_args[i] = (char**) malloc(MAX_INST_SIZE * sizeof(char**));
                        MEMCHECK(seq_args[i]);

                        seq_path_str[i] = (char*)calloc(MAX_PATH_LEN * sizeof(char) , 1);
                        MEMCHECK(seq_path_str[i]);

                        seq_redirect_file[i] = (char*)malloc(MAX_FILE_LEN);
                        MEMCHECK(seq_redirect_file[i]);
                
                        //printf("-------------------------> Seq inst %d : %s\n", i, seq_inst[j]);
                        check_cmd_ret = check_cmd(seq_inst[j], seq_args[i], &seq_args_count[i], seq_path_str[i], seq_redirect_file[i], &seq_redirect[i]);
                        multi_mode[i] = 1; 
                        if(check_cmd_ret == -1)
                        {
                            throw_err();
                            break;
                        }
                    }
                }
            }

            if(!is_multiline)
            {
                /* Execute if syntax is correct */
                if(check_cmd_ret == -1)
                {
                    //Invalid command throw error
                    throw_err();
                }
                else if(check_cmd_ret == 0)
                {
                    //printf("Command success... Path count : %d\n", path_cnt);

                    execute_ret = execute_cmd(args, args_count, path_str, redirect_file, redirect, 1);
                     if(execute_ret<0)
                     {
                         throw_err();
                        // break;
                     }
                }
            }
            else
            {
                //printf("\nStarting execution: count = %d !!!!! \n\n", seq_cnt);
                if(check_cmd_ret != -1)
                {
                    //printf("-------------------------------------------------\n");
                     //printf("\n\nParsed seq inst :\n ");
                    //int j;

                    /*
                    for(i = 0; i < seq_cnt; i++)
                    {
                        for(j =0 ; j< seq_args_count[i]; j++)
                        {
                            printf("\t%s  %d \n", seq_args[i][j], multi_mode[i]);
                        }
                    }
                    */

                    //printf("-------------------------------------------------\n");
                    for(i = 0; i < seq_cnt; i++)
                    {
                      //  printf("Executing inst : %d\n",  i);

                        execute_ret = execute_cmd(seq_args[i], seq_args_count[i], seq_path_str[i], seq_redirect_file[i], seq_redirect[i], multi_mode[i]);

                        prev_par_pid_cnt = par_pid_cnt;

                        if(multi_mode[i] == 0) // parallel ops 
                        {
                            par_pid[par_pid_cnt++] = execute_ret;
                        }
                        else
                        {
                            par_pid_cnt = 0;
                        }

                        if(multi_mode[i] == 0)
                        {
                            
                                waitpid(execute_ret,  NULL, 0);
                        }

                        ///printf("prev : %d , cur : %d\n", prev_par_pid_cnt, par_pid_cnt);

                        if((prev_par_pid_cnt == 1 && par_pid_cnt == 0) || (i == seq_cnt - 1 && multi_mode[i] == 1)) 
                        {
                            for(k = 0; k < par_pid_cnt; k++)
                            {
                               // printf("waiting for pid ...\n");
                                waitpid(par_pid[k], NULL, 0);
                            }
                            //:wq!
                            (void)par_pid;
                        }



                    }
                }

                
                for(i = 0; i < seq_cnt; i++)
                {
                    if(seq_args[i] != NULL)
                        free(seq_args[i]);
                    if(seq_path_str[i]!= NULL)
                        free(seq_path_str[i]);
                    if(seq_redirect_file[i]!= NULL)
                        free(seq_redirect_file[i]);
                }
            }
            free(inp_dup);
        }
    }


    /* Free memory allocation */
    free(seq_args);
    free(seq_args_count);
    free(seq_redirect_file);
    free(seq_redirect);
    free(seq_args);
    free(seq_path_str);
    free(seq_redirect_file);
    free(redirect_file);
    free(path_str);
    for(i = 0; i < args_count; i++)
    {
        free(args[i]);
    }
    free(args);

    for(i = 0 ; i < path_cnt; i++)
    {
        free(PATH[i]);
    }
    free(PATH);
    for(i = 0 ; i < seq_cnt; i++)
    {
        free(seq_inst[i]);
    }
    free(inp);
    free(seq_inst);
    return 0;
}


int main(int argc, char** argv)
{

    FILE* fp ; 

    if(argc == 2)
    {
        /* Batch mode */
        fp = fopen(argv[1], "r");
        if(fp == NULL)
        {
            throw_err();
        }
        shell(fp, 0);
    }
    else if(argc > 2)
    {
        throw_err();
        exit(1);
    }
    else
    {
        shell(stdin, 1);
    }


}
