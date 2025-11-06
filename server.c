#include "common.h"

// ==== NEW: Global Session Management ====
#define MAX_CLIENTS 10 // Max concurrent users
char active_users[MAX_CLIENTS][20];
int active_user_count = 0;
pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;
// =====================================

// ==== LOCKING UTILITIES ====

void lock_file(int fd, short lock_type) {
    struct flock lock;
    lock.l_type = lock_type;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock entire file
    lock.l_pid = getpid();
    fcntl(fd, F_SETLKW, &lock);
}

void lock_user_record(int fd, int record_num, short lock_type) {
    struct flock lock;
    lock.l_type = lock_type;
    lock.l_whence = SEEK_SET;
    lock.l_start = record_num * sizeof(User);
    lock.l_len = sizeof(User);
    lock.l_pid = getpid();
    fcntl(fd, F_SETLKW, &lock);
}

void lock_account_record(int fd, int record_num, short lock_type) {
    struct flock lock;
    lock.l_type = lock_type;
    lock.l_whence = SEEK_SET;
    lock.l_start = record_num * sizeof(Account);
    lock.l_len = sizeof(Account);
    lock.l_pid = getpid();
    fcntl(fd, F_SETLKW, &lock);
}

void lock_loan_record(int fd, int record_num, short lock_type) {
    struct flock lock;
    lock.l_type = lock_type;
    lock.l_whence = SEEK_SET;
    lock.l_start = record_num * sizeof(Loan);
    lock.l_len = sizeof(Loan);
    lock.l_pid = getpid();
    fcntl(fd, F_SETLKW, &lock);
}

void lock_feedback_record(int fd, int record_num, short lock_type) {
    struct flock lock;
    lock.l_type = lock_type;
    lock.l_whence = SEEK_SET;
    lock.l_start = record_num * sizeof(Feedback);
    lock.l_len = sizeof(Feedback);
    lock.l_pid = getpid();
    fcntl(fd, F_SETLKW, &lock);
}

// ==== TRANSACTION LOGGING ====

void log_transaction(char* userId, char* type, double amount, char* otherUser) {
    Transaction trans;
    trans.transactionId = (int)time(NULL);
    strcpy(trans.userId, userId);
    strcpy(trans.type, type);
    trans.amount = amount;
    strcpy(trans.otherUser, otherUser);

    int fd = open(TRANSACTION_DB, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) return;
    
    lock_file(fd, F_WRLCK);
    write(fd, &trans, sizeof(Transaction));
    lock_file(fd, F_UNLCK);
    close(fd);
}

// ==== AUTHENTICATION ====

int authenticate_user(int client_socket, User* current_user) {
    char userId[20], password[20];
    
    send(client_socket, "Enter User ID: ", 17, 0);
    int read_val = read(client_socket, userId, sizeof(userId) - 1);
    if (read_val <= 0) return 0;
    userId[strcspn(userId, "\n")] = 0;
    
    send(client_socket, "Enter Password: ", 18, 0);
    read_val = read(client_socket, password, sizeof(password) - 1);
    if (read_val <= 0) return 0;
    password[strcspn(password, "\n")] = 0;

    int fd = open(USER_DB, O_RDONLY);
    if (fd == -1) {
        send(client_socket, "Error: Cannot open user database.\n", 35, 0);
        return 0;
    }

    User user;
    int user_found = 0;

    lock_file(fd, F_RDLCK);
    lseek(fd, 0, SEEK_SET); 

    while (read(fd, &user, sizeof(User)) == sizeof(User)) {
        if (strcmp(user.userId, userId) == 0 && strcmp(user.password, password) == 0) {
            user_found = 1;
            *current_user = user;
            break;
        }
    }
    
    lock_file(fd, F_UNLCK); 
    close(fd);

    if (user_found) {
        return 1;
    } else {
        send(client_socket, "Login Failed: Invalid credentials.\n", 34, 0);
        return 0;
    }
}


// ==== CLIENT HANDLER ====
void *handle_client(void *client_socket_ptr) {
    int client_socket = *(int*)client_socket_ptr;
    free(client_socket_ptr);
    
    User current_user;

    if (!authenticate_user(client_socket, &current_user)) {
        close(client_socket);
        printf("Client login failed (bad credentials).\n");
        return NULL;
    }

    int already_logged_in = 0;
    pthread_mutex_lock(&session_mutex); 

    for (int i = 0; i < active_user_count; i++) {
        if (strcmp(active_users[i], current_user.userId) == 0) {
            already_logged_in = 1;
            break;
        }
    }

    if (already_logged_in) {
        send(client_socket, "Error: This user is already logged in.\n", 39, 0);
        pthread_mutex_unlock(&session_mutex);
        close(client_socket);
        printf("Client %s failed (already logged in).\n", current_user.userId);
        return NULL;
    }

    if (active_user_count < MAX_CLIENTS) {
        strcpy(active_users[active_user_count], current_user.userId);
        active_user_count++;
    } else {
        send(client_socket, "Error: Server is full. Try again later.\n", 40, 0);
        pthread_mutex_unlock(&session_mutex); 
        close(client_socket);
        printf("Client %s failed (server full).\n", current_user.userId);
        return NULL;
    }
    
    pthread_mutex_unlock(&session_mutex);

    send(client_socket, "Login Successful!\n", 18, 0);
    printf("User %s logged in. Active users: %d\n", current_user.userId, active_user_count);

    while (1) {
        char temp_buf;
        int peek = recv(client_socket, &temp_buf, 1, MSG_PEEK | MSG_DONTWAIT);
        if (peek == 0) {
            break; 
        }

        if (strcmp(current_user.role, "admin") == 0) {
            handle_admin_menu(client_socket, &current_user);
        } 
        else if (strcmp(current_user.role, "manager") == 0) {
            handle_manager_menu(client_socket, &current_user);
        }
        else if (strcmp(current_user.role, "employee") == 0) {
            handle_employee_menu(client_socket, &current_user);
        }
        else if (strcmp(current_user.role, "customer") == 0) {
            handle_customer_menu(client_socket, &current_user);
        }
        else {
            send(client_socket, "Unknown role. Logging out.\n", 28, 0);
            break;
        }

        peek = recv(client_socket, &temp_buf, 1, MSG_PEEK | MSG_DONTWAIT);
        if (peek <= 0) {
            break; 
        }
    }

    pthread_mutex_lock(&session_mutex);
    for (int i = 0; i < active_user_count; i++) {
        if (strcmp(active_users[i], current_user.userId) == 0) {
            active_user_count--;
            strcpy(active_users[i], active_users[active_user_count]);
            memset(active_users[active_user_count], 0, 20);
            break;
        }
    }
    pthread_mutex_unlock(&session_mutex);

    printf("User %s disconnected. Active users: %d\n", current_user.userId, active_user_count);
    close(client_socket);
    return NULL;
}


// === MAIN SERVER FUNCTION ===
int main() {
    printf("--- RUNNING LATEST CODE (FINAL VERSION) ---\n");
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    memset(active_users, 0, sizeof(active_users));

    printf("Server listening on port %d\n", PORT);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("accept failed");
            continue;
        }
        
        printf("New client connected.\n");

        pthread_t thread_id;
        int *client_socket_ptr = malloc(sizeof(int));
        if (client_socket_ptr == NULL) {
            perror("malloc failed");
            close(new_socket);
            continue;
        }
        *client_socket_ptr = new_socket;
        
        if(pthread_create(&thread_id, NULL, handle_client, (void*)client_socket_ptr) != 0) {
            perror("pthread_create failed");
            free(client_socket_ptr);
            close(new_socket);
        }
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}