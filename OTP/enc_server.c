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

void error(const char *msg, int exitCode) {
    fprintf(stderr, "%s\n", msg);
    if (exitCode != 0) {
        exit(exitCode);
    }
}

void setupAddressStruct(struct sockaddr_in* address, int portNumber) {
    memset((char*) address, '\0', sizeof(*address));
    address->sin_family = AF_INET;
    address->sin_port = htons(portNumber);
    address->sin_addr.s_addr = INADDR_ANY;
}

int sendall(int s, char *buf, int *len) {
    // How many bytes we've sent
    int total = 0;
    // How many we have left to send
    int bytesleft = *len;
    int n;

    while (total < *len) {
        n = send(s, buf + total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    // Return number actually sent
    *len = total;
    return n == -1 ? -1 : 0;
}

void encryptText(char *plaintext, char *key, char *ciphertext) {
    int i;
    // Initialize the entire ciphertext buffer to null characters
    memset(ciphertext, '\0', 150000);
    for(i = 0; plaintext[i] != '\0' && key[i] != '\0'; i++) {
        int plainChar = (plaintext[i] == ' ') ? 26 : plaintext[i] - 'A';
        int keyChar = (key[i] == ' ') ? 26 : key[i] - 'A';
        int cipherChar = (plainChar + keyChar) % 27;

        ciphertext[i] = (cipherChar == 26) ? ' ' : cipherChar + 'A';
    }
    // Set null terminator after the loop
    ciphertext[i] = '\0';
}

void handleClient(int connectionSocket) {
    char clientVerification[150000], plaintext[150000], key[150000], ciphertext[150000];
    int bytesReceived, textLength, keyLength;

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

    // Verify client identity for encryption
    if (strcmp(clientVerification, "enc_client") != 0) {
        fprintf(stderr, "Client verification failed. Expected 'enc_client', got '%s'\n", clientVerification);
        close(connectionSocket);
        return;
    }

    // Send acknowledgment for client verification
    char ack[] = "ACK";
    if (send(connectionSocket, ack, sizeof(ack), 0) < 0) {
        fprintf(stderr, "ERROR sending acknowledgment to socket\n");
        close(connectionSocket);
        return;
    }

    // Receive plaintext size
    bytesReceived = recv(connectionSocket, &textLength, sizeof(textLength), 0);
    if (bytesReceived < 0) {
        fprintf(stderr, "ERROR reading plaintext size from socket\n");
        close(connectionSocket);
        return;
    }
    textLength = ntohl(textLength);

    // Read plaintext
    int totalReceived = 0;
    while (totalReceived < textLength) {
        bytesReceived = recv(connectionSocket, plaintext + totalReceived, textLength - totalReceived, 0);
        if (bytesReceived < 0) {
            fprintf(stderr, "ERROR reading plaintext from socket\n");
            close(connectionSocket);
            return;
        }
        totalReceived += bytesReceived;
    }

    // Send acknowledgment for plaintext receipt
    if (send(connectionSocket, ack, sizeof(ack), 0) < 0) {
        fprintf(stderr, "ERROR sending acknowledgment to socket\n");
        close(connectionSocket);
        return;
    }

    // Receive key size
    bytesReceived = recv(connectionSocket, &keyLength, sizeof(keyLength), 0);
    if (bytesReceived < 0) {
        fprintf(stderr, "ERROR reading key size from socket\n");
        close(connectionSocket);
        return;
    }
    keyLength = ntohl(keyLength);

    // Read key
    totalReceived = 0;
    while (totalReceived < keyLength) {
        bytesReceived = recv(connectionSocket, key + totalReceived, keyLength - totalReceived, 0);
        if (bytesReceived < 0) {
            fprintf(stderr, "ERROR reading key from socket\n");
            close(connectionSocket);
            return;
        }
        totalReceived += bytesReceived;
    }
    
    // Perform encryption
    encryptText(plaintext, key, ciphertext);

    //send ciphertext size
    int ciphertextLen = strlen(ciphertext);
    int netCiphertextLen = htonl(ciphertextLen);
    send(connectionSocket, &netCiphertextLen, sizeof(netCiphertextLen), 0);

    // Send the encrypted text back to the client
    ciphertextLen = strlen(ciphertext);
    if (sendall(connectionSocket, ciphertext, &ciphertextLen) == -1) {
        fprintf(stderr, "ERROR writing to socket\n");
    }

    // Always close the connection socket at the end
    close(connectionSocket);
}

int main(int argc, char *argv[]) {
    if (argc < 2) { 
        // Check for proper number of command line arguments
        fprintf(stderr, "USAGE: %s port\n", argv[0]);
        exit(1);
    }

    int portNumber = atoi(argv[1]);

    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) error("ERROR opening socket", 1);

    int yes = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        error("setsockopt", 1);
    }

    struct sockaddr_in serverAddress;
    setupAddressStruct(&serverAddress, portNumber);

    if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        error("ERROR on binding", 1);
    }

    // Up to 5 connections at 1 t
    listen(listenSocket, 5);

    // Fork per-connection
    while (1) {
        int connectionSocket = accept(listenSocket, NULL, NULL);
        if (connectionSocket < 0) {
            error("ERROR on accept", 0);
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            error("ERROR on fork", 0);
        } else if (pid == 0) {
            // Close listening socket
            close(listenSocket);
            // Handle client connection
            handleClient(connectionSocket);
            exit(0);
        } else {
            close(connectionSocket);
        }
    }

    return 0;
}