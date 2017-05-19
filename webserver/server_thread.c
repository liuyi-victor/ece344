#include "request.h"
#include "server_thread.h"
#include "common.h"
#include <pthread.h>
#include <string.h>

#define hash_constant 0.6180339887
#define hash_initial_size 2048
struct tracker;
typedef struct cache_bucket
{
  //need synchronization for fields except file_data (can only be accessed in critical region)
  struct file_data *data;
  int valid;
  int referenced;
  struct tracker *node;
  struct cache_bucket *prev;
  struct cache_bucket *next;
} bucket;

struct tracker
{
  struct cache_bucket *item;
  struct tracker *prev;
  struct tracker *next;
};
struct server
{
	int nr_threads;
	int max_requests;
	int max_cache_size;
	/* add any other parameters you need */
  //shared variables
  int *buffer;
  int in;
  int out;
  int buffersize;
  pthread_t *threads;
  pthread_mutex_t lock;
  pthread_cond_t empty;
  pthread_cond_t full;

  bucket **cache;
  pthread_mutex_t cachelock;
  unsigned int hash_length;
  unsigned cache_size;
  struct tracker *queue;
  struct tracker *queueTail;
};
void producer_enqueue(struct server *sv, int connfd);
int worker_dequeue(struct server *sv);
void* worker(void *arg);
void do_server_request_cached(struct server *sv, int connfd);
bucket *cache_lookup(struct server *sv, char *filename);
int cache_add(struct server *sv, bucket *file);
bucket * cache_insert(struct server *sv, bucket *file);
int cache_evict(struct server *sv, unsigned amount);
unsigned long hash_function(struct server *sv, char *filename);

/* static functions */

/* initialize file data */
static struct file_data *
file_data_init(void)
{
	struct file_data *data;

	data = Malloc(sizeof(struct file_data));
	data->file_name = NULL;
	data->file_buf = NULL;
	data->file_size = 0;
	return data;
}

/* free all file data */
static void
file_data_free(struct file_data *data)
{
	free(data->file_name);
	free(data->file_buf);
	free(data);
}

static void
do_server_request(struct server *sv, int connfd)
{
	int ret;
	struct request *rq;
	struct file_data *data;

	data = file_data_init();

	/* fills data->file_name with name of the file being requested */
	rq = request_init(connfd, data);
	if (!rq) {
		file_data_free(data);
		return;
	}
	/* reads file, 
	 * fills data->file_buf with the file contents,
	 * data->file_size with file size. */
	ret = request_readfile(rq);
	if (!ret)
		goto out;
	/* sends file to client */
	request_sendfile(rq);
out:
	request_destroy(rq);
	file_data_free(data);
}

