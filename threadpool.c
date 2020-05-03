#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<signal.h>
#include<error.h>
#include <unistd.h>
#include"threadpool.h"
/* create threadpool :
 Creates the structure of the process Contains an 
 array for the processes Saves information on the
  number of processes and jobs There is a specific 
  queue that is managed by it so that when new work
  is entered it updates it by locking it with three parameters
  one : mutex
  two: empty
  three: not empty
  */
threadpool* create_threadpool(int num_threads_in_pool){
    threadpool* threadpool_manager =(threadpool*) malloc(sizeof(threadpool));
    if(threadpool_manager==NULL){
        perror("malloc\n");
        return NULL;
    }
    threadpool_manager->num_threads = num_threads_in_pool;
    threadpool_manager->qsize=0;
    threadpool_manager->qhead=NULL;
    threadpool_manager->qtail=NULL;
    threadpool_manager->dont_accept=0;
    threadpool_manager->shutdown=0;
    threadpool_manager->threads = (pthread_t*)malloc(sizeof(pthread_t)*threadpool_manager->num_threads);
    if(threadpool_manager->threads==NULL){
        perror("malloc\n");
        return NULL;
    }
    int rc =pthread_mutex_init(&(threadpool_manager->qlock),NULL);
    if(rc!=0){
        free(threadpool_manager->threads);
        free(threadpool_manager);
        return NULL;
    }
    rc=pthread_cond_init(&(threadpool_manager->q_empty),NULL);
    if(rc!=0){
        free(threadpool_manager->threads);
        free(threadpool_manager);
        pthread_mutex_destroy(&(threadpool_manager->qlock));
        return NULL;
    }
    rc=pthread_cond_init(&(threadpool_manager->q_not_empty)
    ,NULL);
    if(rc!=0){
        free(threadpool_manager->threads);
        free(threadpool_manager);
        pthread_mutex_destroy(&(threadpool_manager->qlock));
        pthread_cond_destroy(&(threadpool_manager->q_empty));
        return NULL;
    }
    for(int i=0;i<threadpool_manager->num_threads;i++){
        rc=pthread_create(&threadpool_manager->threads[i],NULL,do_work,threadpool_manager);
        if(rc!=0){
            return threadpool_manager;
        }
    }
    return threadpool_manager;
}
/*
The function receives an argument and a 
\function to execute adds it to the queue 
that is saved in the general structure and 
when any process can work it will be released
 from its lock it will take the function and
run it with the current argument received
There is also a critical section
Every time you add a job to the job queue, 
you have to worry about using the locking 
tools I listed earlier
*/
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
    if(dispatch_to_here==NULL){
        return;
    }
    pthread_mutex_lock(&from_me->qlock);
    if(from_me->dont_accept==1){
        pthread_mutex_unlock(&from_me->qlock);
        return;
    }
    work_t* work=(work_t*)malloc(sizeof(work_t));
    work->arg=arg;
    work->routine=dispatch_to_here;
    work->next=NULL;
    if(from_me->qhead==NULL){
        from_me->qhead=work;
        from_me->qtail=work;
        from_me->qsize++;
        pthread_cond_signal(&from_me->q_not_empty); 
    }
    else{
        from_me->qtail->next=work;
        from_me->qtail=work;
        from_me->qsize++;
        pthread_cond_signal(&from_me->q_not_empty); 
    }
    pthread_mutex_unlock(&from_me->qlock);
} 
/*
The function that builds the job takes the argument
 from a job object and sends it to the function.
We will create the object work while locking a 
critical area when you pull things off and send 
it to work when we wake up one of the work processes
*/
void* do_work(void* p){ 
    threadpool* p_threadpool = p;
    while(1){
        pthread_mutex_lock(&p_threadpool->qlock);
        if(p_threadpool->shutdown==1){
            pthread_mutex_unlock(&p_threadpool->qlock);
            return NULL;
        }
        if(p_threadpool->qsize==0){
            pthread_cond_wait(&p_threadpool->q_not_empty, &p_threadpool->qlock);
        }
        if(p_threadpool->shutdown==1){
            pthread_mutex_unlock(&p_threadpool->qlock);
             return NULL;
        }
        work_t* work= p_threadpool->qhead;
        if(work==NULL){
            pthread_mutex_unlock(&p_threadpool->qlock);
            continue;
        }
        p_threadpool->qhead=p_threadpool->qhead->next;
        p_threadpool->qsize-=1;
        if(p_threadpool->qsize==0){
            p_threadpool->qhead=NULL;
            p_threadpool->qtail=NULL;
        }
        if(p_threadpool->qsize==0 && p_threadpool->dont_accept==1){
            pthread_cond_signal(&p_threadpool->q_empty); 
        }
        pthread_mutex_unlock(&p_threadpool->qlock);
        work->routine(work->arg); 
        free(work);
    }  
    
}
/*
This function is as long as the queue is not 
empty when the queue is empty informing the 
main process Do not accept any more jobs This
 means do not allow the job function when the
  queue is empty The last process that
 took a job informs it that it took the last
  job and then it begins to demolish all processes
*/
void destroy_threadpool(threadpool* destroyme){
    pthread_mutex_lock(&destroyme->qlock);
    destroyme->dont_accept=1;
    if(destroyme->qsize>0){
        pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock);
    }
    destroyme->shutdown=1;
    pthread_cond_broadcast(&destroyme->q_not_empty); 
    //pthread_cond_signal(&destroyme->q_not_empty);
    pthread_mutex_unlock(&destroyme->qlock);
    for(int i=0;i<destroyme->num_threads;i++){
        pthread_join(destroyme->threads[i],NULL);
    } 
   
    pthread_mutex_destroy(&(destroyme->qlock));
    pthread_cond_destroy(&(destroyme->q_empty));
    pthread_cond_destroy(&(destroyme->q_not_empty));
    free(destroyme->threads); 
    free(destroyme);
    
}
