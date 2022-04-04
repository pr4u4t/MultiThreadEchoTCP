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
#include <semaphore.h>
#include <fcntl.h>

#define USAGE   printf("Usage %s -p port -m length -t threads -b connections -q termination\r\n",argv[0]); \
                printf("-p\t\tnumber of listening port (integer)\r\n"); \
                printf("-m\t\tmaximal message buffer length (integer)\r\n"); \
                printf("-t\t\tsize of worker thread pool (integer)\r\n"); \
                printf("-b\t\tmaximal size of waiting connections buffer (integer)\r\n"); \
                printf("-q\t\tconnection termination character (char)\r\n");

#define BACKLOG 5
#define DEFAULT_PORT 8081
#define DEFAULT_LENGTH 4096
#define DEFULT_NUMBER_OF_THREADS 2
#define DEFAULT_CONNECTION_BUFFER 32
#define DEFAULT_TERMINATION_CHARACTER 'q'
#define SERVER_LOG "./server.log"
#define _DEBUG_ 0

typedef struct _ClientEntry ClientEntry;
struct _ClientEntry {
    int _fd;
    TAILQ_ENTRY(_ClientEntry) entries;
};


typedef TAILQ_HEAD(Clients,_ClientEntry) Queue;

typedef struct _ServerOptions ServerOptions;
struct _ServerOptions{
    int _terminator;
    int _max_length;
    int _thread_pool;
    int _connection_buffer;
    int _port;
    const char* _log;
    int _lfd[2];
    int _logfd;
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
    sem_t* _semaphore;
    pthread_mutex_t _lock;
    Queue _connections;
    volatile sig_atomic_t _size;
    int _max_size;
};

static Server Server_create(ServerOptions *options);

bool Server_setup(Server *srv);

void Server_run(Server *srv,ConnectionsQueue *queue);

void* Server_client_handler(void* data);

void* Server_log_handler(void* data);

ConnectionsQueue ConnectionsQueue_create(int size);

bool ConnectionsQueue_push(ConnectionsQueue *queue, int fd);

int ConnectionsQueue_pop(ConnectionsQueue *queue);

//-------------------------------------------------------------------------------------

int main(int argc, char **argv){
    ConnectionsQueue queue;
    ServerOptions opts;
    Server srv;
    int c;
    
    opts._terminator = DEFAULT_TERMINATION_CHARACTER;
    opts._max_length = DEFAULT_LENGTH;
    opts._thread_pool = DEFULT_NUMBER_OF_THREADS;
    opts._connection_buffer = DEFAULT_CONNECTION_BUFFER;
    opts._port = DEFAULT_PORT;
    opts._log = SERVER_LOG;
    
    while ((c = getopt (argc, argv, "p:m:t:b:c:q:h")) != -1){
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

    //create server data structure
    srv = Server_create(&opts);
    //create queue data structure
    queue = ConnectionsQueue_create(opts._connection_buffer);
    
    //setup server and start listening
    if(!Server_setup(&srv)){
        exit(EXIT_FAILURE);
    }

    //run server
    Server_run(&srv,&queue);

    return 0;
}

Server Server_create(ServerOptions *opts){
    Server ret = { 0 };
    
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
    
    if(!srv){
        return false;
    }
    
    //open log file descriptor
    if(!(srv->_opts->_logfd = open(srv->_opts->_log,O_APPEND | O_CREAT))){
        perror("failed to open log file");
        return false;
    }
    
    if(pipe(srv->_opts->_lfd) < 0 ){
        perror("failed to open pipe");
        return false;
    }
    
    // Creating socket file descriptor
    if ((srv->_server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
        perror("socket failed");
        return false;
    }
    
    // Forcefully attaching socket to the port
    if (setsockopt(srv->_server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,&opt, sizeof(opt))){
        perror("setsockopt");
        return false;
    }
    
    // Forcefully attaching socket to the port
    if (bind(srv->_server_fd, (struct sockaddr *)&srv->_address, sizeof(srv->_address)) < 0){
        perror("bind failed");
        return false;
    }
    
    // Listen for incoming connections
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
        printf("new connection accepted\r\n");
        ConnectionsQueue_push(queue,new_fd);
    }
}

void* Server_client_handler(void* data){
    ServerOptions* opts = (ServerOptions*) data;
    char buffer[opts->_max_length];
    //this variable preserves value across calls
    static int thread_id;
    ssize_t size;
    int fd;

    
    printf("started new worker thread #%d\r\n",thread_id++);
    
    for(;;){
        printf("worker thread #%d waiting for connection\r\n",thread_id);
        if((fd = ConnectionsQueue_pop(data)) <= 0){
            continue;
        }
        
        printf("got connection\r\n");
        for(;;){
            if((size = recv(fd, buffer, opts->_max_length, 0)) > 0){
                send(fd, buffer, size, 0);
            }else{
                printf("connection lost\r\n");
                break;
            }
        }
    }
    
    return 0;
}

void* Server_log_handler(void* data){
    ServerOptions* opts = (ServerOptions*) data;
    char buffer[opts->_max_length];
    ssize_t size;
    
    for(;;){
        if((size = read(opts->_lfd[1],buffer,opts->_max_length))){
            write(opts->_logfd,buffer,size);
            fsync(opts->_logfd);
        }
    }
    
    return 0;
}

ConnectionsQueue ConnectionsQueue_create(int size){
    ConnectionsQueue ret = {0};
    
    ret._size = 0;
    ret._max_size = size;
    
    if(pthread_mutex_init(&ret._lock, 0) != 0){
        return ret;
    }
    
    if ((ret._semaphore = sem_open("queue_counter", O_CREAT, 0644, 1)) == SEM_FAILED) {
        perror("semaphore initilization");
        return ret;
    }
    
    if(pthread_mutex_init(&ret._lock,0) != 0){
        perror("thread obtain lock");
        return ret;
    }
    printf("initialize queue\r\n");

    TAILQ_INIT(&ret._connections);
    
    return ret;
}

bool ConnectionsQueue_push(ConnectionsQueue *queue,int fd){
    ClientEntry *elem;
    if(queue->_size >= queue->_max_size){
        return false;
    }
    
    //prepare new element allocate memory and set
    if((elem = malloc(sizeof(ClientEntry)))){
        memset(elem,0,sizeof(ClientEntry));
        elem->_fd = fd;
    } else return false;
    
    //CRITICAL SECTION THAT NEEDS SYNCHRONIZATION
    if(pthread_mutex_lock(&queue->_lock) != 0 ){
        return false;
    }
    
    TAILQ_INSERT_HEAD(&queue->_connections, (struct _ClientEntry*) elem, entries);
    ++queue->_size;
    
    pthread_mutex_unlock(&queue->_lock);
    sem_post(queue->_semaphore);
    
    return true;
}

int ConnectionsQueue_pop(ConnectionsQueue *queue){
    ClientEntry *elem;
    int ret;
    
    if(sem_wait(queue->_semaphore) != 0){
        return -1;
    }
    
    //CRITICAL SECTION THAT NEEDS SYNCHRONIZATION
    if(pthread_mutex_lock(&queue->_lock) != 0 ){
        return -1;
    }
    
    elem = TAILQ_LAST(&queue->_connections,Clients);
    
    TAILQ_REMOVE(&queue->_connections, (struct _ClientEntry*) elem, entries);
    
    --queue->_size;
    
    pthread_mutex_unlock(&queue->_lock);
    ret = elem->_fd;
    free(elem);
        
    return ret;
}