void do_server_request_cached(struct server *sv, int connfd)
{
    int ret;
    struct request *rq;
    struct file_data *data;

    data = file_data_init();

  /* fills data->file_name with name of the file being requested */
    rq = request_init(connfd, data);  //get the client request
    if (!rq)
    {
      file_data_free(data);
      return;
    }

    pthread_mutex_lock(&(sv->cachelock));
    bucket *result = cache_lookup(sv,data->file_name);
    if(result == NULL)
    {
	/* reads file,
	 * fills data->file_buf with the file contents,
	 * data->file_size with file size. */
        pthread_mutex_unlock(&(sv->cachelock));
	ret = request_readfile(rq);
	
	if (!ret)
	{
	  //cannot serve the request because unable to read the file
	    request_destroy(rq);
	    file_data_free(data);
	    return;
	}
	pthread_mutex_lock(&(sv->cachelock));
	bucket *created = (bucket *)Malloc(sizeof(bucket));
	created->data = data;
	created->referenced = 1;
	created->valid = 1;
	created->prev = NULL;
	created->next = NULL;
        int inserted = cache_add(sv, created);     //try adding to cache (fail if there is a duplicate)
	pthread_mutex_unlock(&(sv->cachelock));
	/* sends file to client */
	request_sendfile(rq);
	
	request_destroy(rq);
	if(!inserted)
	  file_data_free(data);
	/*
	if(inserted != NULL)                     //another thread had already inserted the file data (lost the race)
	{
	    file_data_free(data);
	    free(created);
	    rq->data = inserted->data;
	    result = inserted;
	}
	else
	  result = created;
	*/
    }
    else                     //found the file in cache
    {
        result->referenced++;            //atomically set the referenced flag to true
        file_data_free(data);
	request_set_data(rq, result->data);

	//move the node in the queue link list to the head of the link list
	if(result->node->prev != NULL)
	  result->node->prev->next = result->node->next;
	else
	{
	  assert(sv->queue == result->node);
	  //if(sv->queue != result->node)
	  //  return;
	  sv->queue = result->node->next;           //already the head of the queue
	}
	if(result->node->next != NULL)
	  result->node->next->prev = result->node->prev;
	else
	  sv->queueTail = result->node->prev;
	result->node->prev = NULL;
	result->node->next = sv->queue;
	if(sv->queue != NULL)
	  sv->queue->prev = result->node;
	sv->queue = result->node;
	if(sv->queueTail == NULL)
	  sv->queueTail = result->node;
	pthread_mutex_unlock(&(sv->cachelock));
	
	/* sends file to client */
	request_sendfile(rq);
    
	pthread_mutex_lock(&(sv->cachelock));
	result->referenced = result->referenced - 1;
	assert(result->referenced >= 0 );
	pthread_mutex_unlock(&(sv->cachelock));
    
	request_destroy(rq);
    }
 
    /* sends file to client 
    request_sendfile(rq);
    
    pthread_mutex_lock(&(sv->cachelock));
    result->referenced = result->referenced - 1;
    assert(result->referenced >= 0 );
    pthread_mutex_unlock(&(sv->cachelock));
    
    request_destroy(rq);
    */
}
bucket *cache_lookup(struct server *sv, char *filename)
{
  //the cache lock should be acquired at this point since accessing shared variables
  //returns the cache bucket pointer if found, otherwise return NULL
    int index = hash_function(sv, filename);
    bucket *temp = sv->cache[index];
    while(temp != NULL)
    {
        if(strcmp(temp->data->file_name,filename) == 0)
	  break;
        temp = temp->next;
    }
    return temp;
}
int cache_add(struct server *sv, bucket *file)
{
    bucket *search = cache_lookup(sv,file->data->file_name);
    //if(cache_lookup(file->data->file_name) != NULL)
    if(search != NULL)
      return 0;
    else
    {
        unsigned aftertotal = sv->cache_size + file->data->file_size;
        if(aftertotal > sv->max_cache_size)
	{
	    if(cache_evict(sv, aftertotal - sv->max_cache_size))
	      cache_insert(sv, file);
	    else
	      return 0;
	}
	else
	{
	    cache_insert(sv, file);
	}
	return 1;
    }
}
 bucket * cache_insert(struct server *sv, bucket *file)
{
  //the cache lock should be acquired at this point since accessing shared variables
  //insert at the end of bucket to ensure there is no duplicate
  
  //char *filename = file->data->file_name;
    int index = hash_function(sv, file->data->file_name);
    bucket *temp = sv->cache[index];
    if(temp == NULL)    //empty bucket, surely no duplicate
    {
      //simply add the new cache item to the bucket
      sv->cache[index] = file;
      struct tracker *queueitem = (struct tracker *)Malloc(sizeof(struct tracker));
      queueitem->prev = NULL;
      queueitem->next = sv->queue;
      if(sv->queue != NULL)
	sv->queue->prev = queueitem;
      queueitem->item = file;
      sv->queue = queueitem;
      if(sv->queueTail == NULL)
	sv->queueTail = queueitem;
      sv->cache_size += file->data->file_size;
      file->node = queueitem;
      //return 1;
      return NULL;
    }
    while(temp->next != NULL)
    {
      /*
	if(strcmp(temp->next->data->file_name,filename) == 0)
	{
	    (temp->next->referenced)++;
	    //return 0;
	    return temp->next;
	}
      */
	temp = temp->next;
    }
    //inserts the new bucket to the end of the hash table at the specified index
    temp->next = file;
    file->prev = temp;
    
    struct tracker *queueitem = (struct tracker *)Malloc(sizeof(struct tracker));
    queueitem->prev = NULL;
    queueitem->next = sv->queue;
    if(sv->queue != NULL)
	sv->queue->prev = queueitem;
    queueitem->item = file;
    sv->queue = queueitem;
    if(sv->queueTail == NULL)
	sv->queueTail = queueitem;
    sv->cache_size += file->data->file_size;
    file->node = queueitem;
    return NULL;
    /*
    else
    {
        if(strcmp(temp->data->file_name,filename) == 0)
	{
	  temp->referenced++;
	  //return 0;
	  return temp;
	}    
        while(temp->next != NULL)
	{
	  if(strcmp(temp->next->data->file_name,filename) == 0)
	  {
	      (temp->next->referenced)++;
	      //return 0;
	      return temp->next;
	  }
	  temp = temp->next;
	}
	temp->next = file;
	//return 1;
	return NULL;
    }
    */
}
int cache_evict(struct server *sv, unsigned amount)
{
  //need to both free bucket and the tracker queue, while at the same time change the pointers for the hash table and queue accordingly
  //cache mutex lock should have been acquired at this point
     if(amount > sv->max_cache_size)
       return 0;
     struct tracker *temp = sv->queueTail;
     struct tracker *following = NULL;
     bucket *saved = NULL;
     while(amount > 0)
     {
         if(temp == NULL)
	   return 0;
         if(temp->item->referenced <= 0)
	 {
	     assert(temp->item->referenced == 0);

	     amount -= temp->item->data->file_size;
	     sv->cache_size -= temp->item->data->file_size;
	     
	     //remove the tracker from the link list
	     if(temp->prev != NULL)
	       temp->prev->next = temp->next;
	     else
	       sv->queue = temp->next;
	     if(temp->next != NULL)
	       temp->next->prev = temp->prev;
	     else
	       sv->queueTail = temp->prev;
	     following = temp->prev;
	     temp->prev = NULL;
	     temp->next = NULL;
	     saved = temp->item;
	     free(temp);
	     //freed the tracker element
	     if(saved->prev != NULL)
	       saved->prev->next = saved->next;
	     else
	       (sv->cache)[hash_function(sv, saved->data->file_name)] = saved->next;
	     if(saved->next != NULL)
	       saved->next->prev = saved->prev;
	     saved->prev = NULL;
	     saved->next = NULL;
	     file_data_free(saved->data);
	     free(saved);
	 }
	 temp = following;
     }
     return 1;
}
 unsigned long hash_function(struct server *sv, char *filename)
{
    unsigned long key = 0;
    int i;
    for(i = 0; filename[i] != '\0';i++)
    {
        key = key + (filename[i] * (int)pow(2,i));
    }
    unsigned long index = (unsigned long)floor(fmod(hash_constant*key,1.0) * sv->hash_length);
    return index;
}
/* entry point functions */
struct server *server_init(int nr_threads, int max_requests, int max_cache_size)
{
	struct server *sv;

