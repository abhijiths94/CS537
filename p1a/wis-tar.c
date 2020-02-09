#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    char * src_file = NULL;
    char * tarfile = NULL;
    char *line_buffer = NULL;          // buffer to hold line
    size_t buff_size = 10000;
    FILE * src_fp, *tar_fp; 
    int i = 2;
    int err;                            //store error code
    
    struct stat info;
    char * filename_buf = NULL;
    int filename_size = 100;
    int src_file_len = 0;

    filename_buf = (char*)malloc(filename_size);

    if(filename_buf == NULL)
    {
        printf("Memory not allocated\n");
        exit(1);
    }

    

    /* Input error check */
    if(argc<3)
    {
        printf("wis-tar: tar-file file […]\n");
        exit(1);
    }
    else  
    {
        tarfile = argv[1];
        tar_fp = fopen(tarfile, "w");
        
        while(i<argc)
        {
            src_file = argv[i];
            src_file_len = strlen(src_file);

            memset(filename_buf, '\0', filename_size);
            err = stat(src_file, &info);
            src_fp = fopen(src_file, "r");
            
            if(src_fp == NULL || err != 0)
            {
                printf("wis-tar: cannot open file\n");
                exit(1);
            }

            if(src_file_len > 100)
                filename_size = 100;
            else
                filename_size = src_file_len;

            memcpy(filename_buf, src_file, filename_size);

//            printf("Size of file %s is %ld\n",filename_buf, info.st_size);
            fwrite(filename_buf, 1, 100, tar_fp);

            /*
            for(err = 0; err <100; err++)
            {
                if(filename_buf[err] == '\0')
                    printf("*");
                else
                    printf("%c", filename_buf[err]);
            }
            printf("\n");
            */

            fwrite(&info.st_size, sizeof(long), 1, tar_fp);
            
            while(getline(&line_buffer, &buff_size, src_fp)!= -1)
            {
              
                fprintf(tar_fp, "%s", line_buffer); 
            }
            i++;
            fclose(src_fp);
        }
        fclose(tar_fp);
//        free(line_buffer);

    }
    return 0;
}
