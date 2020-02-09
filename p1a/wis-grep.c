#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
    char * filename = NULL; 
    char *line_buffer = NULL;          // buffer to hold line
    size_t buff_size = 10000;
    FILE *fptr;
    int i = 2;

    /* Input error check */
    if(argc<2)
    {
        printf("wis-grep: searchterm [file …]\n");
        exit(1);
    }
    else if (argc == 2) //only search term
    {
        while(getline(&line_buffer, &buff_size, stdin)!= -1)
        {
            if(strstr(line_buffer, argv[1]) != NULL) 
                printf("%s", line_buffer); 
        }

    }
    else  //general form
    {
        
        while(i<argc)
        {
            filename = argv[i];
            fptr = fopen(filename, "r");
            if(fptr == NULL)
            {
                printf("wis-grep: cannot open file\n");
                exit(1);
            }

            while(getline(&line_buffer, &buff_size, fptr)!= -1)
            {
              
                if(strstr(line_buffer, argv[1]) != NULL) 
                    printf("%s", line_buffer); 
            }
            
            i++;
            fclose(fptr);
        }
        free(line_buffer);
    }
    return 0;
}
