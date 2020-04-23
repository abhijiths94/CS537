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

uint size;
uint num_data_blocks;
uint num_inodes;
uint num_bitblocks;
uint data_start;

typedef struct
{
    uint block;
    uint count;
} hash_t;

typedef struct
{
    uint is_dir;
    uint count;
}dir_count_t;

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
    int i,j, k,l;
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
    
    
    // read superblock info
    sb = (struct superblock *)(img_ptr + BSIZE);

    //printf("Superblock size   : %d\n", sb->size); 
    //printf("Superblock blocks : %d\n", sb->nblocks); 
    //printf("Superblock inodes : %d\n", sb->ninodes);

    size = sb->size;
    num_data_blocks = sb->nblocks;
    num_inodes = sb->ninodes;
    num_bitblocks = (num_data_blocks + (512*8) - 1)/(512*8);
    data_start = 2 + num_inodes/IPB + num_bitblocks + 1;

    hash_t* db_hash =(hash_t*) malloc(sizeof(hash_t)*size);
    uint * link_cnt = (uint*) malloc(sizeof(uint)*num_inodes);
    dir_count_t* dir_cnt = (dir_count_t*)(malloc(sizeof(dir_count_t)*num_inodes));

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
        //int size_in_node = 0;
        int num_blocks_node = 0;
        din = (struct dinode*)(img_ptr + BSIZE*(IBLOCK(i)) + (i % IPB)*(sizeof(struct dinode)));
        
        //printf("Inode : %d\n",i);
        //printf("Inode size : %d\n",din->size);
        //printf("Inode type : %d\n",din->type);
        //printf("------------------\n");

        //check #2 : Inode valid ?
        if(din->type != 0 && din->type != T_FILE && din->type != T_DIR && din->type != T_DEV)
        {    
            perr("ERROR: bad inode.");
        }
        
        if(din->type != 0)
        {
            /* Valid Indode */


            //printf("indoe num %d\n", i);
            //printf("dinode type  : %d\n", din->type);
            //printf("dinode nlink : %d\n", din->nlink);
            //printf("dinode size  : %d\n\n", din->size);

            if(din->nlink == 0)
            {
                perr("ERROR: inode marked used but not found in a directory.");
            }

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

                                if((uint)(*(ind_addr+k)) < (uint)(num_inodes/IPB + num_bitblocks + 3) ||(uint)(*(ind_addr+k)) >(uint)(sb->size))
                                {
                                    perr("ERROR: bad indirect address in inode.");
                                }
                                
                                //check #5 : 
                                if(!check_bit_map(img_ptr, *(ind_addr + k)))
                                {
                                    perr("ERROR: address used by inode but marked free in bitmap.");
                                }

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
                
                printf("---------------------------\n");
                printf("dinode : %d\n", i);
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
                k = 0;
                while(din->addrs[k] != 0)
                {
                    if(k==NDIRECT)
                    {
                        //TODO : traverse indirect nodes to get directories
                        l = 0;
                        uint* ind_de = (uint*)(get_block(img_ptr, din->addrs[NDIRECT]));
                        while(*(ind_de+l) != 0)
                        {
                            
                            //int parent = 0, current = 0;
                            uint m;
                            de = (struct dirent*)get_block(img_ptr, *(ind_de+l));
                            for(m=0; m < BSIZE/sizeof(struct dirent); m++ )
                            {
                                struct dinode* din_tmp = (struct dinode*)(img_ptr + BSIZE*(IBLOCK((de+m)->inum)) + ((de+m)->inum % IPB)*(sizeof(struct dinode)));
                                if((de+m)->inum == 0)
                                    continue;
                                if(din_tmp->type == 0)
                                {
                                    //printf("k : %d , l : %d\n", k,l);
                                    //printf("de->name : %s\n", (de+l)->name);
                                    //printf("de->inum : %d\n", (de+l)->inum);
                                    perr("ERROR: inode referred to in directory but marked free.");
                                }
                                link_cnt[(de+m)->inum]++;
                                
                                if(!(strcmp((de+m)->name, ".") == 0 || strcmp((de+m)->name, "..") == 0))
                                {
                                    //if not current dir or parent direct entry
                                    dir_cnt[(de+m)->inum].is_dir = 1;
                                    if(dir_cnt[(de+m)->inum].count > 0)
                                    {
                                        perr("ERROR: directory appears more than once in file system.");
                                    }
                                    else
                                    {
                                         dir_cnt[(de+m)->inum].count = 1;
                                    }

                                }

                            }
                            l++;
                        }
                        break;
                    }
                    else
                    {

                        //int parent = 0, current = 0;
                        de = (struct dirent*)get_block(img_ptr, din->addrs[k]);
                        for(l = 0; l < BSIZE/sizeof(struct dirent); l++)
                        {
                           //printf("l : %d\n",l);
                           struct dinode* din_tmp = (struct dinode*)(img_ptr + BSIZE*(IBLOCK((de+l)->inum)) + ((de+l)->inum % IPB)*(sizeof(struct dinode)));
                           if((de+l)->inum == 0)
                               continue;
                           if(din_tmp->type == 0)
                           {
                               //printf("k : %d , l : %d\n", k,l);
                               //printf("de->name : %s\n", (de+l)->name);
                               //printf("de->inum : %d\n", (de+l)->inum);
                               perr("ERROR: inode referred to in directory but marked free.");
                           }
                           //add to link_cnt
                           link_cnt[(de+l)->inum]++;

                           if(strcmp((de+l)->name, ".") == 0)
                           {
                               //current = (de+l)->inum;
                               //printf("curr set to %d\n", (de+l)->inum);
                           }
                           else if(strcmp((de+l)->name, "..") == 0)
                           {
                               //parent = (de+l)->inum;
                               //printf("parent set to %d\n", (de+l)->inum);
                           }
                           else
                           {
                               //if(din_tmp->type == T_DIR)
                                    //printf(" p : %d , cur : %d  = %d\n", parent, current,(de+l)->inum);
                               //if not current dir or parent direct entry
                               dir_cnt[(de+l)->inum].is_dir = 1;
                               if(dir_cnt[(de+l)->inum].count == 1)
                               {
                                   perr("ERROR: directory appears more than once in file system.");
                               }
                               else
                               {
                                    dir_cnt[(de+l)->inum].count = 1;
                               }

                           }
                        }
                    }
                    k++;
                }
                
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
    
    //check #11: nlink tally check
    for(i = 0; i < num_inodes; i++)
    {
        din = (struct dinode*)(img_ptr + BSIZE*(IBLOCK(i)) + (i % IPB)*(sizeof(struct dinode)));
        if(din->type == T_FILE)
        {
            if(din->nlink != link_cnt[i])
            {
                //printf("Inode no : %d\n",i);
                //printf("size = %d\n", din->size);
                //printf("nlinks = %d\n", din->nlink);
                //printf("link_cnt = %d\n\n", link_cnt[i]);
                perr("ERROR: bad reference count for file.");
            }
        }
    }


    return 0;
}
