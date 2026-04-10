#define MAX_POLL_EVENTS 64

void read_args(int argc, char* argv[], int* max_clients, int* threads)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0)
    {
        printf("Usage: [max_clients=1024] [threads=1]\n");
        exit(EXIT_SUCCESS);
    }

    if (argc == 1)
        printf("No argumets is provided. Using default values. (try `--help` for usage)\n");

    if (argc >= 2)
        *max_clients = abs(atoi(argv[1]));
    
    if (*max_clients == 0)
    {
        fprintf(stderr, "Zero clients?\n");
        exit(EXIT_FAILURE);
    }

    if (argc >= 3)
        *threads = abs(atoi(argv[2]));
    
    if (*threads == 0)
    {
        fprintf(stderr, "Zero threads?\n");
        exit(EXIT_FAILURE);
    }
}

int create_server_socket()
{
    int sock = socket(AF_INET, SOCK_STREAM /*| SOCK_NONBLOCK*/, 0);
    if (sock == -1)
    {
        perror("Can't create server socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in local = {
        .sin_family = AF_INET,
        .sin_port = htons(6969)
    };
    inet_pton(AF_INET, "127.0.0.1", &local.sin_addr);

    if (bind(sock, (void*)&local, sizeof(local)) == -1)
    {
        perror("Can't bind server socket");
        close(sock);
        exit(EXIT_FAILURE);
    }

    if (listen(sock, 128) == -1)
    {
        perror("Can't listen server socket");
        close(sock);
        exit(EXIT_FAILURE);
    }
    return sock;
}

int create_epoll()
{
    int epoll = epoll_create1(0);
    if (epoll == -1)
    {
        perror("Can't create epoll");
        exit(EXIT_FAILURE);
    }
    return epoll;
}

void add_sock_to_epoll(int epoll, int sock, uint32_t events)
{
    struct epoll_event ev = {
        .data.fd = sock,
        .events = events
    };
    if (epoll_ctl(epoll, EPOLL_CTL_ADD, sock, &ev) == -1) {
        perror("Can't add socket to epoll");
        exit(EXIT_FAILURE);
    }
}

int wait_epoll(int epoll, struct epoll_event* events, size_t size)
{
    int n = epoll_wait(epoll, events, size, -1);
    if (n == -1) [[unlikely]] {
        perror("epoll_wait error");
        exit(EXIT_FAILURE);
    }
    return n;
}

void process_client(int sock)
{

}

int main(int argc, char* argv[])
{
    int max_clients = 1024;
    int threads = 1;
    int epoll;
    int server_sock;

    unsigned char buf[1024];
    struct epoll_event ev, events[MAX_POLL_EVENTS];
    
    read_args(argc, argv, &max_clients, &threads);
    printf("Started. Max clients: %d, threads %d\n", max_clients, threads);

    server_sock = create_server_socket();
    printf("Server socket created: %d\n", server_sock);

    epoll = create_epoll();
    add_sock_to_epoll(epoll, server_sock, EPOLLIN);

    for (;;)
    {
        int nsocks = wait_epoll(epoll, events, MAX_POLL_EVENTS);
        for (int i = 0; i < nsocks; ++i)
        {
            const int cur_sock = events[i].data.fd;
            if (cur_sock == server_sock)
            {
                int client_sock = accept(server_sock, NULL, NULL);
                if (client_sock == -1) {
                    perror("Can't accept new client");
                    continue;
                }
                setnonblocking(client_sock);
                add_sock_to_epoll(epoll, client_sock, EPOLLIN | EPOLLET);
            }
            else
                process_client(cur_sock);
        }
    }
    int client_sock = accept(server_sock, NULL, NULL);
    printf("Client connected: %d\n", client_sock);

    shutdown(server_sock, SHUT_WR);
    while (recv(server_sock, buf, sizeof(buf), 0) > 0);
    close(server_sock);

    return 0;
}