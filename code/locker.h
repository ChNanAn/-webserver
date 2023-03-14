#ifndef LOCKER_H
#define LOCKER_H
//线程同步机制封装类
#include<pthread.h>
#include<exception>
#include<semaphore.h>


//互斥锁类
class locker{
    private:
    pthread_mutex_t m_mutex;

    public:
    locker()
    {
        if(pthread_mutex_init(&m_mutex,NULL)!=0)
        {
        throw std::exception();
        }
    }
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex)==0;

    }
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex)==0;

    }
    pthread_mutex_t * get()
    {
        return &m_mutex;
    }
};

//条件变量类
class cond
{
    public:
    cond()
    {
        if(pthread_cond_init(&m_cond,NULL)!=0)
         throw std::exception();
    }
    ~cond()
    {
        pthread_cond_destroy(&m_cond);

    }
    bool wait(pthread_mutex_t* mutex)
    {
        return pthread_cond_wait(&m_cond,mutex)==0;
    }
    bool timedwait(pthread_mutex_t* mutex, struct timespec* t)
    {
        return pthread_cond_timedwait(&m_cond,mutex,t)==0;
    }
    bool signal()
    {
        return pthread_cond_signal(&m_cond)==0;
    }

    private:
    pthread_cond_t m_cond;

};

//信号量类

class sem
{
    public:
    sem()
    {
        if(sem_init(&m_sem,0,0)!=0)
        throw std::exception();

    }
    sem(unsigned nums)
    {
        if(sem_init(&m_sem,0,nums)!=0)
        throw std::exception();
    }
    ~sem()
    {
        sem_destroy(&m_sem);
    }
    bool wait()
    {
        return sem_wait(&m_sem)==0;

    }
    bool post()
    {
        return sem_post(&m_sem)==0;
    }
    private:
    sem_t m_sem;
};


#endif
