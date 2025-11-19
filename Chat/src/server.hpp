#pragma once

#include <sys/epoll.h>
#include <unistd.h>
#include <unordered_map>
#include "./http.hpp"
#include "./utils.hpp"
#include "./threadpool.hpp"
#include "./timer.hpp"

#define TIMEOUT 6

const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUM = 10000;    //最大事件数


class Server{
int m_port;
int listen_sock;
int m_epoll;
epoll_event events[MAX_EVENT_NUM];
std::unordered_map<int, http_user> users;
std::unordered_map<int, user_timer*> timers;
ThreadPool *threadpool;

Utils utils;
int m_pipe[2];
public:
    Server(int port):m_port(port){
        threadpool = new ThreadPool(10);
    }
    ~Server(){
        delete threadpool;
    }
    void init_server(){
        listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in server_addr;
        server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(8888);
        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        bind(listen_sock, (sockaddr*)&server_addr, sizeof(server_addr));
        listen(listen_sock, 5);
        
        m_epoll = epoll_create1(0);
        setnonblocking(listen_sock);
        epoll_event event;
        event.data.fd = listen_sock;
        event.events = EPOLLIN|EPOLLET|EPOLLRDHUP;
        epoll_ctl(m_epoll, EPOLL_CTL_ADD, listen_sock, &event);

        utils.init(TIMEOUT);
        socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipe);
        //pipe(m_pipe);
        setnonblocking(m_pipe[1]);
        utils.addfd_notoneshot(m_epoll, m_pipe[0]);
        utils.addsig(SIGPIPE, SIG_IGN);
        utils.addsig(SIGALRM, utils.sig_handler);
        utils.addsig(SIGTERM, utils.sig_handler);

        Utils::u_epollfd = m_epoll;
        Utils::u_pipefd = m_pipe;

        alarm(TIMEOUT);

    }

    void server_loop(){
        bool timeout = false;
        bool stop_server = false;
        while(1){
            int idxfd = epoll_wait(m_epoll, events, MAX_EVENT_NUM, -1);
            if(idxfd < 0 && errno != EINTR){
                perror("epoll:");
                break;
            }

            for(int i = 0; i < idxfd; i++){
                int client = events[i].data.fd;
                if(client == listen_sock){
                    accept_client();
                    continue;
                }
                else if(client == m_pipe[0] && (events[i].events & EPOLLIN)){
                    //std::cout<<"timeout"<<std::endl;
                    dealwithsignal(timeout, stop_server);
                }
                else if(events[i].events & EPOLLIN){
                    if (!timers[client]){
                        adjust_timer(timers[client]);
                    }

                    if(users[client].read()){
                        users[client].request.parse();
                        threadpool->enques([client](http_user* user){
                            user->send_response();
                            modfd(client, user->m_epoll, EPOLLIN|EPOLLOUT);
                        }, &users[client]);
                    }
                }
                else if(events[i].events & EPOLLOUT){
                    if (!timers[client]){
                        adjust_timer(timers[client]);
                    }
                    users[client].write();
                }
                else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                    removefd(m_epoll, client);
                    user_timer* timer = timers[client];
                    utils.m_timer_lst.del_timer(timer);
                    timers.erase(client);
                    delete timer;
                    users.erase(client);
                }
            }
            if(timeout){
                std::vector<int> lst = utils.timer_handler();
                for(auto c : lst){
                    removefd(m_epoll, c);
                    timers.erase(c);
                    users.erase(c);
                }
                timeout = false; 
            }
        }

    }

private:
    bool accept_client(){
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        while(true){
            int client_sock = accept(listen_sock, (sockaddr*)&client_addr, &client_len);
            if((client_sock < 0)) break;
            if(users.size() >= MAX_FD) break;

            setnonblocking(client_sock);
            addfd(client_sock, m_epoll);
            users.try_emplace(client_sock);
            users[client_sock].init(client_sock, m_epoll);

            user_timer *timer = new user_timer;
            timer->expire = time(nullptr) + 2 * TIMEOUT;
            timer->m_sock = client_sock;
            timers[client_sock] = timer;
            utils.m_timer_lst.add_timer(timer);
        }
        return true;
    }
    void adjust_timer(user_timer *timer)
    {
        time_t cur = time(NULL);
        timer->expire = cur + 2 * TIMEOUT;
        utils.m_timer_lst.adjust_timer(timer);
    }
    bool dealwithsignal(bool &timeout, bool &stop_server)
    {
        int ret = 0;
        unsigned char sig;
        ret = recv(m_pipe[0], &sig, sizeof(int), 0);
        if (ret == -1)
        {
            return false;
        }
        else if (ret == 0)
        {
            return false;
        }
        else
        {
            switch (sig)
            {
                case SIGALRM:
                {
                    timeout = true;
                    break;
                }
                case SIGTERM:
                {
                    stop_server = true;
                    break;
                }
            }
        }
        return true;
    }
};