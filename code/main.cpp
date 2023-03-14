
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

#define MAX_FD 65535    //最多的http连接
#define MAXEVENTS 10000  //最大的事件数

//添加信号捕捉
int addsig(int sig,void (handler)(int))
{
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig,&sa,NULL);
}

//添加文件描述符到epoll中
//删除文件描述符从epoll中
//修改文件描述符从epoll中，主要是重置epolloneshot
extern  int addfd(int epfd,int fd,uint32_t events);
extern int delfd(int epfd,int fd);
extern int modfd(int epfd,int fd,uint32_t events);




int main(int argc,char* argv[])
{
    if(argc!=2)
    {
        printf("按照如下格式运行：%s port number\n",basename(argv[0])); 
    }
    //对SIGPIE信号处理,忽略它
    addsig(SIGPIPE,SIG_IGN);

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
    int epfd=epoll_create(1);
    epoll_event events[MAXEVENTS];
   
   addfd(epfd,listenfd,EPOLLIN);
   http_conn:: m_epollfd=epfd;

   while(1)
   {
    int num_events=epoll_wait(epfd,events,MAXEVENTS,-1);
    if(num_events==-1)
    {
        std::cerr<<"epoll_wait failed ."<<std::endl;
        exit(-1);
   }
   for(int i=0;i<num_events;i++)
   {
    int sockfd=events[i].data.fd;
    if(events[i].data.fd==listenfd) //new 连接
    {
        printf("new 连接\n");
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
        }

    }
    else if(events[i].events&(EPOLLHUP|EPOLLRDHUP|EPOLLERR))
    {

        user[sockfd].close_conn();
    }
    else if(events[i].events&EPOLLIN)
    {
        printf("有数据到来\n");
        //一次性读取数据成功
        if(user[sockfd].read())
        {
            pool->append(user+sockfd);

        }
        else
        {
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
   }
   close(epfd);
   close(listenfd);
   delete []user;
   delete pool;
   return 0;


}