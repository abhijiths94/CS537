#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <semaphore.h>
#include "mapreduce.h"
#include <assert.h>

#define DEBUG_PRINT 0u

#if DEBUG_PRINT
#define DPRINTF(...) if (DEBUG_PRINT) { printf(__VA_ARGS__);}
#else
#define DPRINTF(...) 
#endif

typedef struct  
{    
   pthread_t thread_id;        /* ID returned by pthread_create() */
   int       thread_num;       /* Application-defined thread # */
} thread_info_t;

typedef struct vnode {
    char *value;
    struct vnode *next;
} vnode_t;

typedef struct kvpair {
    char *key;
    struct vnode *vn;
    struct vnode *current;
    struct kvpair *next;
} kvpair_t;

Mapper fp_map;
Reducer fp_reduce;
Partitioner fp_part;

struct kvpair **map_to_combine;
struct kvpair **combine_to_reducer;

/* Store filenames passed */
char** filenames;
/* Number of filenames passed */
int next_file = 0;
int num_files = 0;
int count_files = 0;
int gl_num_mappers = 0;
int gl_num_reducers = 0;


pthread_mutex_t file_lock;
pthread_mutex_t* m2c_arr_lock;
pthread_mutex_t* c2r_arr_lock;

/* Thread info structure */
thread_info_t *th_info;
/* Global map function */
Mapper map_fn;
/* Global Combine function */
Combiner combine_fn;
/* Global reduce function */
Reducer reduce_fn;
/* Global Partition fn */
Partitioner partition_fn;

unsigned long MR_DefaultHashPartition(char *key, int num_partitions) 
{
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0')
        hash = hash * 33 + c;
    return hash % num_partitions;
}

int get_thread_num(pthread_t thrd, int num)
{
    int i;
    for(i = 0; i < num; i++)
    {
        if(th_info[i].thread_id == thrd)
        {
            return th_info[i].thread_num;
        }
    }
    assert(0);
    return -1;
}

char* reducer_get_next(char *key, int partition_number) 
{
    struct kvpair *ptr;
    char *temp;

    ptr = combine_to_reducer[partition_number];
    if (ptr == NULL)
        return NULL;

    while (ptr != NULL) {
        //if (ptr->key == key) {
        if(strcmp(ptr->key, key)==0) {
            if (ptr->current == NULL)
                return NULL;
            temp = ptr->current->value;
            ptr->current = ptr->current->next;
            return temp;
        }
        ptr = ptr->next;
    }

    return NULL;
}

char* combine_get_next(char *key)
{
    struct kvpair *ptr;
    char *temp;
    int p = get_thread_num(pthread_self(), gl_num_mappers);

    ptr = map_to_combine[p];
    if (ptr == NULL)
        return NULL;

    while (ptr != NULL) {
        if (ptr->key == key) {
            if (ptr->current == NULL)
                return NULL;
            temp = ptr->current->value;
            ptr->current = ptr->current->next;
            return temp;
        }
        ptr = ptr->next;
    }

    return NULL;
}



void* mapper_thread(void* args) 
{
    char *file;
    while (filenames[next_file] != NULL) 
    {
        pthread_mutex_lock(&file_lock);
        if (filenames[next_file]) {
            file = filenames[next_file++];
            pthread_mutex_unlock(&file_lock);
            map_fn(file);
        } 
        else 
        {
            pthread_mutex_unlock(&file_lock);
        }
    }

    /* Start combine here */
    if(combine_fn != NULL)
    {

        kvpair_t *ptr;
        vnode_t *temp;
        int p = get_thread_num(pthread_self(), gl_num_mappers);


        ptr = map_to_combine[p];
        while (ptr != NULL) 
        {
            combine_fn(ptr->key, combine_get_next);
            ptr = ptr->next;
            while (map_to_combine[p]->vn) 
            {
                temp = map_to_combine[p]->vn;
                map_to_combine[p]->vn = map_to_combine[p]->vn->next;
                free(temp->value);
                free(temp);
            }
            free(map_to_combine[p]->key);
            free(map_to_combine[p]);
            map_to_combine[p] = ptr;
        }
    }
    return NULL;
}


void* reducer_thread(void* args) 
{
    kvpair_t *ptr;
    vnode_t *temp;
    int p = get_thread_num(pthread_self(), gl_num_reducers);


    ptr = combine_to_reducer[p];
    while (ptr != NULL) 
    {
        DPRINTF("Calling reducer : key %s and part %d\n", ptr->key, p);
        reduce_fn(ptr->key, NULL, reducer_get_next, p);
        ptr = ptr->next;
        while (combine_to_reducer[p]->vn) 
        {
            temp = combine_to_reducer[p]->vn;
            combine_to_reducer[p]->vn = combine_to_reducer[p]->vn->next;
            free(temp->value);
            free(temp);
        }
        free(combine_to_reducer[p]->key);
        free(combine_to_reducer[p]);
        combine_to_reducer[p] = ptr;
    }
    return NULL;
}


