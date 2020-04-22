#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <assert.h>

#include "mapreduce.h"

#define DEBUG_PRINT 0u

#if DEBUG_PRINT
#define DPRINTF(...) if (DEBUG_PRINT) { printf(__VA_ARGS__);}
#else
#define DPRINTF(...) 
#endif

/*********************************** DATA STRUCTS *****************************/

typedef struct  
{    
   pthread_t thread_id;        /* ID returned by pthread_create() */
   int       thread_num;       /* Application-defined thread # */
}thread_info_t;

typedef struct node
{
    char * key;
    char * value;
    struct node* next;
}node_t;

typedef struct table
{
    int size;
    struct node **list;
}table_t;

/*********************************** GL VARS  *****************************/

/* Store filenames passed */
char** filenames;
/* Number of filenames passed */
int num_files = 0;
int count_files = 0;
int gl_num_mappers = 0;
int gl_num_reducers = 0;

/* Mutex lock */
pthread_mutex_t lock;

/* Thread info structure */
thread_info_t *th_info;

char** combiner_cur_key;
node_t** map_to_combine;
node_t** combine_getter_ptr;

node_t** combine_to_reducer;
char** reducer_cur_key;
node_t** reduce_getter_ptr;

/*********************************** FUNCTIONS *****************************/

/* Global map function */
Mapper map_fn;
/* Global Combine function */
Combiner combine_fn;
/* Global reduce function */
Reducer reduce_fn;
/* Global Partition fn */
Partitioner partition_fn;

/*********************************** FN DEFN *****************************/

struct table *createTable(int size){
    struct table *t = (struct table*)malloc(sizeof(struct table));
    t->size = size;
    t->list = (struct node**)malloc(sizeof(struct node*)*size);
    int i;
    for(i=0;i<size;i++)
        t->list[i] = NULL;
    return t;
};

int get_thread_num(pthread_t thrd)
{
    int i;
    for(i = 0; i < gl_num_mappers; i++)
    {
        if(th_info[i].thread_id == thrd)
        {
            return th_info[i].thread_num;
        }
    }
    assert(0);
    return -1;
}

int red_get_thread_num(pthread_t thrd)
{
    int i;
    for(i = 0; i < gl_num_reducers; i++)
    {
        if(th_info[i].thread_id == thrd)
        {
            return th_info[i].thread_num;
        }
    }
    assert(0);
    return -1;
}

void delete_list(node_t ** head_ref)
{
   node_t* current = *head_ref;
   node_t* next;

   while (current != NULL)
   {
       next = current->next;
       free(current->key);
       free(current->value);
       free(current);
       current = next;
   }
   *head_ref = NULL;
}

unsigned long MR_DefaultHashPartition(char *key, int num_partitions) 
{
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_partitions;
}

char* reduce_get_next(char * key, int part_no)
{
    char * ret;
    
    if(reduce_getter_ptr[part_no] == NULL)
    {
        /* End of list */
        ret = NULL;
        reducer_cur_key[part_no] = NULL;
    }
    else
    {
        if(strcmp(key, reduce_getter_ptr[part_no]->key) == 0)
        {
            ret = reduce_getter_ptr[part_no]->value; 
            reduce_getter_ptr[part_no] = reduce_getter_ptr[part_no]->next;
        }
        else
        {
            ret = NULL;
            reducer_cur_key[part_no] = reduce_getter_ptr[part_no]->key;
        }
    }
    return ret;
}


char* combine_get_next(char * key)
{
    int t_no = get_thread_num(pthread_self());
    char * ret;

    if(combine_getter_ptr[t_no] == NULL)
    {
        /* End of list */
        ret = NULL;
        combiner_cur_key[t_no] = NULL;
    }
    else
    {
        if(strcmp(key, combine_getter_ptr[t_no]->key) == 0)
        {
            ret = combine_getter_ptr[t_no]->value; 
            combine_getter_ptr[t_no] = combine_getter_ptr[t_no]->next;
        }
        else
        {
            ret = NULL;
            combiner_cur_key[t_no] = combine_getter_ptr[t_no]->key;
        }
    }
    return ret;
}

