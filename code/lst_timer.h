#ifndef LST_TIMER_H
#define LST_TIMER_H

#include<stdio.h>
#include<time.h>
#include<arpa/inet.h>

#define BUFF_SIZE 64
class util_timer;  //前向声明

//用户数据

struct client_data
{
    sockaddr_in address; // 客户端的地址
    int sockfd;          //http连接的文件描述符
    util_timer* timer;    //定时器
};

class util_timer
{
    public:
    util_timer():pre(nullptr),next(nullptr){}
     util_timer* pre;
     util_timer* next;


    time_t expire;                     //超时时间
    void (*call_back)(client_data*);   //任务回调函数
    struct client_data *user_data;
   
};


//定时器链表，它是一个升序，双向链表，且带有头节点和尾节点
class sort_timer_lst
{
    public:
    sort_timer_lst():head(nullptr),tail(nullptr){};
    ~sort_timer_lst()
    {
        util_timer*tmp=head;
        while(tmp)
        {
            head=tmp->next;
            delete tmp;
            tmp=head;
        }

    }

    
    void add_timer(util_timer *timers);
    void del_timer(util_timer* timers);
    void adjust_timer(util_timer* timers);
    void tick();

    private:
    void add_timer(util_timer*timers,util_timer* head);
    void del(util_timer*timers);


    private:
    util_timer *head;          //头节点
    util_timer* tail;         //尾节点
 
};






#endif