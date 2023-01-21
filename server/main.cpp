#include "server.h"

Server* server;

//exit the server
void signalHandler(int signal) {
    delete server;
    exit(0);
}

int main(int argc, char** argv) {
    signal(SIGINT, signalHandler);

    // prevent dead sockets from throwing pipe errors on write
    signal(SIGPIPE, SIG_IGN);

    // init server
    server = new Server();

    //the server will work until we turn it off
    server->loop();
}
