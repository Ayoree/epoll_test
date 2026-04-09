int read_args(int argc, char* argv[], int* max_clients, int* threads)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0)
    {
        printf("Usage: [max_clients=1024] [threads=1]\n");
        return 0;
    }

    if (argc == 1)
        printf("No argumets is provided. Using default values. (try `--help` for usage)\n");

    if (argc >= 2)
        *max_clients = abs(atoi(argv[1]));
    
    if (*max_clients == 0)
    {
        fprintf(stderr, "Zero clients?\n");
        return 1;
    }

    if (argc >= 3)
        *threads = abs(atoi(argv[2]));
    
    if (*threads == 0)
    {
        fprintf(stderr, "Zero threads?\n");
        return 1;
    }
    return 0;
}

int create_server_socket(int* sock)
{
    *sock = socket(AF_INET, SOCK_STREAM /*| SOCK_NONBLOCK*/, 0);
    if (*sock == -1)
    {
        perror("Can't create server socket");
        return 1;
    }

    struct sockaddr_in local = {
        .sin_family = AF_INET,
        .sin_port = htons(6969)
    };
    inet_pton(AF_INET, "127.0.0.1", &local.sin_addr);
    if (bind(*sock, (void*)&local, sizeof(local)) == -1)
    {
        perror("Can't bind server socket");
        return 1;
    }

    return 0;
}

int main(int argc, char* argv[])
{
    int max_clients = 1024;
    int threads = 1;
    int server_sock;
    int err;
    
    err = read_args(argc, argv, &max_clients, &threads);
    if (err != 0)
        return err;
    printf("Started. Max clients: %d, threads %d\n", max_clients, threads);

    err = create_server_socket(&server_sock);
    if (err != 0)
        return err;
    printf("Server socket created: %d\n", server_sock);

    listen(server_sock, 128);
    int client_sock = accept(server_sock, NULL, NULL);
    printf("Client connected: %d\n", client_sock);

    return 0;
}