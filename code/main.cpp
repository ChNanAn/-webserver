
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<errno.h>
#include<fcntl.h>
#include<sys/epoll.h>
#include<signal.h>
#include<string>
#include<iostream>
#include"locker.h"
#include"threadpool.h"
#include"http_conn.h"
#include"lst_timer.h"

#define MAX_FD 65535    //最多的http连接
#define MAXEVENTS 10000  //最大的事件数
#define TIME_SLOT 5     //定时器触发时间片

static int pipefd[2];
static sort_timer_lst timer_lst;
static int epfd;
//添加信号捕捉
int addsig(int sig,void (handler)(int))
{
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig,&sa,NULL);
}
void sig_handler(int sig)
{
    int save_errno=errno;
    int msg=sig;
    send(pipefd[1],(char*)&msg,1,0);
    errno=save_errno;
}

void timer_handler()
{
    //定时处理任务，实际就是调用tick()函数
    timer_lst.tick();
    //一次arlarm调用只会引起一次SIGALARM信号，所以重新设闹钟
    alarm(TIME_SLOT);
}

void call_back(client_data* user_data)
{
    epoll_ctl(epfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    //assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
    //printf("关闭连接\n");
}

//添加文件描述符到epoll中
//删除文件描述符从epoll中
//修改文件描述符从epoll中，主要是重置epolloneshot
extern  int addfd(int epfd,int fd,uint32_t events);
extern int delfd(int epfd,int fd);
extern int modfd(int epfd,int fd,uint32_t events);
extern void setnoblocking(int fd);



int main(int argc,char* argv[])
{
    if(argc!=2)
    {
        printf("按照如下格式运行：%s port number\n",basename(argv[0])); 
    }
    //对SIGPIE信号处理,忽略它
    addsig(SIGPIPE,SIG_IGN);

    //对SIGALRM、SIGTERM设置信号处理函数
    addsig(SIGALRM,sig_handler);
    addsig(SIGTERM,sig_handler);

    threadpool<http_conn> * pool;
    try{
        pool=new threadpool<http_conn>;
    }
    catch(...)
    {
        exit(-1);
    } 

    //创建一个数组保存http_conn
    http_conn *user=new http_conn[MAX_FD];

   // 创建监听套接字
    int listenfd=socket(PF_INET,SOCK_STREAM,0);
    
    //设置端口复用
    int reuse=1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

    //绑定
    struct sockaddr_in address;
    memset(&address,0,sizeof(address));
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=INADDR_ANY;
    address.sin_port=htons(atoi(argv[1]));
    bind(listenfd,(struct sockaddr*)&address,sizeof(address));


    //listen
    listen(listenfd,10);

    //创建epoll实例
    epfd=epoll_create(1);
    epoll_event events[MAXEVENTS];
   
   addfd(epfd,listenfd,EPOLLIN);
   http_conn:: m_epollfd=epfd;

    // 创建管道
   int ret=socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    //assert( ret != -1 );
    setnoblocking( pipefd[1] );
    addfd(epfd, pipefd[0] ,EPOLLIN|EPOLLET);

    bool timeout=false;
    client_data* client_users=new client_data[MAX_FD];
    alarm(TIME_SLOT);//定时，5秒后闹钟响

    bool stop_server=false;
   while(!stop_server)
   {
    int num_events=epoll_wait(epfd,events,MAXEVENTS,-1);
    if(num_events==-1)
    {
        if(errno==EINTR) //epoll_wait会被SIGALRM信号中断返回-1
        {
            continue;
        }
        std::cerr<<"epoll_wait failed ."<<std::endl;
        exit(-1);
   }
   for(int i=0;i<num_events;i++)
   {
    int sockfd=events[i].data.fd;
    if(events[i].data.fd==listenfd) //new 连接
    {
        //printf("new 连接\n");
        struct sockaddr_in conaddr;
        socklen_t conlen=sizeof(conaddr);
        int confd=accept(listenfd,(struct sockaddr*)&conaddr,&conlen);
        if(confd==-1)
        {
            std::cerr<<"accept error"<<std::endl;
        }
        if(http_conn::m_user_count>MAX_FD)
        {
            //给客户端响应一个消息，正忙
            close(confd);
            continue;
        }
        else
        {

        user[confd].init(confd,conaddr);



        // 创建定时器，设置其回调函数与超时时间，然后绑定定时器与用户数据，最后将定时器添加到链表timer_lst中
            util_timer* timer = new util_timer;
            timer->call_back = call_back;
            time_t cur = time( NULL );
            timer->expire = cur + 3 * TIME_SLOT;

            client_users[confd].timer=timer;
            client_users[confd].sockfd=confd;
            client_users[confd].address=conaddr;

            timer->user_data = &client_users[confd];
             
            timer_lst.add_timer( timer );
        }

    }
    else if(( sockfd == pipefd[0] ) && ( events[i].events & EPOLLIN ) ) 
    {
                // 处理信号
            int sig;
            char signals[1024];
            int ret = recv( pipefd[0], signals, sizeof( signals ), 0 );
            if( ret == -1 ) {
                continue;
            } else if( ret == 0 ) {
                continue;
            } else  {
                for( int i = 0; i < ret; ++i ) {
                    switch( signals[i] )  {
                        case SIGALRM:
                        {
                            // 用timeout变量标记有定时任务需要处理，但不立即处理定时任务
                             // 这是因为定时任务的优先级不是很高，我们优先处理其他更重要的任务。
                             timeout = true;
                             break;
                        }
                        case SIGTERM:
                        {
                            stop_server = true;
                        }
                        }
                    }
                }
    }
    else if(events[i].events&(EPOLLHUP|EPOLLRDHUP|EPOLLERR))
    {

        user[sockfd].close_conn();
    }
    else if(events[i].events&EPOLLIN)
    {
        util_timer* timer=client_users[sockfd].timer;
        time_t cur = time( NULL );
        timer->expire = cur + 3 * TIME_SLOT;
        timer_lst.adjust_timer(timer);

       
        //printf("有数据到来\n");
        //一次性读取数据成功
        if(user[sockfd].read())
        {
            pool->append(user+sockfd);

        }
        else
        {
            timer_lst.del_timer(timer);
            user[sockfd].close_conn();
        }
        
    }
    else if(events[i].events&EPOLLOUT)
    {
        //一次性写完所有数据
        bool ret=user[sockfd].write();
        if(!ret)
        {
            user[sockfd].close_conn();
        }

       
    }
    
   }
   // 最后处理定时事件，因为I/O事件有更高的优先级。当然，这样做将导致定时任务不能精准的按照预定的时间执行。
    if( timeout ) {
        timer_handler();
        timeout = false;
    }
   }
   close(epfd);
   close(listenfd);
   delete []user;
   delete pool;
   return 0;

}