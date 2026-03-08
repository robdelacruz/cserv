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

ssize_t sendbytes(int sock, char *buf, size_t count);
ssize_t recvbytes(int sock, char *buf, size_t count);
void sendTextMessage(int sock, char *msg);
void sendZeroMessage(int sock);

int main(int argc, char *argv[]) {
    int z;
    int sock;

    if (argc < 3) {
        printf("Usage: tclient <server domain> <port>\n");
        printf("Ex. tclient 127.0.0.1 5000\n");
        exit(1);
    }

    char *server_domain = argv[1];
    char *server_port = argv[2];
    struct addrinfo hints, *servaddr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    z = getaddrinfo(server_domain, server_port, &hints, &servaddr);
    if (z != 0) {
        fprintf(stderr, "getaddrinfo(): %s\n", strerror(errno));
        exit(1);
    }

    // Server socket
    sock = socket(servaddr->ai_family, servaddr->ai_socktype, servaddr->ai_protocol);
    if (sock == -1) {
        fprintf(stderr, "socket(): %s\n", strerror(errno));
        exit(1);
    }

    z = connect(sock, servaddr->ai_addr, servaddr->ai_addrlen);
    if (z != 0) {
        fprintf(stderr, "connect(): %s\n", strerror(errno));
        exit(1);
    }

    freeaddrinfo(servaddr);
    servaddr = NULL;

    printf("Connected to %s:%s\n", server_domain, server_port);

    sendTextMessage(sock, "Now is the time for all good men to come to the aid of the party.");
    sendTextMessage(sock, "abc");
    sendTextMessage(sock, "def");
    sendTextMessage(sock, "Z");
    //sendZeroMessage(sock);
    sendTextMessage(sock, "abc");

//    printf("Receiving response...\n");
//    z = recvbytes(sock, respmsg, sizeof(respmsg));
//    if (z == -1) {
//        fprintf(stderr, "recv(): %s\n", strerror(errno));
//        exit(1);
//    }
//    respmsg[z] = '\0';
//    printf("%s", respmsg);

    sleep(10);
    z = close(sock);
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