void MR_EmitToReducer(char *key, char *value) 
{
    struct kvpair *ptr;
    struct kvpair *prev = NULL, *new;
    int p;

    p = partition_fn(key, gl_num_reducers);

    DPRINTF("ER : ");

    pthread_mutex_lock(&c2r_arr_lock[p]);
    ptr = combine_to_reducer[p];
    if (ptr == NULL) 
    {
        DPRINTF("Adding to head : key %s : val %s\n", key, value);
        new = malloc(sizeof(struct kvpair));
        new->key = strdup(key);
        new->next = NULL;

        struct vnode *newv = malloc(sizeof(struct vnode));
        newv->value = strdup(value);
        newv->next = NULL;
        new->vn = newv;
        new->current = newv;

        combine_to_reducer[p] = new;
        goto end;
    }

    while (ptr != NULL) 
    {
        int cmp = strcmp(ptr->key, key);
        if (cmp == 0) 
        {

            DPRINTF("Adding to existing : key %s : val %s\n", key, value);
            struct vnode *newv = malloc(sizeof(struct vnode));
            newv->value = strdup(value);
            newv->next = ptr->vn;
            ptr->vn = newv;
            ptr->current = newv;
            goto end;
        }
        else if (cmp > 0) 
        {
            break;
        }
        prev = ptr;
        ptr = ptr->next;
    }


    DPRINTF("Adding new key : key %s : val %s\n", key, value);
    new = malloc(sizeof(struct kvpair));
    new->key = strdup(key);
    new->next = ptr;
    if (prev != NULL)
        prev->next = new;
    else
        combine_to_reducer[p] = new;

    struct vnode *newv = malloc(sizeof(struct vnode));
    newv->value = strdup(value);
    newv->next = new->vn;
    new->vn = newv;
    new->current = newv;

end:
    pthread_mutex_unlock(&c2r_arr_lock[p]);
}

void MR_EmitToCombiner(char *key, char *value) 
{
    struct kvpair *ptr;
    struct kvpair *prev = NULL, *new;
    int p;

    p = get_thread_num(pthread_self(), gl_num_mappers);

    pthread_mutex_lock(&m2c_arr_lock[p]);
    ptr = map_to_combine[p];
    if (ptr == NULL) 
    {
        new = malloc(sizeof(struct kvpair));
        new->key = strdup(key);
        new->next = NULL;

        struct vnode *newv = malloc(sizeof(struct vnode));
        newv->value = strdup(value);
        newv->next = new->vn;
        new->vn = newv;
        new->current = newv;

        map_to_combine[p] = new;
        goto end;
    }

    while (ptr != NULL) 
    {
        int cmp = strcmp(ptr->key, key);
        if (cmp == 0) 
        {
            struct vnode *newv = malloc(sizeof(struct vnode));
            newv->value = strdup(value);
            newv->next = ptr->vn;
            ptr->vn = newv;
            ptr->current = newv;
            goto end;
        }
        else if (cmp > 0) 
        {
            break;
        }
        prev = ptr;
        ptr = ptr->next;
    }

    new = malloc(sizeof(struct kvpair));
    new->key = strdup(key);
    new->next = ptr;
    if (prev != NULL)
        prev->next = new;
    else
        map_to_combine[p] = new;

    struct vnode *newv = malloc(sizeof(struct vnode));
    newv->value = strdup(value);
    newv->next = new->vn;
    new->vn = newv;
    new->current = newv;

end:
    pthread_mutex_unlock(&m2c_arr_lock[p]);
}



void MR_Run(int argc, char *argv[],
        Mapper map, int num_mappers,
        Reducer reduce, int num_reducers,
        Combiner combine,
        Partitioner partition)
{
   
    int i;

    /* Initial condition checks */
    if(num_mappers < 1 || num_reducers < 1 || map == NULL || reduce == NULL || argc <2)
        exit(1);

    if (pthread_mutex_init(&file_lock, NULL) != 0)
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
    if(partition != NULL)
        partition_fn = partition;
    else
        partition_fn = MR_DefaultHashPartition;


    DPRINTF("num files : %d\n", num_files);

    /* Hold mapper threads info */
    th_info = (thread_info_t*)malloc(num_mappers* sizeof(thread_info_t));
    map_to_combine = (kvpair_t**)malloc(num_mappers* sizeof(kvpair_t*));
    combine_to_reducer = (kvpair_t**)malloc(num_reducers* sizeof(kvpair_t*));

    m2c_arr_lock = (pthread_mutex_t*)malloc(num_mappers* sizeof(pthread_mutex_t));
    c2r_arr_lock = (pthread_mutex_t*)malloc(num_reducers* sizeof(pthread_mutex_t));
    
    for(i= 0; i < num_mappers; i++)
    {
        th_info[i].thread_num = i;
        pthread_mutex_init(&m2c_arr_lock[i], NULL);
        pthread_create(&th_info[i].thread_id, NULL, mapper_thread, NULL);
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
        pthread_mutex_init(&c2r_arr_lock[i], NULL);
        pthread_create(&th_info[i].thread_id, NULL, reducer_thread, NULL);
    }

     /* Wait for threads to finish and join */
    for(i= 0; i < num_reducers; i++)
    {
        pthread_join(th_info[i].thread_id, NULL);
    }

    free(map_to_combine);
    free(combine_to_reducer);
    free(th_info);
    free(m2c_arr_lock);
    free(c2r_arr_lock);
}


