#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <stdatomic.h>

struct string_view
{
    unsigned char* ptr;
    unsigned int len;
};

struct user_input
{
    bool is_start;
    bool is_command;
    unsigned short command_len;
    unsigned char command[32];
};
