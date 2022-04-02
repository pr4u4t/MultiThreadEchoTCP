#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <pthread.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <sys/queue.h>

#define USAGE printf("Usage %s -p port\r\n",argv[0]);
#define BACKLOG 5
#define DEFAULT_PORT 8081
#define DEFAULT_LENGTH 4096
#define DEFULT_NUMBER_OF_THREADS 32
#define DEFAULT_CONNECTION_BUFFER 32



typedef struct _Server Server;
struct _Server{
    struct sockaddr_in _address;
    short _port;
    int _server_fd;
    int _addrlen;
    volatile sig_atomic_t _run;
};

typedef struct _Client Client; 
struct _Client{
    int _fd;
    Server *_srv;
};

static Server Server_create(int port);

bool Server_setup(Server *srv);

void Server_run(Server *srv);

void* Server_client_handler(void* data);

int main(int argc, char **argv){
    Server srv;
    int c;
    
    if(argc == 1){
        USAGE
        return 1;
    }
    
    while ((c = getopt (argc, argv, "p:")) != -1){
        switch (c){
            case 'p':
                srv = Server_create(atoi(optarg));
                break;
            case 'm':    
                break;
            case 'h':
                USAGE
                return 1;
            default:
                USAGE
                return 1;
        }
    }
    
    if(!Server_setup(&srv)){
        exit(EXIT_FAILURE);
    }
    
    Server_run(&srv);

    return 0;
}

Server Server_create(int port){
    Server ret;
    
    ret._addrlen = sizeof(ret._address);
    ret._address.sin_family = AF_INET;
    ret._address.sin_addr.s_addr = INADDR_ANY;
    ret._address.sin_port = htons( port );
    ret._run = true;
    
    return ret;
}

bool Server_setup(Server *srv){
    int opt = 1;
    
    if(srv){
        return false;
    }
    
    // Creating socket file descriptor
    if ((srv->_server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
        perror("socket failed");
        return false;
    }
    
    // Forcefully attaching socket to the port 8080
    if (setsockopt(srv->_server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,&opt, sizeof(opt))){
        perror("setsockopt");
        return false;
    }
    
    // Forcefully attaching socket to the port 8080
    if (bind(srv->_server_fd, (struct sockaddr *)&srv->_address, sizeof(srv->_address)) < 0){
        perror("bind failed");
        return false;
    }
    
    if (listen(srv->_server_fd, BACKLOG) < 0){
        perror("listen");
        return false;
    }
    
    return true;
}

void Server_run(Server *srv){
    int new_fd;
    
    if(!srv){
        return;
    }
    
    while(srv->_run){
        if ((new_fd = accept(srv->_server_fd, (struct sockaddr *)&srv->_address, (socklen_t*)&srv->_addrlen)) < 0){
            perror("accept");
            continue;
        }
        
        pthread_t tid;
        Client *clt;
        if((clt = malloc(sizeof(Client)))){
            pthread_create(&tid, NULL, Server_client_handler, 0);
        }
    }
}

void* Server_client_handler(void* data){
    
    return 0;
}

