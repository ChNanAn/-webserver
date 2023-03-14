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
#include<arpa/inet.h>
#include<signal.h>

int main(int arc,char *argv[])
{
    int cfd=socket(PF_INET,SOCK_STREAM,0);
    sockaddr_in seraddr;
    memset(&seraddr,0,sizeof(seraddr));
    seraddr.sin_family=AF_INET;
    inet_pton(AF_INET,"127.0.0.1",&seraddr.sin_addr.s_addr);
    seraddr.sin_port=htons(8080);

    int conreval=connect(cfd,(struct sockaddr* )&seraddr,sizeof(seraddr));
    if(conreval==-1)
    {
        printf("连接失败");
    }
    int num=0;
    while(1)
    {
        char sendbuff[1024]={0};
        sprintf(sendbuff,"send data %d\n",num++);
        if(write(cfd,sendbuff,strlen(sendbuff)+1)==-1)
        printf("write error");

        int len=read(cfd,sendbuff,sizeof(sendbuff));
        if(len==-1)
        perror("read error");
        else if(len==0)
        printf("服务器断开连接");
        else
        printf("recv data%s",sendbuff);
        sleep(1);
    }

    
}