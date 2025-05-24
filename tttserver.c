
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <errno.h>
#include <sys/wait.h>

#define PORT 12345
#define K 3
#define BOARD_SIZE (K*K)
#define SHM_NAME "/ttt_shm"
#define SEM1_NAME "/ttt_sem1"
#define SEM2_NAME "/ttt_sem2"
#define BUFFER_SIZE 1024


void init_board(char *board) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        board[i] = ' ';
    }
}


void board_to_string(char *board, char *buf, size_t buflen) {
    int pos = 0;
    pos += snprintf(buf+pos, buflen-pos, "\n");
    for (int i = 0; i < K; i++) {
        for (int j = 0; j < K; j++) {
            pos += snprintf(buf+pos, buflen-pos, " %c ", board[i*K+j]);
            if (j < K-1) {
                pos += snprintf(buf+pos, buflen-pos, "|");
            }
        }
        pos += snprintf(buf+pos, buflen-pos, "\n");
        if (i < K-1) {
            for (int j = 0; j < K; j++) {
                pos += snprintf(buf+pos, buflen-pos, "---");
                if (j < K-1) {
                    pos += snprintf(buf+pos, buflen-pos, "+");
                }
            }
            pos += snprintf(buf+pos, buflen-pos, "\n");
        }
    }
    pos += snprintf(buf+pos, buflen-pos, "\n");
}


int check_win(char *board, char symbol) {
    
    for (int i = 0; i < K; i++) {
        int win = 1;
        for (int j = 0; j < K; j++) {
            if (board[i*K+j] != symbol) {
                win = 0;
                break;
            }
        }
        if (win) return 1;
    }
  
    for (int j = 0; j < K; j++) {
        int win = 1;
        for (int i = 0; i < K; i++) {
            if (board[i*K+j] != symbol) {
                win = 0;
                break;
            }
        }
        if (win) return 1;
    }
 
    int win = 1;
    for (int i = 0; i < K; i++) {
        if (board[i*K+i] != symbol) {
            win = 0;
            break;
        }
    }
    if (win) return 1;

    return 0;
}


int check_draw(char *board) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (board[i] == ' ') return 0;
    }
    return 1;
}


void handle_client(int client_sock, char symbol, sem_t *own_sem, sem_t *other_sem, char *board) {
    char buffer[BUFFER_SIZE];
    char board_str[BUFFER_SIZE];
    int game_over = 0;
    
    while (1) {
     
        sem_wait(own_sem);

       
        if (check_win(board, 'X') || check_win(board, 'O') || check_draw(board))
            game_over = 1;

       
        board_to_string(board, board_str, sizeof(board_str));
        if (game_over) {
            if (check_win(board, 'X')) {
                strcat(board_str, "Game Over. Winner: X\n");
            } else if (check_win(board, 'O')) {
                strcat(board_str, "Game Over. Winner: O\n");
            } else {
                strcat(board_str, "Game Over. Draw.\n");
            }
            send(client_sock, board_str, strlen(board_str), 0);
           
            sem_post(other_sem);
            break;
        } else {
            char prompt[100];
            snprintf(prompt, sizeof(prompt), "Your turn (%c). Enter move as row and col (0-indexed): ", symbol);
            strcat(board_str, prompt);
            send(client_sock, board_str, strlen(board_str), 0);
        }

       
        memset(buffer, 0, sizeof(buffer));
        int n = recv(client_sock, buffer, sizeof(buffer)-1, 0);
        if (n <= 0) {
            perror("recv");
            break;
        }
        
       
        int row, col;
        if (sscanf(buffer, "%d %d", &row, &col) != 2) {
            char *msg = "Invalid move format. Try again.\n";
            send(client_sock, msg, strlen(msg), 0);
            sem_post(own_sem);  // allow retry
            continue;
        }
        if (row < 0 || row >= K || col < 0 || col >= K || board[row*K+col] != ' ') {
            char *msg = "Invalid move (out of range or cell occupied). Try again.\n";
            send(client_sock, msg, strlen(msg), 0);
            sem_post(own_sem);  // allow retry
            continue;
        }
      
        board[row*K+col] = symbol;

        
        if (check_win(board, symbol) || check_draw(board))
            sem_post(other_sem);
        else
            sem_post(other_sem);
    }
    close(client_sock);
    exit(0);
}

int main() {
    int sockfd, client1, client2;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;

 
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }
    listen(sockfd, 5);
    printf("Server listening on port %d...\n", PORT);

    
    clilen = sizeof(cli_addr);
    client1 = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (client1 < 0) {
        perror("ERROR on accept client1");
        exit(1);
    }
    printf("Client1 connected.\n");

   
    client2 = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (client2 < 0) {
        perror("ERROR on accept client2");
        exit(1);
    }
    printf("Client2 connected.\n");

    
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        exit(1);
    }
    ftruncate(shm_fd, BOARD_SIZE * sizeof(char));
    char *board = mmap(NULL, BOARD_SIZE * sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (board == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    init_board(board);

   
    sem_t *sem1 = sem_open(SEM1_NAME, O_CREAT, 0666, 0);
    sem_t *sem2 = sem_open(SEM2_NAME, O_CREAT, 0666, 0);
    if (sem1 == SEM_FAILED || sem2 == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    
    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("fork");
        exit(1);
    }
    if (pid1 == 0) {
        close(sockfd);
        handle_client(client1, 'X', sem1, sem2, board);
    }

  
    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("fork");
        exit(1);
    }
    if (pid2 == 0) {
        close(sockfd);
        handle_client(client2, 'O', sem2, sem1, board);
    }

    
    close(client1);
    close(client2);

    
    sem_post(sem1);

    
    wait(NULL);
    wait(NULL);


    sem_close(sem1);
    sem_close(sem2);
    sem_unlink(SEM1_NAME);
    sem_unlink(SEM2_NAME);
    munmap(board, BOARD_SIZE * sizeof(char));
    shm_unlink(SHM_NAME);

    close(sockfd);
    printf("Game finished. Server ready for a new game.\n");
    return 0;
}

