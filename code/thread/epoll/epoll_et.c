#include<sys/select.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/time.h>
#include<stdio.h>
#include<sys/socket.h>
#include<bits/types.h>
#include<string.h>
#include<netinet/in.h>
#include<stdlib.h>
#include<sys/epoll.h>
#include<fcntl.h>
int main(int argc,char*argv[])
{
    if(argc!=2)
    {
        printf("按照如下格式运行：%s port number\n",argv[0]); 
    }
    int lfd=socket(PF_INET,SOCK_STREAM,0);
    sockaddr_in seraddr;
    memset(&seraddr,0,sizeof(seraddr));
    seraddr.sin_family=AF_INET;
    seraddr.sin_addr.s_addr=INADDR_ANY;
    seraddr.sin_port=htons(atoi(argv[1]));

    int optval=1;

    setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval));

    if(bind(lfd,(struct sockaddr*)&seraddr,sizeof(seraddr))==-1)
    {printf("bind error");
    exit(-1);}

    if(listen(lfd,10)==-1)
    {printf("listen error");

    exit(-1);}

    int epfd=epoll_create(2);
    epoll_event ep_events[1025];
    epoll_event m_event;
    m_event.events=EPOLLIN;
    m_event.data.fd=lfd;
    
    epoll_ctl(epfd,EPOLL_CTL_ADD,lfd,&m_event);
    while(1)
    {
        int epo_retval=epoll_wait(epfd,ep_events,1024,-1);
        for(int i=0;i<epo_retval;i++)
        {
             if(ep_events[i].data.fd==lfd)
                {
                    struct sockaddr_in caddr;
                    socklen_t len=sizeof(caddr);
                    int confd= accept(lfd,(struct sockaddr*)&caddr,&len);
                    printf("建立新连接\n");
                    int fl=fcntl(confd,F_GETFL);
                    fcntl(confd,F_SETFL,fl|O_NONBLOCK);
                    m_event.events=EPOLLIN|EPOLLET;
                    m_event.data.fd=confd;
                    epoll_ctl(epfd,EPOLL_CTL_ADD,confd,&m_event);
                }
                else
                {
                char buff[5];
                int len=read(ep_events[i].data.fd,buff,sizeof(buff));
                    if(len==0)
                    {
                    epoll_ctl(epfd,EPOLL_CTL_DEL,ep_events[i].data.fd,NULL);
                    close(ep_events[i].data.fd);
                    }
                   else
                    {
                    printf("read %s\n",buff);
                    write(ep_events[i].data.fd,buff,strlen(buff));
                    }
                }
            
        }
        
    }
    close(lfd);
    return 0;

}