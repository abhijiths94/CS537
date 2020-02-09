#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int min(int a , int b)
{
    return a < b ? a : b;
}

int main(int argc, char** argv)
{
    char * src_file = NULL;
    char * tarfile = NULL;
    FILE * src_fp, *tar_fp; 
    
    char * filename_buf = NULL;
    char * file_buf = NULL;
    int filename_size = 100;
    long src_file_len = 0;

    int ret;

    filename_buf = (char*)malloc(filename_size);
    src_file = (char*)malloc(100);
    file_buf = (char*)malloc(1000);

    
    if(filename_buf == NULL || src_file == NULL || file_buf == NULL)
    {
        printf("Memory not allocated\n");
        exit(1);
    }

    

    /* Input error check */
    if(argc != 2)
    {
        printf("wis-untar: tar-file\n");
        exit(1);
    }
    else  
    {
        tarfile = argv[1];
        tar_fp = fopen(tarfile, "r");
       
        if(tar_fp == NULL)
        {
             printf("wis-untar: cannot open file\n");
             exit(1);
        }

        while(fread(filename_buf, 1, 100, tar_fp ))
        {
            strcpy(src_file, filename_buf);
            src_file_len = 0;
            
            ret = fread(&src_file_len, 8, 1, tar_fp );
            if(ret == 0)
            {
                printf("wis-untar: unable to write to file\n");
                exit(1);
            }
            
            src_fp = fopen(src_file, "w");
            if(src_fp == NULL)
            {
                printf("wis-untar: cannot create file\n");
                exit(1);
            }
            
            memset(filename_buf, '\0', filename_size);
            
            while(src_file_len > 0 && fread(file_buf, 1, min(src_file_len, 1000), tar_fp))
            {
                ret = fwrite(file_buf, 1, min(src_file_len, 1000), src_fp);
                if(ret == 0)
                {
                    printf("wis-untar: unable to write to file\n");
                    exit(1);
                }
                src_file_len -= 1000;
            }

            fclose(src_fp);
        }
        fclose(tar_fp);

    }
    return 0;
}
