#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "clib.h"
#include "cnet.h"

ssize_t sendbytes(int sock, char *buf, size_t count);
ssize_t recvbytes(int sock, char *buf, size_t count);
void sendTextMessage(int sock, char *msg);
void sendZeroMessage(int sock);

int main(int argc, char *argv[]) {
    int z;
    char *serverhost = "localhost";
    char *serverport = "8000";

    if (argc < 3) {
        printf("Usage: %s <server> <port>\n", argv[0]);
        printf("Ex. tclient 127.0.0.1 5000\n");
        exit(1);
    }

    int backlog = 50;
    struct sockaddr sa;
    int s0 = OpenConnectSocket(serverhost, serverport, backlog, &sa);
    if (s0 == -1)
        exit(1);

    String ipaddr = StringNew("");
    GetTextIPAddress(&sa, &ipaddr);
    printf("Connected to %.*s port %s...\n", ipaddr.len, ipaddr.bs, serverport);
    StringFree(&ipaddr);

    return 0;
}

void sendTextMessage(int sock, char *msg) {
    int z;
    unsigned short msglen = strlen(msg);
    unsigned short netmsglen = htons(msglen);
    z = sendbytes(sock, (char *) &netmsglen, sizeof(netmsglen));
    if (z == -1)
        fprintf(stderr, "send(): %s\n", strerror(errno));
    z = sendbytes(sock, msg, strlen(msg));
    if (z == -1)
        fprintf(stderr, "send(): %s\n", strerror(errno));
}

void sendZeroMessage(int sock) {
    unsigned short msglen = 0;
    int z = sendbytes(sock, (char *) &msglen, sizeof(msglen));
    if (z == -1)
        fprintf(stderr, "send(): %s\n", strerror(errno));
}

// Send count buf bytes into sock.
// Returns num bytes sent or -1 for error
ssize_t sendbytes(int sock, char *buf, size_t count) {
    int nsent = 0;
    while (nsent < count) {
        int z = send(sock, buf+nsent, count-nsent, 0);
        // socket closed, no more data
        if (z == 0) {
            // socket closed
            break;
        }
        // interrupt occured during send, retry send.
        if (z == -1 && errno == EINTR) {
            continue;
        }
        // no data available at the moment, just return what we have.
        if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        // any other error
        if (z == -1) {
            return z;
        }
        nsent += z;
    }

    return nsent;
}

// Receive count bytes into buf.
// Returns num bytes received or -1 for error.
ssize_t recvbytes(int sock, char *buf, size_t count) {
    memset(buf, '*', count); // initialize for debugging purposes.

    int nread = 0;
    while (nread < count) {
        int z = recv(sock, buf+nread, count-nread, 0);
        // socket closed, no more data
        if (z == 0) {
            break;
        }
        // interrupt occured during read, retry read.
        if (z == -1 && errno == EINTR) {
            continue;
        }
        // no data available at the moment, just return what we have.
        if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        // any other error
        if (z == -1) {
            return z;
        }
        nread += z;
    }

    return nread;
}

