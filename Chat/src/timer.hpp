#pragma once

#include <vector>
#include <signal.h>
#include <string.h>
#include "./utils.hpp"

class user_timer{
public:
    user_timer():prev(nullptr),next(nullptr){}

    int m_sock;

    time_t expire;
    
    user_timer* prev;
    user_timer* next;

};

class timer_list{
public:
    timer_list(){
        head = nullptr;
        tail = nullptr;
    }
    ~timer_list(){
        user_timer* tmp = head;
        while(tmp){
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }

    void add_timer(user_timer* timer){
        if(!timer){
            return;
        }
        if(!head){
            head = timer;
            tail = timer;
            return; 
        }
        if(timer->expire < head->expire){
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }
        add_timer(timer, head);
    }

    void adjust_timer(user_timer* timer){
        if(!timer){
            return;
        }
        user_timer* tmp = timer->next;
        if(!tmp || (timer->expire < tmp->expire)) return;
        if(timer == head){
            head = head->next;
            head->prev = nullptr;
            timer->next = nullptr;
            add_timer(timer, head);
        }
        else{
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next);
        }
    }

    void del_timer(user_timer* timer){
        if(!timer){
            return;
        }
        if(timer == head&&timer == tail){
            delete timer;
            head =nullptr;
            tail = nullptr;
            return;
        }
        else if(timer == head){
            head = timer->next;
            head->prev = nullptr;
            delete timer;
            return;
        }
        else if(timer == tail){
            tail = timer->prev;
            tail->next = nullptr;
            return;
        }
        else{
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            delete timer;
            return;
        }
    }

    std::vector<int> tick(){
        if(!head){
            return {};
        }
        std::vector<int> del_lst;
        time_t cur = time(nullptr);
        user_timer* tmp = head;
        while(tmp){
            if(cur < tmp->expire){
                break;
            }
            del_lst.emplace_back(tmp->m_sock);
            head = tmp->next;
            if(head){
                head->prev = nullptr;
            }
            delete tmp;
            tmp = head;
        }
        return del_lst;
    }

private:
    void add_timer(user_timer* timer, user_timer* lst_timer){
        user_timer* prev = lst_timer;
        user_timer* tmp = prev->next;
        while(tmp){
            if(timer->expire < tmp->expire){
                timer->prev = prev;
                timer->next = tmp;
                prev->next = timer;
                tmp->prev = timer;
                break;
            }
            prev = tmp;
            tmp = tmp->next;
        }
        if(!tmp){
            prev->next = timer;
            timer->prev = prev;
            timer->next = nullptr;
            tail = timer;
        }
    }
    user_timer* head;
    user_timer* tail;
};

class Utils{
public:
    void init(int TIMEOUT){
        m_TIMEOUT = TIMEOUT;
    }
    void addfd_notoneshot(int epoll, int sock){
        epoll_event event;
        event.data.fd = sock;
        event.events = EPOLLIN | EPOLLRDHUP;
        epoll_ctl(epoll, EPOLL_CTL_ADD, sock, &event);
        setnonblocking(sock);
    }

    void addsig(int sig, void(handler)(int))
    {
        struct sigaction sa;
        memset(&sa, '\0', sizeof(sa));
        sa.sa_handler = handler;
        sigfillset(&sa.sa_mask);
        sigaction(sig, &sa, NULL);
    }

    static void sig_handler(int sig);

    std::vector<int> timer_handler(){
        std::vector<int> lst = m_timer_lst.tick();
        alarm(m_TIMEOUT);
        return lst;
    }

    timer_list m_timer_lst;
    static int* u_pipefd;
    static int u_epollfd;
    int m_TIMEOUT;
};

int* Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;
void Utils::sig_handler(int sig){
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (unsigned char *)&msg, 1, 0);
    errno = save_errno;
}
