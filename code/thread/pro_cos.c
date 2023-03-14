#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>





struct Node
{
    int num;
    Node* next;
};

Node* head=NULL;

int nums;
pthread_mutex_t m_mutex1;

pthread_cond_t cond,cond2;

void * producer(void *)
{
    while(1)
    {
        pthread_mutex_lock(&m_mutex1);
        while(nums==10)
        {
            pthread_cond_wait(&cond2,&m_mutex1);
        }
        Node *newNode=(Node*)malloc(sizeof(Node));
        newNode->next=head;
        head=newNode;
        head->num=nums;
        printf("producer write%d\n",nums);
        nums++;
        pthread_mutex_unlock(&m_mutex1);
        pthread_cond_signal(&cond);
        usleep(2);
    }

}
void * customer(void * arg)
{
    
    while(1)
    {
        pthread_mutex_lock(&m_mutex1);
        while(head==NULL)
        {
        pthread_cond_wait(&cond,&m_mutex1);
        }
        Node* temp=head;
        head=head->next;
        printf("%ld read %d\n",pthread_self(),temp->num);
        delete temp;
        nums--;
        pthread_mutex_unlock(&m_mutex1);
        pthread_cond_signal(&cond2);
       
    }

}




int main()
{
    pthread_mutex_init(&m_mutex1,NULL);
    pthread_cond_init(&cond,NULL);
    pthread_cond_init(&cond2,NULL);
    pthread_t p_thread[5],c_thread[5];

    for(int i=0;i<5;i++)
    {
        pthread_create(&p_thread[i],NULL,producer,NULL);
    }
    for(int i=0;i<5;i++)
    {
        pthread_create(&c_thread[i],NULL,customer,NULL);
    }
    for(int i=0;i<5;i++)
    {
        pthread_join(p_thread[i],NULL);
        pthread_join(c_thread[i],NULL);
    }

}