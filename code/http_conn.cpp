#include<fcntl.h>
#include"http_conn.h"

//test git 

const char*ok_200_title="ok";
const char*error_400_title="bad request";
const char* error_400_form="your request has bad syntaxor is inherently impossible to satisfy.\n";
const char*error_403_title="forbiden";
const char*error_403_form="you do not have permission to get  file from this server\n";
const char*error_404_title="not fount";
const char*error_404_form="the request file was not found on this server.\n";
const char*error_500_title=" internal error";
const char*error_500_form="there was an unusual problem serving the requested file\n";




const char * root_dic="/home/tangni/webservermy/root";
int http_conn::m_epollfd=0; //所有sock上的事件都注册到一个epoll中
int http_conn::m_user_count=0; //http连接的总数量
void setnoblocking(int fd)
{
    int oldfg=fcntl(fd,F_GETFL);
    oldfg=oldfg|O_NONBLOCK;
    fcntl(fd,F_SETFL,oldfg);
}
 int addfd(int epfd,int fd,uint32_t events)
 {
    epoll_event m_event;
    m_event.events=events|EPOLLHUP|EPOLLRDHUP|EPOLLERR;//文件描述符关闭，对端连接异常断开，发生错误
    m_event.data.fd=fd;

    epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&m_event);
    setnoblocking(fd);

 }
 int delfd(int epfd,int fd)
 {
    epoll_ctl(epfd,EPOLL_CTL_DEL,fd,NULL);
    close(fd);
 }
 int modfd(int epfd,int fd,uint32_t events)
 {
    epoll_event event={0};
    event.events=events|EPOLLRDHUP|EPOLLERR|EPOLLHUP;
    event.data.fd=fd;

    return epoll_ctl(epfd,EPOLL_CTL_MOD,fd,&event)==0;


 }


void http_conn::init(int fd,sockaddr_in addr)
{
    m_sockfd=fd;
    m_addr=addr;

    //设置端口复用
    int optval=1;
    setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval));

    //添加到epoll实例中
    addfd(m_epollfd,fd,EPOLLIN|EPOLLONESHOT|EPOLLET);
    http_conn::m_user_count++;
    init();
}
 void http_conn::init()
 {
    m_index_write=0;//记录写缓存区第一个空的位置
    m_index_read=0;
    m_index_check=0;//检查时当前字符所在缓冲区的位置
    m_line_start=0 ; //当前行的起始位置
    m_check_state=CHECK_REQUESTLINE;

    bzero(m_read_buff,read_buff_size);
    bzero(m_write_buff,write_buff_size);
    bzero(m_real_file, FILENAME_LEN);

     m_url=NULL;
     m_version=NULL;
     m_methon=GET;
     m_linger=false;
     m_content_length=0;
      m_host=0;  

    bytes_to_send=0; //将要发送的数据的字节数
    bytes_have_send=0;
 }
void http_conn::close_conn()
{
    if(m_sockfd!=-1)
    {
        delfd(m_epollfd,m_sockfd);
        m_sockfd=-1;
        http_conn::m_user_count--;
    }
}

 bool http_conn::read()
 {
    if(m_index_read>=read_buff_size)
    {
        return false;
    }
    while(1)
    {
        int read_byte=recv(m_sockfd,m_read_buff+m_index_read,read_buff_size-m_index_read,0);
        if(read_byte==-1)
        {
            if(errno==EAGAIN||errno==EWOULDBLOCK)
            break;
            return false;
        }
        else if(read_byte==0)
        {
            return false;
        }
        else
        m_index_read+=read_byte;
    }

    printf("一次性读完数据:\n%s",m_read_buff);
    return true;
 } 
bool http_conn::write()
{
    //
    printf("一次性写完数据\n");
        
        int temp=0;
        if(bytes_to_send==0){
            modfd(m_epollfd,m_sockfd,EPOLLIN|EPOLLONESHOT|EPOLLET);
            init();
            return true;
        }
        while(1)
        {
            temp=writev(m_sockfd,m_iv,m_iv_count);
            if(temp<=-1)
            {
                if((errno==EAGAIN)||(errno==EINTR)){
                    modfd(m_epollfd,m_sockfd,EPOLLOUT|EPOLLONESHOT|EPOLLET);
                    return true;
                }
                unmap();
                return false;
            }
            bytes_have_send+=temp;
            bytes_to_send-=temp;

            if(bytes_have_send>=m_iv[0].iov_len)
            {
                m_iv[0].iov_len=0;
                m_iv[1].iov_base=m_file_address+(bytes_have_send-m_index_write);
                m_iv[1].iov_len=bytes_to_send;
            }
            else
            {
                m_iv[0].iov_base=m_write_buff+bytes_have_send;
                m_iv[0].iov_len=m_iv[0].iov_len-temp;
            }
            if(bytes_to_send<=0)
            {
                //数据发送完了
                unmap();
                modfd(m_epollfd,m_sockfd,EPOLLIN|EPOLLET|EPOLLONESHOT);
                if(m_linger)
                {

                    init();
                    return true;
                }
                else
                {
                    return false;
                }
            }
        }
}

