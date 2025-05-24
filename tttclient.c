/*    NAME-DEEP KUMAR SARKAR
      ROLL-22CS8125   */




#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 12345
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        exit(1);
    }
    
  
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address/ Address not supported\n");
        exit(1);
    }
    
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        exit(1);
    }
    printf("Connected to server %s:%d\n", argv[1], PORT);

    while (1) {
        memset(buffer, 0, sizeof(buffer));
       
        int n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
        if (n <= 0) {
            printf("Connection closed by server.\n");
            break;
        }
        printf("%s", buffer);
        
        
        if (strstr(buffer, "Game Over") != NULL) {
            break;
        }
        
      
        printf("Enter your move (row col): ");
        fflush(stdout);
        char move[50];
        if (fgets(move, sizeof(move), stdin) == NULL) {
            break;
        }
        send(sockfd, move, strlen(move), 0);
    }
    close(sockfd);
    return 0;
}

