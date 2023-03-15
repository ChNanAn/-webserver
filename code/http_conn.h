#ifndef HTTPCONNECTTION_H
#define HTTPCONNECTTION_H
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<errno.h>
#include<fcntl.h>
#include<sys/epoll.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<sys/uio.h>
#include<signal.h>
#include<stdarg.h>
#include"locker.h"
#include"lst_timer.h"


class http_conn{
    public:
    static int m_epollfd; //所有sock上的事件都注册到一个epoll中
    static int m_user_count; //http连接的总数量
    const static int read_buff_size=2048;    //读缓冲区大小
    const static int write_buff_size=2048;   //写缓冲区大小
    const static int FILENAME_LEN=200;      //文件名长度

    //方法
    enum METHOED{GET=0,POST,PUT,HEAD,DELETE,TRACE,CONNECT};

    //解析请求时，主状态机的状态，
    enum CHECK_STATE{CHECK_REQUESTLINE=0,CHECK_REQUESTHEAD,CHECK_REQUESTCONTENT};
    //从状态机的三种状态,OK， 出错， 不完整
    enum LINE_STATE{LINE_OK=0,LINE_BAD,LINE_OPEN};

     /*
        服务器处理HTTP请求的可能结果，报文解析的结果
        NO_REQUEST          :   请求不完整，需要继续读取客户数据
        GET_REQUEST         :   表示获得了一个完成的客户请求
        BAD_REQUEST         :   表示客户请求语法错误
        NO_RESOURCE         :   表示服务器没有资源
        FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
        FILE_REQUEST        :   文件请求,获取文件成功
        INTERNAL_ERROR      :   表示服务器内部错误
        CLOSED_CONNECTION   :   表示客户端已经关闭连接了
    */
    enum HTTP_CODE{NO_REQUEST,GET_REQUEST,BAD_REQUEST,NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,INTERNAL_ERROR, CLOSED_CONNECTION };




    http_conn(){};
    ~http_conn(){};
    void init(int fd,sockaddr_in);//初始化新接受的连接
    void close_conn();    //关闭连接
    void process();//处理客户端请求
    bool read();   //非阻塞的读
    bool write();  //非阻塞的写

    HTTP_CODE do_request();
    char *get_line()
    {
        return m_read_buff+m_line_start;
    }
    
    private:
    void init();
    HTTP_CODE process_read();//解析请求
    HTTP_CODE parse_requestline(char *text);
    HTTP_CODE parse_request_head(char *text);
    HTTP_CODE parse_request_content(char *text);
    LINE_STATE parse_line();


   bool process_write( HTTP_CODE read_ret);


    private:

    char m_read_buff[read_buff_size];
    int m_index_read;//记录读缓冲区的第一个空的位置
    char m_write_buff[write_buff_size];
    int m_index_write;//记录写缓存区第一个空的位置
    struct iovec m_iv[2];
    int  m_iv_count;
    int m_sockfd;  //这个http连接对象的sockfd
    sockaddr_in  m_addr; //这个http连接对象连接的地址
    int m_index_check; //检查时当前字符所在缓冲区的位置
    int m_line_start;  //当前行的起始位置
    CHECK_STATE m_check_state;//主状态机当前的状态

    char* m_url;
    char* m_version;
    METHOED m_methon;
    char * m_host;
    bool m_linger; //http连接是否要保持
    int m_content_length;

    char m_real_file[ FILENAME_LEN ];       // 客户请求的目标文件的完整路径，其内容等于 doc_root + m_url, doc_root是网站根目录
    struct stat m_file_stat;
    char * m_file_address;


    bool add_status_line(int status,const char* status_title);
    bool add_response(const char*format,...);
    bool add_heads(int content_lentgth);
    bool add_content_length(int content_length);
    bool add_content_type();
    bool add_linger();
    bool add_blank_line();
    bool add_content(const char *);
    void unmap();

    int bytes_to_send; //将要发送的数据的字节数
    int bytes_have_send;
    //util_timer* time;    //定时器

};
#endif