void http_conn::process()//由工作线程调用，处理http请求的入口函数
{
    //解析请求
    HTTP_CODE read_ret=process_read();

    if(read_ret==NO_REQUEST)
    {
        modfd(m_epollfd,m_sockfd,EPOLLIN|EPOLLONESHOT|EPOLLET);
        return ;
    }

    
    //准备响应
    bool write_ret = process_write( read_ret );
    if ( !write_ret ) {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT|EPOLLONESHOT|EPOLLET);


}


http_conn::HTTP_CODE http_conn::process_read()
{
     //解析请求
    printf("线程:%ld 开始解析请求\n",pthread_self());

    LINE_STATE line_state=LINE_OK;
    HTTP_CODE ret=NO_REQUEST;
    char *text=0;
    
    while(((m_check_state==CHECK_REQUESTCONTENT)&&(line_state==LINE_OK))||
          ((line_state=parse_line())==LINE_OK))
          {
            text=get_line();
            m_line_start=m_index_check;

            switch(m_check_state)
            {
                case CHECK_REQUESTLINE:
                {
                    ret=parse_requestline(text);
                    if(ret==BAD_REQUEST)
                    return BAD_REQUEST;
                     break;
                }
               
                case CHECK_REQUESTHEAD:
                {
                    ret=parse_request_head(text);
                    if(ret==BAD_REQUEST)
                    return BAD_REQUEST;
                    else if(ret==GET_REQUEST)
                    {
                        return do_request();
                    }
                    break;
                }
                case CHECK_REQUESTCONTENT:
                {
                    ret=parse_request_content(text);
                    if(ret==GET_REQUEST)
                    {
                        do_request();
                    }
                    line_state=LINE_OPEN;
                    break;

                }
                default:
                {
                    return INTERNAL_ERROR;

                }
                
            }

          }
        return NO_REQUEST;

}
http_conn::HTTP_CODE http_conn::parse_requestline(char *text)  //解析请求行 方法，url,版本号
{
    //get /index.html http/1.1
    m_url=strpbrk(text," \t");
    
    *m_url++='\0';
    printf(" 解析的方法:%s\n",text);   
    //get\0/index.html http/1.1
    char *method=text;
    if(strcasecmp(method,"get")==0)
    {
        m_methon=GET;
    }
    else if(strcasecmp(method,"post"))
    {
        m_methon=POST;
         
    }
    else{
        return BAD_REQUEST;
    }
    m_version=strpbrk(m_url," \t");
    if(!m_version)
    {
        return BAD_REQUEST;

    }
    *m_version++='\0';

     // GET /index.html HTTP/1.1
    // m_url = strpbrk(text, " \t"); // 判断第二个参数中的字符哪个在text中最先出现
    // if (! m_url) { 
    //     return BAD_REQUEST;
    // }
    // // GET\0/index.html HTTP/1.1
    // *m_url++ = '\0';    // 置位空字符，字符串结束符
    // char* method = text;
    // if ( strcasecmp(method, "GET") == 0 ) { // 忽略大小写比较
    //     m_methon = GET;
    // } else {
    //     return BAD_REQUEST;
    // }
    // // /index.html HTTP/1.1
    // // 检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标。
    // m_version = strpbrk( m_url, " \t" );
    // if (!m_version) {
    //     return BAD_REQUEST;
    // }
    // *m_version++ = '\0';
    // if (strcasecmp( m_version, "HTTP/1.1") != 0 ) {
    //     return BAD_REQUEST;
    // }
    // /**
    //  * http://192.168.110.129:10000/index.html
    // */
    // if (strncasecmp(m_url, "http://", 7) == 0 ) {   
    //     m_url += 7;
    //     // 在参数 str 所指向的字符串中搜索第一次出现字符 c（一个无符号字符）的位置。
    //     m_url = strchr( m_url, '/' );
    // }
    // if ( !m_url || m_url[0] != '/' ) {
    //     return BAD_REQUEST;
    // }
     printf("版本:%s\n",m_version);   
     printf("方法:%d\n",m_methon);
     printf("URL:%s\n",m_url);
    m_check_state=CHECK_REQUESTHEAD;
    return NO_REQUEST;
    


}
http_conn::HTTP_CODE http_conn::parse_request_head(char *text)
{
    //遇到空行表示头部字段解析完毕
    if(text[0]=='\0'){
        //如果由消息体，则还需要继续解析
        //将状态机转移到check_request_content
        if(m_content_length!=0)
        {
            m_check_state=CHECK_REQUESTCONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }else if(strncasecmp(text,"connection:",11)==0){
        //处理connection头部字段
        text+=11;
        text+=strspn(text," \t");
        if(strcasecmp(text,"keep-alive")==0){
            m_linger=true;
        }
    }else if(strncasecmp(text,"Content-Length:",15)==0){
        text+=15;
        text+=strspn(text," \t");
        m_content_length=atol(text);
    }else if(strncasecmp(text,"Host:",5)==0){
        text+=5;
        text+=strspn(text," \t");
        m_host=text;
    }else{
        printf("oop! unknow header%s\n",text);
    }
    return NO_REQUEST;


}
http_conn::HTTP_CODE http_conn::parse_request_content(char *text)
{

}
http_conn::LINE_STATE http_conn::parse_line()
{
    for(;m_index_check<m_index_read;m_index_check++)
    {
        if(m_read_buff[m_index_check]=='\r')
        {
            if(m_index_check+1==m_index_read)
            {
                return LINE_OPEN;
            }
            else if(m_read_buff[m_index_check+1]=='\n')
            {
                m_read_buff[m_index_check++]='\0';
                 m_read_buff[m_index_check++]='\0';
                 //m_line_start=m_index_check; 
                 return LINE_OK;   
            }
            return LINE_BAD;
        }
        else if(m_read_buff[m_index_check]=='\n')
        {
            if((m_index_check>1)&&(m_read_buff[m_index_check-1]=='\r'))
            {
                m_read_buff[m_index_check-1]='\0';
                m_read_buff[m_index_check++]='\0';
                //m_line_start=m_index_check; 
                return LINE_OK; 
            }
            return LINE_BAD;
        }

    }
    return LINE_OPEN;
}


http_conn::HTTP_CODE http_conn::do_request()
{
    //解析请求
    

    strcpy(m_real_file,root_dic);
    int len=strlen(root_dic);
    strncpy( m_real_file + len, m_url, FILENAME_LEN - len - 1 );
    // 获取m_real_file文件的相关的状态信息，-1失败，0成功
    if ( stat( m_real_file, &m_file_stat ) < 0 ) {
        return NO_RESOURCE;
    }

    // 判断访问权限
    if ( ! ( m_file_stat.st_mode & S_IROTH ) ) {
        return FORBIDDEN_REQUEST;
    }

    // 判断是否是目录
    if ( S_ISDIR( m_file_stat.st_mode ) ) {
        return BAD_REQUEST;
    }

    // 以只读方式打开文件
    int fd = open( m_real_file, O_RDONLY );
    // 创建内存映射
    m_file_address = ( char* )mmap( 0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
    close( fd );
    return FILE_REQUEST;
       
}

//下面是生产响应的部分
bool http_conn::process_write( HTTP_CODE read_ret)
{

    switch(read_ret)
    {
         case FILE_REQUEST:
        {
            add_status_line(200,ok_200_title);
            add_heads(m_file_stat.st_size);
            m_iv[0].iov_base=m_write_buff;
            m_iv[0].iov_len=m_index_write;
            m_iv[1].iov_base=m_file_address;
            m_iv[1].iov_len=m_file_stat.st_size;
            m_iv_count=2;
            bytes_to_send=m_index_write+m_file_stat.st_size;
            return true;
            break;

        }
        case NO_RESOURCE:
        {
            add_status_line( 404, error_404_title );
            add_heads(strlen(error_404_form));
            if(!add_content(error_404_form))
            return false;
           break;
        }

        case FORBIDDEN_REQUEST:
        {
            add_status_line(403,error_403_title);
            add_heads(strlen(error_403_form));
            if(!add_content(error_403_form))
            return false;
            break;
        }

        case INTERNAL_ERROR:
        {
            add_status_line(500,error_500_title);
            add_heads(strlen(error_500_form));
            if(!add_content(error_500_form));
            break;
        }
        case BAD_REQUEST:
        {
            add_status_line(400,error_400_title);
            add_heads(strlen(error_400_form));
            if(!add_content(error_400_form))
            return false;
             break;
        }
       
        default:
        return false;

    }
    m_iv[0].iov_base=m_write_buff;
    m_iv[0].iov_len=m_index_write;
    m_iv_count=1;
    bytes_to_send=m_index_write;
    return true;

}

bool http_conn::add_status_line(int status,const char* status_title)
{
    return add_response("%s%d%s\r\n","HTTP/1.1",status,status_title);
    
}
 bool http_conn:: add_response(const char*format,...)
 {
    if(m_index_write>=write_buff_size){
        return false;
    }
    va_list arg_list;
    va_start(arg_list,format);
    int len=vsnprintf(m_write_buff+m_index_write,write_buff_size-1-m_index_write,format,arg_list);
    if(len>=(write_buff_size-1-m_index_write)){
        return false;
    }
    m_index_write+=len;
    va_end(arg_list);
    return true;

 }
  bool http_conn::add_heads(int content_length)
  {
    add_content_length(content_length);
    add_content_type();
    add_linger();
    add_blank_line();
  }

  bool http_conn::add_content(const char *content)
  {
    add_response("%s",content);

  }
 bool http_conn::add_content_length(int content_length)
 {
    return add_response("content-length:%d\r\n",content_length);
 }
 bool http_conn:: add_content_type(){
    return add_response("content-type:%s\r\n","text/html");
}
 bool http_conn::add_linger(){
    return add_response("connection:%s\r\n",(m_linger==true)?"keep-live":"close");
}
 bool http_conn:: add_blank_line()
 {
    return add_response("%s","\r\n");
 }
 void http_conn::unmap()
 {
    if(m_file_address)
    {
        munmap(m_file_address,m_file_stat.st_size);
        m_file_address=0;
    }

 }
  

