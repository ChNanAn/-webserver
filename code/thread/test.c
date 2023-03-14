#include<pthread.h>
#include<unistd.h>
#include<stdio.h>



int a;
pthread_rwlock_t rw_lock;
void * read(void* arg)
{
    while(1)
    {
    pthread_rwlock_rdlock(&rw_lock);
    
    printf("thread %ld==read %d\n",pthread_self(),a);
    pthread_rwlock_unlock(&rw_lock);
    usleep(200);
    }
    return NULL;

}
void * write(void* arg)
{
    while(1)
    {
     pthread_rwlock_wrlock(&rw_lock);
    a++;
    printf("==writeing%d\n",a);
    pthread_rwlock_unlock(&rw_lock);
    usleep(200);
    }
    return NULL;


}


int main()
{
    pthread_t w_thread[3],r_thread[5];
    //pthread_rwlock_t rwlock;
    pthread_rwlock_init(&rw_lock,NULL);

  for(int i=0;i<3;i++)
    {
        pthread_create(&w_thread[i],NULL,write,NULL);
    }
    for(int i=0;i<5;i++)
    {
      pthread_create(&r_thread[i],NULL,read,NULL);

    }
    
    pthread_detach(w_thread[0]);
     pthread_detach(w_thread[1]);
      pthread_detach(w_thread[2]);
      pthread_detach(r_thread[0]);
        pthread_detach(r_thread[1]);
          pthread_detach(r_thread[2]);
            pthread_detach(r_thread[3]);
              pthread_detach(r_thread[4]);
    pthread_rwlock_destroy(&rw_lock);
    pthread_exit(NULL);
}