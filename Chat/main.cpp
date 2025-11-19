#include "./src/http.hpp"
#include "./src/server.hpp"

int main(){
    Server server(8888);
    server.init_server();
    server.server_loop();
}
