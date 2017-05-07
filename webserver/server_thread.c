#include "request.h"
#include "server_thread.h"
#include "common.h"
#include <pthread.h>

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
};
void producer_enqueue(struct server *sv, int connfd);
int worker_dequeue(struct server *sv);
void* worker(void *arg);

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
    while(1)
    {
        int connfd = worker_dequeue(sv);
	do_server_request(sv, connfd);
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



