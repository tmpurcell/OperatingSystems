#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <netinet/in.h>

#define MAX_CHILDREN 5

void error(const char *msg, int shouldExit) {
    perror(msg);
    if (shouldExit) exit(1);
}

void setupAddressStruct(struct sockaddr_in* address, int portNumber) {
    memset((char*) address, '\0', sizeof(*address));
    address->sin_family = AF_INET;
    address->sin_port = htons(portNumber);
    address->sin_addr.s_addr = INADDR_ANY;
}

void decryptText(char *ciphertext, char *key, char *plaintext) {
    int i;
    for(i = 0; ciphertext[i] != '\0' && key[i] != '\0'; i++) {
        int cipherChar = (ciphertext[i] == ' ') ? 26 : ciphertext[i] - 'A';
        int keyChar = (key[i] == ' ') ? 26 : key[i] - 'A';
        int plainChar = (cipherChar - keyChar + 27) % 27;

        plaintext[i] = (plainChar == 26) ? ' ' : plainChar + 'A';
    }
    // Null-terminator
    plaintext[i] = '\0';
}

int sendall(int s, char *buf, int *len) {
    int total = 0; // How many bytes we've sent
    int bytesleft = *len; // How many we have left to send
    int n;

    while (total < *len) {
        n = send(s, buf + total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // Return number actually sent here

    return n == -1 ? -1 : 0; // Return -1 on failure, 0 on success
}

void handleClient(int connectionSocket) {
    char clientVerification[150000], plaintext[150000], key[150000], ciphertext[150000];
    int bytesReceived, ciphertextLength, keyLength;

    // Initialize buffers
    memset(clientVerification, '\0', sizeof(clientVerification));
    memset(plaintext, '\0', sizeof(plaintext));
    memset(key, '\0', sizeof(key));
    memset(ciphertext, '\0', sizeof(ciphertext));

    // Attempt to read client verification message
    bytesReceived = recv(connectionSocket, clientVerification, sizeof(clientVerification) - 1, 0);
    if (bytesReceived < 0) {
        fprintf(stderr, "Error reading from socket\n");
        close(connectionSocket);
        return;
    }

    // Verify client verification message
    if (strcmp(clientVerification, "dec_client") != 0) {
        error("Client verification failed", 0);
        close(connectionSocket);
        return;
    }

    // Send acknowledgment for client verification
    char ack[] = "ACK";
    if (send(connectionSocket, ack, sizeof(ack), 0) < 0) {
        error("ERROR sending acknowledgment to socket", 0);
        close(connectionSocket);
        return;
    }

    // Receive ciphertext size
    bytesReceived = recv(connectionSocket, &ciphertextLength, sizeof(ciphertextLength), 0);
    if (bytesReceived < 0) {
        error("ERROR reading ciphertext size from socket", 1);
    }
    ciphertextLength = ntohl(ciphertextLength);

    // Receive ciphertext
    int totalReceived = 0;
    while (totalReceived < ciphertextLength) {
        bytesReceived = recv(connectionSocket, ciphertext + totalReceived, ciphertextLength - totalReceived, 0);
        if (bytesReceived < 0) {
            error("ERROR reading ciphertext from socket", 1);
        }
        totalReceived += bytesReceived;
    }

    // Send acknowledgment for ciphertext receipt
    if (send(connectionSocket, ack, sizeof(ack), 0) < 0) {
        error("ERROR sending acknowledgment to socket", 0);
        close(connectionSocket);
        return;
    }

    // Receive key size
    bytesReceived = recv(connectionSocket, &keyLength, sizeof(keyLength), 0);
    if (bytesReceived < 0) {
        error("ERROR reading key size from socket", 1);
    }
    keyLength = ntohl(keyLength);

    // Receive key based on the received size
    totalReceived = 0;
    while (totalReceived < keyLength) {
        bytesReceived = recv(connectionSocket, key + totalReceived, keyLength - totalReceived, 0);
        if (bytesReceived < 0) {
            error("ERROR reading key from socket", 1);
        }
        totalReceived += bytesReceived;
    }

    // Decrypt the text
    decryptText(ciphertext, key, plaintext);

    //send ciphertext size
    int plaintextlen = strlen(plaintext); // Assuming ciphertext is null-terminated
    int netPlainTextLen = htonl(plaintextlen);
    send(connectionSocket, &netPlainTextLen, sizeof(netPlainTextLen), 0);

    // Send the encrypted text back to the client
    plaintextlen = strlen(plaintext);
    if (sendall(connectionSocket, plaintext, &plaintextlen) == -1) {
        fprintf(stderr, "ERROR writing to socket\n");
    }

    // Close the connection
    close(connectionSocket);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "USAGE: %s port\n", argv[0]);
        exit(1);
    }

    int portNumber = atoi(argv[1]);
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) error("ERROR opening socket", 1);

    int yes = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        error("setsockopt", 1);
    }

    struct sockaddr_in serverAddress;
    setupAddressStruct(&serverAddress, portNumber);

    if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        error("ERROR on binding", 1);
    }

    listen(listenSocket, 5);

    while (1) {
        struct sockaddr_in clientAddress;
        socklen_t sizeOfClientInfo = sizeof(clientAddress);
        
        int connectionSocket = accept(listenSocket, (struct sockaddr *)&clientAddress, &sizeOfClientInfo);
        if (connectionSocket < 0) {
            error("ERROR on accept", 0);
            continue;
        }

        pid_t spawnPid = fork();
        if (spawnPid == -1) {
            error("ERROR on fork", 0);
            close(connectionSocket);
            continue;
        } else if (spawnPid == 0) {
            // Child process
            close(listenSocket); 
            handleClient(connectionSocket);
            exit(0);
        } else {
            close(connectionSocket);
        }
    }

    return 0;
}