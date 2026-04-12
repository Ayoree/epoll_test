#define MAX_POLL_EVENTS 64

int epoll;
int server_sock;

atomic_int clients_count = 0;

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
    const int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
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
    const int epoll = epoll_create1(0);
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
    const int n = epoll_wait(epoll, events, size, -1);
    if (n == -1) {
        perror("epoll_wait error");
        exit(EXIT_FAILURE);
    }
    return n;
}

void process_command(const unsigned char* command)
{
    printf("Command: %s\n", command);
    fflush(stdout);
}

int process_client(int sock)
{
    unsigned char buf[8];
    unsigned char command[32] = {0};
    bool is_start = true;
    bool is_command = false;
    unsigned short command_len = 0;

    for (;;)
    {
        int readed = recv(sock, buf, sizeof(buf), 0);
        if (readed == 0)
        {
            printf("Client disconnected\n");
            fflush(stdout);
            return 1;
        }
        else if (readed == -1)
        {
            if (errno == EAGAIN)
                break; 
            perror("Can't read from socket");
            return 1;
        }

        struct string_view sv = {
            .ptr = buf,
            .len = readed
        };

        while (sv.ptr < buf + readed)
        {
            // proccess lines
            if (is_start)
            {
                is_command = *sv.ptr == '/';
                if (is_command)
                    memset(command, 0, sizeof(command));
                is_start = false;
            }
    
            unsigned char* nl_ptr = memchr(sv.ptr, '\n', sv.len);

            // echo
            if (!is_command)
            {
                if (nl_ptr == NULL)
                {
                    send(sock, sv.ptr, sizeof(*buf) * sv.len, MSG_NOSIGNAL);
                    break;
                }
                else
                {
                    const unsigned int to_send_count = nl_ptr - sv.ptr + 1;
                    //const unsigned int new_len = sv.len - to_send_count;
                    sv.len = to_send_count;
                    send(sock, sv.ptr, sizeof(*buf) * sv.len, MSG_NOSIGNAL);
                    sv.ptr = nl_ptr + 1;
                    is_start = true;
                    command_len = 0;
                    continue;
                }
            }

            // command
            const unsigned short remaining_command_space = sizeof(command) - command_len - 1;
            if (remaining_command_space == 0)
            {
                printf("Command is too big. Skipped\n");
                break;
            }
            if (nl_ptr == NULL)
            {
                //const unsigned int old_len = sv.len;
                sv.len = remaining_command_space < readed ? remaining_command_space : readed;
                
                memcpy(command + command_len, sv.ptr, sv.len);
                command_len += sv.len;
                sv.ptr += sv.len;
                break;
            }
            else
            {
                const unsigned int remaining_to_nl = nl_ptr - sv.ptr;
                const unsigned int to_add_count = remaining_command_space < remaining_to_nl ? remaining_command_space : remaining_to_nl;
                //const unsigned int new_len = sv.len - to_add_count;
                sv.len = to_add_count;
                memcpy(command + command_len, sv.ptr, sv.len);
                process_command(command);
                sv.ptr += sv.len + 1;
                is_start = true;
                command_len = 0;
                continue;
            }
        }
        //printf("Readed: %.*s", readed, buf);
        //fflush(stdout);
    }
    return 0;
}

void* worker(void* args)
{
    struct epoll_event events[MAX_POLL_EVENTS];
    int thread_id = *(int*)args;
    free(args);

    printf("Started new thread %d\n", thread_id);
    fflush(stdout);

    for (;;)
    {
        int n = wait_epoll(epoll, events, MAX_POLL_EVENTS);
        for (int i = 0; i < n; ++i)
        {
            const int cur_sock = events[i].data.fd;
            if (cur_sock == server_sock)
            {
                int client_sock = accept4(server_sock, NULL, NULL, SOCK_NONBLOCK);
                if (client_sock == -1) {
                    if (errno == EAGAIN)
                        continue;
                    perror("Can't accept new client");
                    continue;
                }
                atomic_fetch_add(&clients_count, 1);
                add_sock_to_epoll(epoll, client_sock, EPOLLIN | EPOLLET);
                printf("New client connected. Thread ID: %d\n", thread_id);
                fflush(stdout);
            }
            else
            {
                int res = process_client(cur_sock);
                if (res != 0)
                {
                    epoll_ctl(epoll, EPOLL_CTL_DEL, cur_sock, NULL);
                    close(cur_sock);
                    atomic_fetch_sub(&clients_count, 1);
                }
            }
        }
    }
}

int main(int argc, char* argv[])
{
    int max_clients = 1024;
    int threads_count = 1;
    
    read_args(argc, argv, &max_clients, &threads_count);
    printf("Started. Max clients %d, threads %d\n", max_clients, threads_count);

    server_sock = create_server_socket();
    printf("Server socket created (fd: %d)\n", server_sock);

    epoll = create_epoll();
    add_sock_to_epoll(epoll, server_sock, EPOLLIN);

    pthread_t* threads = malloc(threads_count * sizeof(pthread_t));

    for (int i = 0; i < threads_count; ++i)
    {
        int* thr_id = malloc(sizeof(int));
        *thr_id = i;
        pthread_create(&threads[i], NULL, worker, thr_id);
    }
    for (int i = 0; i < threads_count; ++i)
        pthread_join(threads[i], NULL);

    close(server_sock);
    free(threads);

    return 0;
}