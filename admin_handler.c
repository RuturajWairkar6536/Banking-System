#include "common.h"
#include <ctype.h> // Needed for trim

// --- NEW HELPER FUNCTION ---
static void trim_whitespace(char *str) {
    if (!str) return;
    char *start = str;
    while (isspace((unsigned char)*start)) {
        start++;
    }
    char *end = str + strlen(str) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        end--;
    }
    end[1] = '\0';
    memmove(str, start, end - start + 2);
}

// --- NEW SAFE READ FUNCTION ---
// Always reads into a large temp buffer to clear the socket
static int safe_read(int sock, char *buffer, int len) {
    char temp_buf[1024]; // Use a large temp buffer
    memset(temp_buf, 0, sizeof(temp_buf));
    
    int read_val = read(sock, temp_buf, sizeof(temp_buf) - 1);
    if (read_val <= 0) return read_val;
    
    temp_buf[read_val] = '\0';
    trim_whitespace(temp_buf);
    
    // Copy the cleaned string into the destination buffer
    strncpy(buffer, temp_buf, len - 1);
    buffer[len - 1] = '\0'; // Ensure null termination
    
    return read_val;
}

// --- Shared Password Change Function ---
static void user_change_password(int client_socket, char* userId) {
    char old_pass[20], new_pass[20];
    send(client_socket, "Enter Old Password: ", 20, 0);
    safe_read(client_socket, old_pass, sizeof(old_pass));
    
    send(client_socket, "Enter New Password: ", 20, 0);
    safe_read(client_socket, new_pass, sizeof(new_pass));

    int fd = open(USER_DB, O_RDWR);
    if (fd == -1) {
        send(client_socket, "Error: Could not open user database.\n", 37, 0); return;
    }
    User user;
    int record_num = 0;
    int user_found = 0;
    while (read(fd, &user, sizeof(User)) == sizeof(User)) {
        if (strcmp(user.userId, userId) == 0) {
            user_found = 1;
            lock_user_record(fd, record_num, F_WRLCK);
            if (strcmp(user.password, old_pass) != 0) {
                send(client_socket, "Error: Incorrect old password.\n", 30, 0);
                lock_user_record(fd, record_num, F_UNLCK);
                close(fd); return;
            }
            lseek(fd, -sizeof(User), SEEK_CUR); 
            strcpy(user.password, new_pass);
            write(fd, &user, sizeof(User));
            lock_user_record(fd, record_num, F_UNLCK); 
            send(client_socket, "Success: Password changed.\n", 27, 0);
            break;
        }
        record_num++;
    }
    if (!user_found) send(client_socket, "Error: User not found.\n", 23, 0);
    close(fd);
}