	sv = Malloc(sizeof(struct server));
	sv->nr_threads = nr_threads;
	sv->max_requests = max_requests;
	sv->max_cache_size = max_cache_size;
	
	if(nr_threads > 0 || max_requests > 0)
	{
	    sv->buffer = (int *)Malloc(sizeof(int)*(max_requests+1));
	    sv->buffersize = max_requests + 1;
	    sv->in = 0;
	    sv->out = 0;
	    pthread_mutex_init(&sv->lock,NULL);
	    pthread_cond_init(&sv->empty,NULL);
	    pthread_cond_init(&sv->full,NULL);
	if(max_cache_size > 0)
	{
	    sv->cache = (bucket **)Malloc(sizeof(bucket *) * hash_initial_size);
	    pthread_mutex_init(&sv->cachelock,NULL);
	    sv->hash_length = hash_initial_size;
	    sv->cache_size = 0;
	    sv->queue = NULL;
	    sv->queueTail = NULL;
	}
	    sv->threads = (pthread_t *)Malloc(sizeof(pthread_t) * nr_threads);
	    int i;
	    for (i = 0; i < nr_threads; i++)
	    {
	      SYS(pthread_create(&(sv->threads[i]), NULL, worker,
				 (void *)sv));
	      //SYS(pthread_create((sv->threads + i), NULL, worker,
	    }
	}
/*
	if(max_cache_size > 0)
	{
	    sv->cache = (bucket **)Malloc(sizeof(bucket *) * hash_initial_size);
	    pthread_mutex_init(&sv->cachelock,NULL);
	    sv->hash_length = hash_initial_size;
	    sv->cache_size = 0;
	    sv->queue = NULL;
	    sv->queueTail = NULL;
	}
*/
	/*
	if (nr_threads > 0 || max_requests > 0 || max_cache_size > 0) {
		TBD();
	}
	*/
	
	/* Lab 4: create queue of max_request size when max_requests > 0 */

	/* Lab 5: init server cache and limit its size to max_cache_size */

	/* Lab 4: create worker threads when nr_threads > 0 */

	return sv;
}
void* worker(void *arg)
{
    struct server *sv = (struct server *)arg;
    if(sv->max_cache_size > 0)
    {
        while(1)
	{
	  int connfd = worker_dequeue(sv);
	  do_server_request_cached(sv, connfd);
	}
    }
    else
    {
        while(1)
	{
	  int connfd = worker_dequeue(sv);
	  do_server_request(sv, connfd);
	}
    }
    return NULL;
}
int worker_dequeue(struct server *sv)
{
      pthread_mutex_lock(&(sv->lock));
      while(sv->in == sv->out)   //buffer is empty(no client requests pending)
      {
	  pthread_cond_wait(&(sv->empty), &(sv->lock));
      }
      int descriptor = *(sv->buffer + sv->out);
      //if((sv->in - sv->out + sv->buffersize)%sv->buffersize == sv->buffersize - 1)
	  pthread_cond_signal(&(sv->full));
      sv->out = (sv->out + 1)%sv->buffersize;
      pthread_mutex_unlock(&(sv->lock));
      return descriptor;
}
void producer_enqueue(struct server *sv, int connfd)
{
      pthread_mutex_lock(&(sv->lock));
      while((sv->in - sv->out + sv->buffersize)%sv->buffersize == sv->buffersize - 1)   //buffer is full(no buffer slots)
      {
	  pthread_cond_wait(&(sv->full), &(sv->lock));
      }
      *(sv->buffer + sv->in) = connfd;
      //if(sv->in == sv->out)
	  pthread_cond_signal(&(sv->empty));
      sv->in = (sv->in + 1)%sv->buffersize;
      pthread_mutex_unlock(&(sv->lock));
}
void server_request(struct server *sv, int connfd)
{
	if (sv->nr_threads == 0)
	{ /* no worker threads */
		do_server_request(sv, connfd);
	}
	else
	{
		/*  Save the relevant info in a buffer and have one of the
		 *  worker threads do the work. */
		//TBD();
	  producer_enqueue(sv, connfd);
	}
}



