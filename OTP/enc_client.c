#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

void error(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void setupAddressStruct(struct sockaddr_in* address, int portNumber, char* hostname) {
    memset((char*) address, '\0', sizeof(*address));
    address->sin_family = AF_INET;
    address->sin_port = htons(portNumber);
    struct hostent* hostInfo = gethostbyname(hostname); 
    if (hostInfo == NULL) { 
        error("CLIENT: ERROR, no such host");
    }
    memcpy((char*) &address->sin_addr.s_addr, hostInfo->h_addr_list[0], hostInfo->h_length);
}

int sendall(int s, char *buf, int *len) {
    // How many bytes we've sent
    int total = 0;
    // How many we have left to send
    int bytesleft = *len;
    int n;

    while(total < *len) {
        n = send(s, buf + total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }
    // Return number actually sent here
    *len = total;
    return n == -1 ? -1 : 0;
}

int main(int argc, char *argv[]) {
    int socketFD, portNumber, charsWritten, charsRead;
    struct sockaddr_in serverAddress;
    char buffer[150000];
    char ackBuffer[25];

    // Check argument count
    if (argc != 4) { 
        fprintf(stderr, "USAGE: %s plaintext key port\n", argv[0]);
        exit(1);
    }

   // Open plaintext file
    FILE *plaintextFile = fopen(argv[1], "r");
    if (plaintextFile == NULL) {
        error("CLIENT: ERROR opening plaintext file");
    }

    // Read plaintext from file and remove newline
    if (fgets(buffer, sizeof(buffer), plaintextFile) != NULL) {
        buffer[strcspn(buffer, "\n")] = '\0';
    }
    fclose(plaintextFile);

    // Open key file
    FILE *keyFile = fopen(argv[2], "r");
    if (keyFile == NULL) {
        error("CLIENT: ERROR opening key file");
    }

    // Read key from file
    char key[150000];
    // Read key from file and remove newline
    if (fgets(key, sizeof(key), keyFile) != NULL) {
        key[strcspn(key, "\n")] = '\0';
    }
    fclose(keyFile);

    // Check for bad characters in plaintext or key
    for (int i = 0; i < strlen(buffer); i++) {
        if ((buffer[i] < 'A' || buffer[i] > 'Z') && buffer[i] != ' ' && buffer[i] != '\n') {
            error("CLIENT: ERROR bad characters in plaintext");
        }
    }
    for (int i = 0; i < strlen(key); i++) {
        if ((key[i] < 'A' || key[i] > 'Z') && key[i] != ' ' && key[i] != '\n') {
            error("CLIENT: ERROR bad characters in key");
        }
    }

    // Check if key is shorter than plaintext
    if (strlen(key) < strlen(buffer)) {
        error("CLIENT: ERROR key is shorter than plaintext");
    }

    // Create socket
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0) {
        error("CLIENT: ERROR opening socket");
    }

    // Set up server address
    setupAddressStruct(&serverAddress, atoi(argv[3]), "localhost");

    // Connect to server
    if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        fprintf(stderr, "CLIENT: ERROR connecting to server on port %d\n", atoi(argv[3]));
        exit(2);
    }

    // Send verification message to the server
    char verificationMsg[] = "enc_client";
    charsWritten = send(socketFD, verificationMsg, strlen(verificationMsg), 0); 
    if (charsWritten < 0) {
        error("CLIENT: ERROR writing verification message to socket");
    }

    // Wait for acknowledgment from the server
    memset(ackBuffer, '\0', sizeof(ackBuffer));
    charsRead = recv(socketFD, ackBuffer, sizeof(ackBuffer) - 1, 0);
    if (charsRead < 0 || strcmp(ackBuffer, "ACK") != 0) {
        error("CLIENT: ERROR receiving acknowledgment after verification message");
    }

    // Send plaintext size to server
    int plaintextLength = strlen(buffer);
    // Convert to network byte order
    int netPlaintextLength = htonl(plaintextLength);
    charsWritten = send(socketFD, &netPlaintextLength, sizeof(netPlaintextLength), 0);
    if (charsWritten < 0) {
        error("CLIENT: ERROR writing plaintext length to socket");
    }

    // Send plaintext to server
    int msgLength = strlen(buffer);
    if (sendall(socketFD, buffer, &msgLength) == -1) {
        error("CLIENT: ERROR sending plaintext to socket");
    }

    // Wait for acknowledgment from the server before sending the key
    memset(ackBuffer, '\0', sizeof(ackBuffer));
    charsRead = recv(socketFD, ackBuffer, sizeof(ackBuffer) - 1, 0);
    if (charsRead < 0 || strcmp(ackBuffer, "ACK") != 0) {
        error("CLIENT: ERROR receiving acknowledgment after sending plaintext");
    }

    // Send key size to server
    int keyLength = strlen(key);
    // Convert to network byte order
    int netKeyLength = htonl(keyLength);
    charsWritten = send(socketFD, &netKeyLength, sizeof(netKeyLength), 0);
    if (charsWritten < 0) {
        error("CLIENT: ERROR writing key length to socket");
    }

    // Send key to server
    msgLength = strlen(key);
    if (sendall(socketFD, key, &msgLength) == -1) {
        error("CLIENT: ERROR sending key to socket");
    }

    int netCiphertextLen;
    charsRead = recv(socketFD, &netCiphertextLen, sizeof(netCiphertextLen), 0);
    if (charsRead < 0) {
        error("CLIENT: ERROR reading ciphertext length from socket");
    }
    int ciphertextLen = ntohl(netCiphertextLen);

    // Receive the ciphertext
    memset(buffer, '\0', sizeof(buffer));
    int totalCharsRead = 0;
    while (totalCharsRead < ciphertextLen) {
        charsRead = recv(socketFD, buffer + totalCharsRead, ciphertextLen - totalCharsRead, 0);
        if (charsRead < 0) {
            error("CLIENT: ERROR reading from socket");
        } else if (charsRead == 0) {
            // The connection has been closed before all the data was received
            break;
        }
        totalCharsRead += charsRead;
    }

    // Output ciphertext to stdout
    printf("%s\n", buffer);

    close(socketFD); 
    return 0;
}