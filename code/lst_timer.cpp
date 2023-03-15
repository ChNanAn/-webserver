#include"lst_timer.h"



 void sort_timer_lst::add_timer(util_timer *timers)
 {
   // printf("add timer %o\n",timers);
    if(!timers)
    return ;
    if(head==NULL)
    {
        tail=head=timers;
        return;
    }
    if(timers->expire<=head->expire)
    {
        timers->next=head;
        head->pre=timers;
        head=timers;
        return;
    }
    add_timer(timers,head);
   
 }

void sort_timer_lst::add_timer(util_timer*timers,util_timer* head)
{
    util_timer *tem_pre=head;
    util_timer *tem=head->next;

    while(tem)
    {
        if(timers->expire<tem->expire)
        {
            timers->next=tem;
            tem_pre->next=timers;
            timers->pre=tem_pre;
            tem->pre=timers;
            break;

        }
        tem_pre=tem;
        tem=tem->next;
    }
    if(!tem)
    {
        tail->next=timers;
        timers->pre=tail;
        tail=timers;
        tail->next=nullptr;
    }

}
 void sort_timer_lst::del_timer(util_timer* timers)
 {
    //printf("删除定时器\n");

    if(!timers)
    {
        return;
    }
    if((timers==head)&&(timers==tail))
    {
        delete timers;
        head=tail=NULL;
        return;
    }
    if(timers==head)
    {
        head->next->pre=NULL;
        head=head->next;
        delete timers;
        return ;
    }
    if(timers==tail)
    {
        tail=tail->pre;
        tail->next=nullptr;
        delete timers;
        return;
    }

    timers->pre->next=timers->next;
    timers->next->pre=timers->pre;
    delete timers;
 }
void sort_timer_lst::adjust_timer(util_timer* timers)
{
    del(timers);
    timers->next=nullptr;
    timers->pre=nullptr;
    add_timer(timers);
}

void sort_timer_lst::del(util_timer*timers)  //删除不释放资源，用来直接重新加入
{
    if(!timers)
    {
        return;
    }
    if((timers==head)&&(timers==tail))
    {
        head=tail=NULL;
        return;
    }
    if(timers==head)
    {
        head->next->pre=NULL;
        head=head->next;
        return ;
    }
    if(timers==tail)
    {
        tail=tail->pre;
        tail->next=nullptr;
        return;
    }

    timers->pre->next=timers->next;
    timers->next->pre=timers->pre;
    

}

/* SIGALARM 信号每次被触发就在其信号处理函数中执行一次 tick() 函数，以处理链表上到期任务。*/
void sort_timer_lst::tick()
{
    
    if((!head)){
        return;
    }
    printf("timer tick\n");//定时器响了一下

    
    time_t cur=time(NULL);  //获取当前时间

    util_timer *temp=head;
    while(temp!=nullptr){

    if(cur<temp->expire)
    {
        break;
    }

    //// 调用定时器的回调函数，以执行定时任务
    temp->call_back(temp->user_data);
    
    head=temp->next;
    if(head)
    {
        head->pre=nullptr;
    }
    delete temp;
    temp=head;
    }

}