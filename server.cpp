#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <cerrno>

#define CLIENTS 1
#define PORT 8080

volatile sig_atomic_t wasSighup = 0;

void sighupHandler(int r) {
    wasSighup = 1;
}

int main() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        printf("Socket creation error");
        return -1;
    }

    struct sigaction sa{};
    sigaction(SIGHUP, nullptr, &sa);
    sa.sa_handler = sighupHandler;
    sa.sa_flags |= SA_RESTART;
    sigaction(SIGHUP, &sa, nullptr);

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(PORT);
    

    if (bind(serverSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == -1) {
        printf("Socket binding error");
        return -1;
    }

    if (listen(serverSocket, CLIENTS) == -1) {
        printf("Listening error");
        return -1;
    }

    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    sigprocmask(SIG_BLOCK, &blockedMask, &origMask);

    int clientSocket = -1;
    int newSocketFdConnection = -1;

    while (true) {
        fd_set readFileDescriptors;
        FD_ZERO(&readFileDescriptors);
        FD_SET(serverSocket, &readFileDescriptors);

        if (clientSocket != -1) {
            FD_SET(clientSocket, &readFileDescriptors);
        }

        int maxFd = (serverSocket > clientSocket) ? serverSocket : clientSocket;

        int pSelectResult = pselect(maxFd + 1, &readFileDescriptors, nullptr, nullptr, nullptr, &origMask);

        if (pSelectResult == -1) {
            if (errno == EINTR) {
                if (wasSighup) {
                    printf("Received SIGHUP");
                    break;
                } else {
                    printf("Received signal that is not processed in the system");
                }
            } else {
                printf("PSelect unknown error.");
                return -1;
            }
        }

        if (pSelectResult == 0) {
            continue;
        }

        if (FD_ISSET(serverSocket, &readFileDescriptors)) {
            if ((newSocketFdConnection = accept(serverSocket, nullptr, nullptr)) == -1) {
                printf("accept error");
                return -1;
            }

            if (clientSocket == -1) {
                clientSocket = newSocketFdConnection;
                printf("new client, socket num = %d \n", clientSocket);
                FD_SET(clientSocket, &readFileDescriptors);
            } else {
                close(newSocketFdConnection);
            }
        }


        if (FD_ISSET(clientSocket, &readFileDescriptors)) {
            char buffer[1024];
            ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);

            if (bytesReceived > 0) {
                printf("%ld bytes received from socket %d", bytesReceived, clientSocket);
                printf("Message: %s", buffer);
            } else if (bytesReceived == 0) {
                printf("Socket %d sent no bytes, closing it", clientSocket);
                close(clientSocket);
                clientSocket = -1;
            } else {
                printf("Error receiving data");
                return -1;
            }
        }
    }

    close(serverSocket);
    close(clientSocket);
    return 0;
}