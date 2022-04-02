#include <csignal>
#include <sys/wait.h>
#include "common.h"

#define BACKLOG 10

void sigchild_handler(int s)
{
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

int main()
{
    int server, client;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage client_addr;
    socklen_t size;
    struct sigaction sa;
    int yes = 1;
    char str[INET6_ADDRSTRLEN];
    int ret;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((ret = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((server = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("chatter-server: socket");
            continue;
        }
        if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }
        if (bind(server, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(server);
            perror("chatter-server: bind");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL)
    {
        fprintf(stderr, "chatter-server: failed to bind\n");
        exit(1);
    }

    if (listen(server, BACKLOG) == -1)
    {
        perror("chatter-server: listen");
        exit(1);
    }

    sa.sa_handler = sigchild_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("chatter-server: sigaction");
        exit(1);
    }

    cout << "Waiting for clients...\n";

    while (true)
    {
        size = sizeof client_addr;
        client = accept(server, (struct sockaddr*)&client_addr, &size);
        if (client == -1)
        {
            perror("chatter-server: accept");
            continue;
        }

        inet_ntop(client_addr.ss_family,
            get_in_addr((struct sockaddr*)&client_addr), str, sizeof str);
        printf("New connection from %s\n", str);

        if (!fork())
        {
            close(server);
            if (send(client, "Hello, world!", 13, 0) == -1)
            {
                perror("chatter-server: send");
            }
            close(client);
            exit(0);
        }
        close(client);
    }

    return 0;
}