// --- Admin Feature 1: Add New Employee ---
static void admin_add_employee(int client_socket) {
    User new_user;
    char role_choice[10];
    send(client_socket, "Add (1) Employee or (2) Manager? ", 34, 0);
    safe_read(client_socket, role_choice, sizeof(role_choice));
    
    if (atoi(role_choice) == 2) {
        strcpy(new_user.role, "manager");
    } else {
        strcpy(new_user.role, "employee");
    }

    send(client_socket, "Enter New Employee User ID: ", 29, 0);
    safe_read(client_socket, new_user.userId, sizeof(new_user.userId));
    send(client_socket, "Enter New Employee Password: ", 30, 0);
    safe_read(client_socket, new_user.password, sizeof(new_user.password));
    send(client_socket, "Enter New Employee Name: ", 26, 0);
    safe_read(client_socket, new_user.name, sizeof(new_user.name));
    send(client_socket, "Enter 10-digit Phone Number: ", 30, 0);
    safe_read(client_socket, new_user.phone, sizeof(new_user.phone));

    int user_fd = open(USER_DB, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (user_fd == -1) {
        send(client_socket, "Error: Could not open user DB.\n", 31, 0); return;
    }
    lock_file(user_fd, F_WRLCK); 
    write(user_fd, &new_user, sizeof(User));
    lock_file(user_fd, F_UNLCK); 
    close(user_fd);
    send(client_socket, "Success: New employee added.\n", 29, 0);
}

// --- Admin Feature 2: Modify User Details ---
static void admin_modify_details(int client_socket) {
    char target_userId[20];
    send(client_socket, "Enter User ID to modify: ", 27, 0);
    safe_read(client_socket, target_userId, sizeof(target_userId));

    int fd = open(USER_DB, O_RDWR);
    if (fd == -1) {
        send(client_socket, "Error: Could not open user database.\n", 37, 0); return;
    }
    User user;
    int record_num = 0;
    int user_found = 0;
    while (read(fd, &user, sizeof(User)) == sizeof(User)) {
        if (strcmp(user.userId, target_userId) == 0) {
            user_found = 1;
            char new_name[50], new_phone[11];
            send(client_socket, "Enter new name: ", 17, 0);
            safe_read(client_socket, new_name, sizeof(new_name));

            send(client_socket, "Enter new 10-digit Phone: ", 28, 0);
            safe_read(client_socket, new_phone, sizeof(new_phone));
            
            lock_user_record(fd, record_num, F_WRLCK);
            lseek(fd, -sizeof(User), SEEK_CUR); 
            strcpy(user.name, new_name);
            strcpy(user.phone, new_phone);
            write(fd, &user, sizeof(User));
            lock_user_record(fd, record_num, F_UNLCK); 
            send(client_socket, "Success: User details updated.\n", 31, 0);
            break;
        }
        record_num++;
    }
    if (!user_found) send(client_socket, "Error: User not found.\n", 23, 0);
    close(fd);
}

// --- Admin Feature 3: Manage User Roles ---
static void admin_manage_role(int client_socket) {
    char target_userId[20], new_role[20];
    send(client_socket, "Enter Target User ID: ", 22, 0);
    safe_read(client_socket, target_userId, sizeof(target_userId));
    send(client_socket, "Enter New Role: ", 16, 0);
    safe_read(client_socket, new_role, sizeof(new_role));

    int fd = open(USER_DB, O_RDWR);
    if (fd == -1) {
        send(client_socket, "Error: Could not open user database.\n", 37, 0); return;
    }
    User user;
    int record_num = 0;
    int user_found = 0;
    while (read(fd, &user, sizeof(User)) == sizeof(User)) {
        if (strcmp(user.userId, target_userId) == 0) {
            user_found = 1;
            lock_user_record(fd, record_num, F_WRLCK);
            lseek(fd, -sizeof(User), SEEK_CUR); 
            strcpy(user.role, new_role);
            write(fd, &user, sizeof(User));
            lock_user_record(fd, record_num, F_UNLCK); 
            send(client_socket, "Success: User role updated.\n", 29, 0);
            break;
        }
        record_num++;
    }
    if (!user_found) send(client_socket, "Error: User not found.\n", 23, 0);
    close(fd);
}


// --- Main Admin Menu (FINAL) ---
void handle_admin_menu(int client_socket, User* user) {
    
    while(1) {
        char buffer[1024];
        char menu[] = "\n--- Admin Menu ---\n"
                      "1. Add New Bank Employee\n"
                      "2. Modify Customer/Employee Details\n"
                      "3. Manage User Roles\n"
                      "4. Change Password\n"
                      "5. Logout\nChoice: ";
        send(client_socket, menu, strlen(menu), 0);
        
        if (safe_read(client_socket, buffer, sizeof(buffer)) <= 0) return;
        
        if (strcmp(buffer, "") == 0) {
            continue; 
        }
        
        int choice = atoi(buffer);

        switch (choice) {
            case 1: admin_add_employee(client_socket); break;
            case 2: admin_modify_details(client_socket); break;
            case 3: admin_manage_role(client_socket); break;
            case 4: user_change_password(client_socket, user->userId); break;
            case 5: 
                send(client_socket, "Logging out...\n", 15, 0); 
                sleep(1);
                return;
            default: send(client_socket, "Invalid choice.\n", 16, 0);
        }
    }
}