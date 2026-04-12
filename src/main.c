#define MAX_POLL_EVENTS 64

const char* local_addr = "127.0.0.1";
const uint16_t local_port = 6969;

int epoll;
int server_sock;

atomic_uint_fast32_t current_clients_count = 0;
atomic_uint_fast32_t all_clients_count = 0;


void read_args(int argc, char* argv[], int* threads)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0)
    {
        printf("Usage: [threads=1]\n");
        exit(EXIT_SUCCESS);
    }

    if (argc == 1)
        printf("No argumets is provided. Using default values. (try `--help` for usage)\n");

    if (argc >= 2)
        *threads = abs(atoi(argv[1]));
    
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
    inet_pton(AF_INET, local_addr, &local.sin_addr);

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
    printf("Server running at %s:%u\n", local_addr, local_port);
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

int process_command(const char* command, int sock)
{
    if (strcmp(command, "/stats") == 0)
    {
        unsigned long a = atomic_load_explicit(&all_clients_count, memory_order_relaxed);
        unsigned long b = atomic_load_explicit(&current_clients_count, memory_order_relaxed);
        //send(sock, &a, sizeof(a), MSG_NOSIGNAL);
        //send(sock, &b, sizeof(b), MSG_NOSIGNAL);
        char buf[32] = {0};
        snprintf((void*)buf, sizeof(buf), "%lu\n", a);
        send(sock, buf, sizeof(*buf) * strlen(buf), MSG_NOSIGNAL);
        memset(buf, 0, sizeof(buf));
        snprintf((void*)buf, sizeof(buf), "%lu\n", b);
        send(sock, buf, sizeof(*buf) * strlen(buf), MSG_NOSIGNAL);
    }
    else if (strcmp(command, "/time") == 0)
    {
        time_t t = time(NULL);
        struct tm tm;
        char buf[20];
        localtime_r(&t, &tm);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        buf[19] = '\n';
        send(sock, buf, sizeof(buf), MSG_NOSIGNAL);
    }
    else if (strcmp(command, "/shutdown") == 0)
    {
        printf("Client %d shutdowned\n", sock);
        return 1;
    }
    else
    {
        printf("Received unknown command `%s` from %d\n", command, sock);
        fflush(stdout);
    }

    return 0;
}

int parse_user_input(int sock, char* buf, const int readed, struct user_input* ui)
{
    struct string_view sv = {
        .ptr = buf,
        .len = readed
    };

    while (sv.ptr < buf + readed)
    {
        if (ui->is_start)
        {
            ui->is_command = *sv.ptr == '/';
            if (ui->is_command)
                memset(ui->command, 0, sizeof(ui->command));
            ui->is_start = false;
        }

        char* nl_ptr = memchr(sv.ptr, '\n', sv.len);
        if (!ui->is_command)
        {
            if (nl_ptr == NULL)
            {
                send(sock, sv.ptr, sizeof(*buf) * sv.len, MSG_NOSIGNAL);
                break;
            }
            else
            {
                const unsigned int to_send_count = nl_ptr - sv.ptr + 1;
                sv.len = to_send_count;
                send(sock, sv.ptr, sizeof(*buf) * sv.len, MSG_NOSIGNAL);
                sv.ptr = nl_ptr + 1;
                ui->is_start = true;
                ui->command_len = 0;
                continue;
            }
        }
        const unsigned short remaining_command_space = sizeof(ui->command) - ui->command_len - 1;
        if (remaining_command_space == 0)
        {
            // Пропускаем слишком длинные команды, которых у нас точно нет
            break;
        }
        if (nl_ptr == NULL)
        {
            sv.len = remaining_command_space < readed ? remaining_command_space : readed;
            
            memcpy(ui->command + ui->command_len, sv.ptr, sv.len);
            ui->command_len += sv.len;
            sv.ptr += sv.len;
            break;
        }
        else
        {
            const unsigned int remaining_to_nl = nl_ptr - sv.ptr;
            const unsigned int to_add_count = remaining_command_space < remaining_to_nl ? remaining_command_space : remaining_to_nl;
            sv.len = to_add_count;
            memcpy(ui->command + ui->command_len, sv.ptr, sv.len);
            int command_res = process_command(ui->command, sock);
            if (command_res != 0)
                return command_res;
            sv.ptr += sv.len + 1;
            ui->is_start = true;
            ui->command_len = 0;
            continue;
        }
    }
    return 0;
}

int process_client(int sock)
{
    char buf[8];
    struct user_input ui = {
        .is_start = true,
        .command_len = 0,
        .command = {0}
    };

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

        int parse_res = parse_user_input(sock, buf, readed, &ui);
        if (parse_res != 0)
            return parse_res;
    }
    return 0;
}

void* worker(void* args)
{
    struct epoll_event events[MAX_POLL_EVENTS];
    int thread_id = *(int*)args;
    free(args);

    printf("Started new thread %d\n", thread_id);

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
                atomic_fetch_add_explicit(&all_clients_count, 1, memory_order_relaxed);
                atomic_fetch_add_explicit(&current_clients_count, 1, memory_order_relaxed);
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
                    shutdown(cur_sock, SHUT_WR);
                    char buf[128];
                    while (recv(cur_sock, buf, sizeof(buf), 0) > 0);
                    close(cur_sock);
                    atomic_fetch_sub_explicit(&current_clients_count, 1, memory_order_relaxed);
                }
            }
        }
    }
}

int main(int argc, char* argv[])
{
    int threads_count = 1;
    
    read_args(argc, argv, &threads_count);
    printf("Started at %d threads\n", threads_count);

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