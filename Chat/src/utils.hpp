#pragma once

#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <fstream>
#include <vector>

inline int setnonblocking(int sock)
{
    int flags = fcntl(sock, F_GETFL, 0);
    return fcntl(sock, F_SETFL, flags|O_NONBLOCK);
}

inline void addfd(int sock, int epoll)
{
    epoll_event ev;
    ev.data.fd = sock;
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;
    setnonblocking(sock);
    epoll_ctl(epoll, EPOLL_CTL_ADD, sock, &ev);
}

inline void modfd(int sock, int epoll, int ev)
{
    epoll_event event;
    event.data.fd = sock;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epoll, EPOLL_CTL_MOD, sock, &event);
}

inline void removefd(int epollfd, int sock)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, sock, 0);
    close(sock);
}

inline std::string get_content(std::string const &path)
{
    std::ifstream file(path);
    if(!file.is_open()){
        return "not found";
    }
    std::string content{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>{}};
    return content;
}

inline void file_put_content(std::string const &path, std::string_view content) 
{
    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::system_error(errno, std::generic_category());
    }
    std::copy(content.begin(), content.end(), std::ostreambuf_iterator<char>(file));
}

inline void lTrim(std::string& str)
{
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](char ch){ return !isspace(ch); }));
}
