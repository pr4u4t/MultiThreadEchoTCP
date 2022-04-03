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
#include <string.h>

#define USAGE printf("Usage %s -p port\r\n",argv[0]);

#define BACKLOG 5
#define DEFAULT_PORT 8081
#define DEFAULT_LENGTH 4096
#define DEFULT_NUMBER_OF_THREADS 32
#define DEFAULT_CONNECTION_BUFFER 32
#define DEFAULT_TERMINATION_CHARACTER 'q'


typedef struct _ClientEntry ClientEntry;
struct _ClientEntry {
    int _fd;
    LIST_ENTRY(_ClientEntry) entries;
};

typedef LIST_HEAD(tailhead, _ClientEntry) Queue;

typedef struct _ServerOptions ServerOptions;
struct _ServerOptions{
    int _terminator;
    int _max_length;
    int _thread_pool;
    int _connection_buffer;
    int _port;
};

typedef struct _Server Server;
struct _Server{
    struct sockaddr_in _address;
    int _server_fd;
    int _addrlen;
    volatile sig_atomic_t _run;
    ServerOptions* _opts;
    pthread_t *_pool;
};

typedef struct _ConnectionsQueue ConnectionsQueue;
struct _ConnectionsQueue{
    pthread_rwlock_t _rwlock;
    Queue _connections;
    int _size;
};

static Server Server_create(ServerOptions *options);

bool Server_setup(Server *srv);

void Server_run(Server *srv,ConnectionsQueue *queue);

void* Server_client_handler(void* data);

ConnectionsQueue ConnectionsQueue_create(int size);

bool ConnectionsQueue_push(ConnectionsQueue *queue, int fd);

int ConnectionsQueue_pop(ConnectionsQueue *queue);

//-------------------------------------------------------------------------------------

int main(int argc, char **argv){
    Server srv;
    ServerOptions opts;
    ConnectionsQueue queue;
    
    int c;
    
    if(argc == 1){
        USAGE
        return 1;
    }
    
    opts._terminator = DEFAULT_TERMINATION_CHARACTER;
    opts._max_length = DEFAULT_LENGTH;
    opts._thread_pool = DEFULT_NUMBER_OF_THREADS;
    opts._connection_buffer = DEFAULT_CONNECTION_BUFFER;
    opts._port = DEFAULT_PORT;
    
    while ((c = getopt (argc, argv, "p:")) != -1){
        switch (c){
            case 'p':
                opts._port = atoi(optarg);
                break;
            case 'm':
                opts._max_length = atoi(optarg);
                break;
            case 't':
                opts._thread_pool = atoi(optarg);
                break;
            case 'b':
                opts._connection_buffer = atoi(optarg);
                break;
            case 'c':
                opts._terminator = atoi(optarg);
                break;
            case 'q':
                opts._terminator = atoi(optarg);
                break;
            case 'h':
                USAGE
                return 1;
            default:
                USAGE
                return 1;
        }
    }
    
    srv = Server_create(&opts);
    
    if(!Server_setup(&srv)){
        exit(EXIT_FAILURE);
    }
    
    Server_run(&srv,&queue);

    return 0;
}

Server Server_create(ServerOptions *opts){
    Server ret;
    ret._addrlen = sizeof(ret._address);
    ret._address.sin_family = AF_INET;
    ret._address.sin_addr.s_addr = INADDR_ANY;
    ret._address.sin_port = htons(opts->_port);
    ret._opts = opts;
    ret._run = true;
    
    if((ret._pool = malloc(sizeof(pthread_t)*opts->_thread_pool))){
        memset(ret._pool,0,sizeof(pthread_t)*opts->_thread_pool);
    }
    
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

void Server_run(Server *srv,ConnectionsQueue *queue){
    int new_fd;
       
    if(!srv || !queue){
        return;
    }
    
    for(int i = 0; i < srv->_opts->_thread_pool;++i){
        pthread_create(&srv->_pool[i], NULL, Server_client_handler, queue);
    }
    
    while(srv->_run){
        if ((new_fd = accept(srv->_server_fd, (struct sockaddr *)&srv->_address, (socklen_t*)&srv->_addrlen)) < 0){
            perror("accept");
            continue;
        }
        
        
        
    }
}

void* Server_client_handler(void* data){
    
    for(;;){
        
        
        
    }
    
    return 0;
}

ConnectionsQueue ConnectionsQueue_create(int size){
    ConnectionsQueue ret;
    if(pthread_rwlock_init(&ret._rwlock,0) != 0){
        return ret;
    }
    
    return ret;
}

bool ConnectionsQueue_push(ConnectionsQueue *queue,int fd){
    ClientEntry *elem;
    
    if(pthread_rwlock_trywrlock(&queue->_rwlock) != 0){
        perror("thread obtain lock");
        return false;
    }
    
    
    //prepare new element allocate memory and set
    if((elem = malloc(sizeof(ClientEntry)))){
        memset(elem,0,sizeof(ClientEntry));
        elem->_fd = fd;
    } else return false;
    
    LIST_INSERT_HEAD(&queue->_connections, (struct _ClientEntry*) elem, entries);
    //unlock
    return true;
}

int ConnectionsQueue_pop(ConnectionsQueue *queue){
    return 0;
}
