#pragma once

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ostream>
#include <sstream>
#include "./utils.hpp"
#include "./json.hpp"

#define MAX_MESSAGE 1000

class http_parse{
public:
std::string m_header;
std::string m_body;
std::string m_resource = "../static";

size_t m_content_length = 0;
std::string m_method;
std::string m_path;
std::string m_version;
std::string m_connection = "";
std::string m_host;
std::string read_buf;

    void reset(){
    m_header = "";
    m_body = "";
    m_content_length = 0;
    m_method = "";
    m_path = "";
    m_version = "";
    m_connection = "";
    m_host = "";
    read_buf = "";
    }

    void parse(){
        size_t header_len = read_buf.find("\r\n\r\n");
        m_body = read_buf.substr(header_len + 4);
        m_header = read_buf.substr(0, header_len);
        //parse
        m_version = m_header.substr(0, m_header.find("\r\n"));
        m_method = m_version.substr(0,m_version.find_first_of(" \t"));
        m_path = m_version.substr(m_version.find_first_of(" \r")+1);
        m_version = m_path.substr(m_path.find_first_of(" \r")+1, m_path.find("\r\n"));
        m_path.resize(m_path.find_first_of(" \r"));
        //std::cout<<m_header<<std::endl;
        //std::cout<<m_method<<std::endl;
        //std::cout<<m_path<<std::endl;

        if(m_header.find("Content-Length:") != std::string::npos){
            std::string content_length = m_header.substr(m_header.find("Content-Length:")+15);
            content_length = content_length.substr(0, content_length.find("\r\n"));
            m_content_length = stoi(content_length);
        }
        if(m_header.find("Connection:") != std::string::npos){
            std::string connection = m_header.substr(m_header.find("Connection:")+11);
            connection = connection.substr(0, connection.find("\r\n"));
            lTrim(connection);
            m_connection = connection;
        }
        if(m_header.find("Host:") != std::string::npos){
            std::string host = m_header.substr(m_header.find("Host:")+5);
            host = host.substr(0, host.find("\r\n"));
            lTrim(host);
            m_host = host;
        }
    }
};

class http_user{
public:
    static std::deque<std::string> Message; 
    static ssize_t mes_del;
    int m_index;

    int m_sockfd;
    int m_epoll;


    http_parse request;
    std::string response;

    void init(int sock, int epoll){
        m_sockfd = sock;
        m_epoll = epoll;

    }



    void reset(){
        removefd(m_epoll, m_sockfd);
    }

    bool read(){
        char buf[2048];
        while(true){
            int recv_len = recv(m_sockfd, buf, sizeof(buf), 0);
            if(recv_len < 0 ){
                if(errno == EAGAIN||errno == EWOULDBLOCK){
                    break;
                }
                else{
                    return false;
                }
            }
            else if(recv_len == 0){
                return false;
            }
            else{
                request.read_buf.append(buf, recv_len);
            }
        }
        return true;
    }

    void send_response(){
        std::string res = "";
        //页面
        if(request.m_path == "/"){
            std::string path = request.m_resource + request.m_path; 
            path += "index.html";
            //std::cout<<path<<std::endl;
            std::string content = get_content(path);
            res += write_response(200, content);
        }
        //POET消息
        else if(request.m_path == "/send"){
            res += write_response(200, "收到了");
            if(Message.size() > MAX_MESSAGE){
                Message.pop_front();
                mes_del++;
            }
            else{
                Message.emplace_back(request.m_body);
            }
        }
        else if(request.m_path == "/recv"){
            auto [json, _] = parse(request.m_body);
            m_index = json["first"];
            res += write_response(200,recv_message());
        }
        else{
            std::string path = request.m_resource + request.m_path;
            //std::cout<<path<<std::endl;
            std::string content = get_content(path);
            res += write_response(200, content);
        }
        
        if(res == "not found")
            res += send_err_message();
        response = res;        
    }

std::string write_response(int code, std::string const content)
    {
        // std::cout<< request.m_connection <<std::endl;
        std::ostringstream resp;
        resp << "HTTP/1.1 "<< code << " OK\r\n"
             << "Server: chatserver\r\n";
        if(request.m_connection == "")
            resp << "Connection: close\r\n";
        else
            resp << "Connection: " << request.m_connection.c_str() << "\r\n";
        // << "Connection-type: text/html\r\n"
        resp << "Content-Length: " << content.size() << "\r\n"
             << "\r\n";
        if(request.m_method != "HEAD")
             resp <<  content;

        std::string res = resp.str();
        //std::cout<< "[" <<res<< "]" <<std::endl;
        return res;
    }

    std::string send_err_message()
    {
        std::string err_message = "HTTP/1.1 404\r\n"
        "Server: chatserver\r\n"
        "Connection: close\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: ";
        std::string err_html =  "<html><head><title>404</title></head>"
                                "<body><h1>404 Not Found</h1></body></html>";
        err_message += err_html.size();
        err_message += "\r\n\r\n";
        err_message += err_html;
        return err_message;
    }

    std::string recv_message()
    {
        //return "[]";
        ssize_t base = mes_del;
        if(m_index < base) m_index = base;
        size_t idx = m_index - base;
        if (idx >= Message.size()) return "[]";

        std::string res = "[";
        res += Message[idx++];
        while(idx < Message.size()){
            res += ',';
            res += Message[idx++];
        }
        res += ']';
        return res;
    }

    void write(){
        while(!response.empty()){
            int n = send(m_sockfd, response.c_str(), response.size(), 0);
            if(n > 0) response.erase(0, n);
            if(n < 0){
                if(errno == EAGAIN || errno == EWOULDBLOCK){
                    modfd(m_sockfd, m_epoll, EPOLLOUT);
                    return;//full
                }
                response.clear();//error
            }
            if(n == 0) break;//send finish
        }
        request.reset();
        shutdown(m_sockfd, SHUT_WR);
        modfd(m_sockfd, m_epoll, EPOLLIN);
    }
};

ssize_t http_user::mes_del = 0;
std::deque<std::string> http_user::Message;