void sortedInsert(node_t** head_ref, node_t* new_node)
{
    node_t* current;

    
    /* Special case for the head end */
    if (*head_ref == NULL || strcmp((*head_ref)->key, new_node->key)>0)
    {
        new_node->next = *head_ref;
        *head_ref = new_node;
    }
    else
    {
        /* Locate the node before the point of insertion */
        current = *head_ref;
        while (current->next!=NULL &&
            strcmp(current->next->key ,new_node->key)<0)
        {
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
    }
}

void MR_EmitToCombiner(char *key, char *value)
{
    int thread_num = get_thread_num(pthread_self());
    
    char* key_p = strdup(key);
    char* val_p = strdup(value);

    node_t* node = (node_t*)malloc(sizeof(node_t));
    node->key = key_p;
    node->value = val_p;
    node->next = NULL;

    DPRINTF("Appending key : %s , val : %s\n", key, value);
    sortedInsert(&map_to_combine[thread_num], node);
}

void MR_EmitToReducer(char *key, char *value)
{
    unsigned long part_no ;

    char* key_p = strdup(key);
    char* val_p = strdup(value);

    part_no = (*partition_fn)(key_p, gl_num_reducers);

    node_t* node = (node_t*)malloc(sizeof(node_t));
    node->key = key_p;
    node->value = val_p;
    node->next = NULL;

    DPRINTF("ER : Appending key : %s , val : %s\n", key, value);
    sortedInsert(&combine_to_reducer[part_no], node);

}


void* mapper_thread(void* args)
{ 
    char* thread_filename;
    thread_info_t * tinfo = args;

    map_to_combine[tinfo->thread_num] = NULL;
    
    pthread_mutex_lock(&lock); 
    while(count_files < num_files)
    {
        thread_filename = filenames[count_files++];
        pthread_mutex_unlock(&lock);
        
        /* Call map */
        DPRINTF("tid : %d: %s\n",get_thread_num(tinfo->thread_id), thread_filename);
        (*map_fn)(thread_filename);

#if DEBUG_PRINT
        node_t* n = map_to_combine[tinfo->thread_num];
        while(n != NULL)
        {
            DPRINTF("%s : %s\n", n->key, n->value);
            n = n->next;
        }
#endif        
                /* TODO : Delete all structures */
    }
    pthread_mutex_unlock(&lock);
    
    /* Start combine */

    DPRINTF("Thread %d : Calling combine \n", tinfo->thread_num);
  
    if(combine_fn != NULL && map_to_combine[tinfo->thread_num] != NULL)
    {
        combiner_cur_key[tinfo->thread_num] = map_to_combine[tinfo->thread_num]->key;
        combine_getter_ptr[tinfo->thread_num] = map_to_combine[tinfo->thread_num];


        while(combiner_cur_key[tinfo->thread_num] != NULL)
        {
            (*combine_fn)(combiner_cur_key[tinfo->thread_num], combine_get_next);
        }
    }
    DPRINTF("Thread %d : Finished combine\n", tinfo->thread_num);

    /* Free all memory used between map and combine */
    delete_list(&map_to_combine[tinfo->thread_num]);
    return NULL;
}

void* reducer_thread(void* args)
{
    int t_no = red_get_thread_num(pthread_self());

    /* call reducer fn */
    DPRINTF("Thread %d : Starting reduction\n", t_no);

    /*loop across all keys in each partition */
    if(combine_to_reducer[t_no] != NULL)
    {
        reducer_cur_key[t_no] = combine_to_reducer[t_no]->key;
        reduce_getter_ptr[t_no] = combine_to_reducer[t_no];

        while(reducer_cur_key[t_no] != NULL)
        {
            (*reduce_fn)(reducer_cur_key[t_no], NULL, reduce_get_next, t_no);
        }
    }
    DPRINTF("Thread %d : Finished reducing ....\n", t_no);

    /* Delete all combine to reduce entries */
    delete_list(&combine_to_reducer[t_no]);
    return NULL;
}

void MR_Run(int argc, char *argv[],
        Mapper map, int num_mappers,
        Reducer reduce, int num_reducers,
        Combiner combine,
        Partitioner partition)
{
   
    int i;

    /* Initial condition checks */
    if(num_mappers < 1)
        exit(1);

    if (pthread_mutex_init(&lock, NULL) != 0)
        exit(1);

    /* Initialize global variables */
    filenames   = argv + 1;  
    num_files   = argc - 1;
    count_files = 0;
    gl_num_mappers = num_mappers;
    gl_num_reducers = num_reducers;
    map_fn = map;
    combine_fn = combine;
    reduce_fn = reduce;
    partition_fn = partition;

    DPRINTF("num files : %d\n", num_files);


    th_info = (thread_info_t*)malloc(num_mappers* sizeof(thread_info_t));
    map_to_combine = (node_t**)malloc(num_mappers* sizeof(node_t*));
    combiner_cur_key = (char**)malloc(num_mappers* sizeof(char*));
    combine_getter_ptr = (node_t**)malloc(num_mappers* sizeof(node_t*));
    combine_to_reducer = (node_t**)malloc(num_reducers* sizeof(node_t*));
    reducer_cur_key = (char**)malloc(num_reducers* sizeof(char*));
    reduce_getter_ptr = (node_t**)malloc(num_reducers* sizeof(node_t*));

    for(i= 0; i < num_mappers; i++)
    {
        th_info[i].thread_num = i;
        pthread_create(&th_info[i].thread_id, NULL, mapper_thread, &th_info[i]);
    }

    /* Wait for threads to finish and join */
    for(i= 0; i < num_mappers; i++)
    {
        pthread_join(th_info[i].thread_id, NULL);
    }

    DPRINTF("Deleting mapper threads ..\n");
    free(th_info);
    
    DPRINTF("--------------------------------------------------------------------\n");
    th_info = (thread_info_t*)malloc(num_reducers*sizeof(thread_info_t));


    for(i= 0; i < num_reducers; i++)
    {
        th_info[i].thread_num = i;
        pthread_create(&th_info[i].thread_id, NULL, reducer_thread, &th_info[i]);
    }

     /* Wait for threads to finish and join */
    for(i= 0; i < num_reducers; i++)
    {
        pthread_join(th_info[i].thread_id, NULL);
    }


    free(map_to_combine);
    free(combiner_cur_key); /*TODO free the individual elems */
    free(combine_getter_ptr);
    
    free(reducer_cur_key); /*TODO free the individual elems */
    free(reduce_getter_ptr);
    
    free(combine_to_reducer);
    free(th_info);

}
