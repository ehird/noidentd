#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <pwd.h>

/* one more byte than is needed ("65535 , 65535\r\n" = 15) */
#define MAXREQ 16
#define MAXCONNS 10
#define SEPARATOR " : USERID : UNIX : "
#define NELEMS(array) (sizeof(array) / sizeof((array)[0]))
#define COPY(dest, src, n) \
    do { \
        memcpy(dest, src, n); \
        dest += n; \
    } while (0)

static int server;

static void die(char *s)
{
    perror(s);
    exit(1);
}

static void cleanup()
{
    close(server);
}

static void drop(struct pollfd *client)
{
    shutdown(client->fd, SHUT_RDWR);
    close(client->fd);
    client->fd = -1;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s name\n", argv[0]);
        return 1;
    }

    char *name = argv[1];
    size_t namelen = strlen(name);

    server = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server < 0)
        die("socket");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(113);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server, (struct sockaddr *) &addr, sizeof(addr)) < 0)
        die("bind");

    atexit(cleanup);

    struct passwd *pwd = getpwnam("nobody");

    if (pwd == NULL)
        die("getpwnam");

    if (setuid(pwd->pw_uid) < 0)
        die("setuid");

    if (listen(server, MAXCONNS) < 0)
        die("listen");

    struct pollfd clients[MAXCONNS + 1];
    struct { size_t len; char data[MAXREQ]; } bufs[MAXCONNS];
    size_t serials[MAXCONNS], serial = 0;
    char *sendbuf = malloc(MAXREQ + sizeof(SEPARATOR) + namelen);
    if (sendbuf == NULL)
        die("malloc");

    clients[0].fd = server;
    clients[0].events = POLLIN;

    for (size_t i = 1; i < NELEMS(clients); i++)
        clients[i].fd = -1;

    for (;;) {
        if (poll(clients, NELEMS(clients), -1) <= 0)
            continue;

        if (clients[0].revents & POLLIN) {
            int fd = accept(server, NULL, NULL);
            if (fd > 0) {
                size_t slot;
                bool ok = false;
                for (size_t i = 1; i < NELEMS(clients); i++) {
                    if (clients[i].fd == -1) {
                        slot = i;
                        ok = true;
                        break;
                    }
                }
                if (!ok) {
                    size_t minserial = 0;
                    for (size_t i = 0; i < NELEMS(serials); i++) {
                        if (!ok || serials[i] < minserial) {
                            ok = true;
                            slot = i + 1;
                            minserial = serials[i];
                        }
                    }
                    size_t maxserial = 0;
                    for (size_t i = 0; i < NELEMS(serials); i++) {
                        serials[i] -= minserial;
                        if (serials[i] > maxserial)
                            maxserial = serials[i];
                    }
                    serial = maxserial + 1;
                    drop(&clients[slot]);
                }
                clients[slot].fd = fd;
                clients[slot].events = POLLIN;
                bufs[slot-1].len = 0;
                serials[slot-1] = serial++;
            }
        }

        for (size_t i = 0; i < NELEMS(clients); i++) {
            struct pollfd *client = &clients[i+1];

            if (client->revents & POLLIN) {
                if (bufs[i].len >= MAXREQ) {
                    drop(client);
                    continue;
                }

                char *destbuf = bufs[i].data + bufs[i].len;
                size_t bytesleft = MAXREQ - bufs[i].len;
                ssize_t bytes = recv(client->fd, destbuf, bytesleft, 0);
                if (bytes <= 0) {
                    drop(client);
                    continue;
                }

                char *end = memchr(bufs[i].data + bufs[i].len, '\n', bytes);
                bufs[i].len += bytes;
                if (!end)
                    continue;

                if (end > bufs[i].data && *(end-1) == '\r')
                    end--;

                char *p = sendbuf;
                COPY(p, bufs[i].data, end - bufs[i].data);
                COPY(p, SEPARATOR, sizeof(SEPARATOR) - 1);
                COPY(p, name, namelen);
                *p++ = '\r';
                *p++ = '\n';
                send(client->fd, sendbuf, p - sendbuf, 0);
                drop(client);
            }
        }
    }
}
