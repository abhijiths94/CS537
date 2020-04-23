#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "types.h"
#include "fs.h"

#define stat xv6_stat
#include "stat.h"
#undef stat

int size;
int num_data_blocks;
int num_inodes;
int num_bitblocks;
int data_start;

typedef struct
{
    uint block;
    uint count;
} hash_t;

void* get_block(void* img , uint block)
{
    return (img + BSIZE*block);
}

int check_bit_map(void* img, int block)
{
    int bbyte = block/8;
    int bbit  = block%8;

    uchar* bit_block = (uchar*)(img + BSIZE*(3+num_inodes/IPB) + bbyte);
    
    if(*bit_block & 1<<bbit)
        return 1;
    else
        return 0;
}

void perr(char* msg)
{
    fprintf(stderr,"%s\n", msg);
    exit(1);
}

int main(int argc, char* argv[])
{
    struct stat fs_stat;
    struct superblock *sb;
    struct dinode *din;
    int i,j, k;
    struct dirent *de;

    if (argc == 1)
        perr("Usage: xfsck <file_system_image>");

    int fsfd;
    fsfd = open(argv[1], O_RDONLY);

    if (fsfd < 0)
        perr("image not found.");

    fstat(fsfd, &fs_stat);
    // map image to virtual memory
    void* img_ptr = mmap(NULL, fs_stat.st_size, PROT_READ, MAP_PRIVATE, fsfd, 0);
    
 //   printf("Size of image is : %ld\n",fs_stat.st_size);
    
    
    // read superblock info
    sb = (struct superblock *)(img_ptr + BSIZE);
    /*
    printf("Superblock size   : %d\n", sb->size); 
    printf("Superblock blocks : %d\n", sb->nblocks); 
    printf("Superblock inodes : %d\n", sb->ninodes);
*/
    size = sb->size;
    num_data_blocks = sb->nblocks;
    num_inodes = sb->ninodes;
    num_bitblocks = (num_data_blocks + (512*8) - 1)/(512*8);
    data_start = 2 + num_inodes/IPB + num_bitblocks + 1;

    hash_t* db_hash =(hash_t*) malloc(sizeof(hash_t)*size);

    //check #1 : Superblock corrupted ?
    if(sb->size < sb->nblocks + (sb->ninodes/IPB) + 1 + 1)
        perr("ERROR: superblock is corrupted.");

/*    
    for( i = 0 ; i < 3; i++)
    {
        din = (struct dinode*)(img_ptr + BSIZE*(IBLOCK(i)) + (i % IPB)*(sizeof(struct dinode)));
        printf("dinode addr  : %p\n", din);
        printf("dinode type  : %d\n", din->type);
        printf("dinode major : %d\n", din->major);
        printf("dinode minor : %d\n", din->minor);
        printf("dinode nlink : %d\n", din->nlink);
        printf("dinode size  : %d\n", din->size);
        
        for(j = 0 ; j <= NDIRECT; j++)
        {
            printf("dinode d_addr %d : %d\n",j, din->addrs[j]);
        }

        printf("\n");
    }
*/
    for(i = 0 ; i < sb->ninodes; i++)
    {
        int size_in_node = 0;
        int num_blocks_node = 0;

        din = (struct dinode*)(img_ptr + BSIZE*(IBLOCK(i)) + (i % IPB)*(sizeof(struct dinode)));
        
        //check #2 : Inode valid ?
        if(din->type != 0 && din->type != T_FILE && din->type != T_DIR && din->type != T_DEV)
        {    
            perr("ERROR: bad inode.");
        }
        
        if(din->type != 0)
        {
            for(j = 0; j <= NDIRECT; j++)
            {
                if(din->addrs[j] != 0)
                {
                    
                                                  
                    //check #3 : bad address ?
                    if(din->addrs[j] < ((sb->ninodes/IPB) + 3) || din->addrs[j] > sb->size)
                    {
                        if(j != NDIRECT)
                            perr("ERROR: bad direct address in inode.");
                        else
                            perr("ERROR: bad indirect address in inode.");

                    }
                    
                    if(j == NDIRECT)
                    {
                        //handle indirect pointers 
                        if(din->addrs[NDIRECT] != 0)
                        {
                             //TODO
                             //perr("ERROR: bad indirect address in inode.");
                        }
                    }
                    
                    //check #5 : 
                    if(!check_bit_map(img_ptr, din->addrs[j]))
                    {
                        perr("ERROR: address used by inode but marked free in bitmap.");
                    }

                    if(db_hash[din->addrs[j]].count == 0) 
                        db_hash[din->addrs[j]].count = 1;
                    else
                    {
                        perr("ERROR: direct address used more than once.");
                        // TODO : multiple refererences to same data block
                    }

                    if(j == NDIRECT)
                    {
                        uint* ind_addr = (uint*)(img_ptr + BSIZE*(din->addrs[j]));
                        for(k = 0; k < BSIZE/4; k++)
                        {
                            if(*(ind_addr + k) == 0)
                                continue; //TODO : change to break ??
                            else
                            {
                                num_blocks_node ++;
                                //printf("dinode indirect  : %d : %p\n", *(ind_addr + k), ind_addr+k);
                                if(db_hash[*(ind_addr + k)].count == 0)
                                    db_hash[*(ind_addr + k)].count = 1;
                                else
                                {
                                    /*
                                    printf("dinode type  : %d\n", din->type);
                                    printf("dinode nlink : %d\n", din->nlink);
                                    printf("dinode size  : %d\n", din->size);
                                    printf("dinode direct  : %d\n", din->addrs[0]);
                                    printf("dinode direct  : %d\n", din->addrs[1]);
                                    printf("dinode direct  : %d\n", din->addrs[2]);
                                    printf("dinode direct  : %d\n", din->addrs[3]);
                                    printf("dinode direct  : %d\n", din->addrs[4]);
                                    printf("dinode direct  : %d\n", din->addrs[5]);
                                    printf("dinode direct  : %d\n", din->addrs[6]);
                                    printf("dinode direct  : %d\n", din->addrs[7]);
                                    printf("dinode direct  : %d\n", din->addrs[8]);
                                    printf("dinode direct  : %d\n", din->addrs[9]);
                                    printf("dinode direct  : %d\n", din->addrs[10]);
                                    printf("dinode direct  : %d\n", din->addrs[11]);
                                    printf("dinode indirect head  : %d\n", din->addrs[j]);

                                    printf("%d %d %d\n", i, j, k);
                                    */
                                    perr("ERROR: indirect address used more than once.");
                                }
                            }
                        }
                    }
                    else
                    {
                        num_blocks_node ++;
                    }

                                   }
            }
            
            //check #4 : directory contains . and .. entries
            if(din->type == T_DIR)
            {
                /*
                printf("dinode addr  : %p\n", din);
                printf("dinode type  : %d\n", din->type);
                printf("dinode nlink : %d\n", din->nlink);
                printf("dinode size  : %d\n", din->size);
                */

                if(din->addrs[0] != 0)
                {
                    de = (struct dirent*)get_block(img_ptr, din->addrs[0]);
                    if(strcmp(de->name, ".") != 0 || strcmp((de+1)->name, "..") != 0 || de->inum != i)
                    {
                        perr("ERROR: directory not properly formatted.");
                    }
                }
                //printf("dinode addr  : %p\n", din);
                //printf("dinode type  : %d\n", din->type);
                //printf("dinode nlink : %d\n", din->nlink);
                //printf("dinode size  : %d\n", din->size);
                
                
                /*
                for(j = 0 ; j <= NDIRECT; j++)
                {
                    printf("dinode d_addr %d : %d\n",j, din->addrs[j]);
                    if(din->addrs[j] == 0)
                        break;
                }
                */
                
                /*de = (struct dirent*)get_block(img_ptr, 29);
                for(; de->inum != 0; de++)
                {
                    printf(" Dir inum : %d\n", de->inum );
                    printf(" Dir name : %s\n", de->name );
                    printf("------------------\n");
                }*/
                
            }
            
            //check #8: file size stored must be within the actual number of blocks 
            if(!( (num_blocks_node - 1)*BSIZE < (int)din->size && din->size <= num_blocks_node*BSIZE))
            {
                //printf("Inode no : %d\n",i);
                //printf("size = %d\n", din->size);
                //printf(" b = %d\n", num_blocks_node);
                perr("ERROR: incorrect file size in inode.");
            }

            
        }
        
        
    }

    
    for(i = data_start; i < size; i++)
    {
        if(check_bit_map(img_ptr, i))
        {
            if(db_hash[i].count == 0)
                perr("ERROR: bitmap marks block in use but it is not in use.");
        }
        
    }
    


    return 0;
}
