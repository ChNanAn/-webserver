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

    int maxfd=lfd;
    fd_set readfds,temp;
    FD_ZERO(&readfds);
    FD_SET(lfd,&readfds);
    while(1)
    {
        temp=readfds;
        select(maxfd+1,&temp,NULL,NULL,NULL);
        for(int i=0;i<maxfd+1;i++)
        {
            if(FD_ISSET(i,&temp))
            {
                if(i==lfd)
                {
                    struct sockaddr_in caddr;
                    socklen_t len=sizeof(caddr);
                    int confd= accept(lfd,(struct sockaddr*)&caddr,&len);
                    printf("建立新连接\n");

                    FD_SET(confd,&readfds);
                    maxfd=maxfd>confd?maxfd:confd;
                }
                else
                {
                char buff[5];
                int len=read(i,buff,5);
                    if(len==0)
                    {
                    FD_CLR(i,&readfds);
                    close(i);         //断开连接考虑最大描述符为多少？？
                    }
                   else
                    {
                    printf("read %s\n",buff);
                    write(i,buff,strlen(buff));
                    }
                }
            }
        }
        
    }
    close(lfd);
    return 0;

}