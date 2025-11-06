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
static int safe_read(int sock, char *buffer, int len) {
    char temp_buf[1024];
    memset(temp_buf, 0, sizeof(temp_buf));
    
    int read_val = read(sock, temp_buf, sizeof(temp_buf) - 1);
    if (read_val <= 0) return read_val;
    
    temp_buf[read_val] = '\0';
    trim_whitespace(temp_buf);
    
    strncpy(buffer, temp_buf, len - 1);
    buffer[len - 1] = '\0';
    
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

// --- Manager Feature 1: Activate/Deactivate Account ---
static void manager_set_account_status(int client_socket, int status) {
    char target_acctId[20];
    send(client_socket, "Enter Target Account ID: ", 25, 0);
    safe_read(client_socket, target_acctId, sizeof(target_acctId));
    
    int fd = open(ACCT_DB, O_RDWR);
    if (fd == -1) {
        send(client_socket, "Error: Could not open account database.\n", 40, 0); return;
    }
    Account acct;
    int record_num = 0;
    int acct_found = 0;
    while (read(fd, &acct, sizeof(Account)) == sizeof(Account)) {
        if (strcmp(acct.accountId, target_acctId) == 0) {
            acct_found = 1;
            lock_account_record(fd, record_num, F_WRLCK); 
            lseek(fd, -sizeof(Account), SEEK_CUR);
            acct.isActive = status;
            write(fd, &acct, sizeof(Account));
            lock_account_record(fd, record_num, F_UNLCK); 
            if (status == 1)
                send(client_socket, "Success: Account activated.\n", 28, 0);
            else
                send(client_socket, "Success: Account deactivated.\n", 30, 0);
            break;
        }
        record_num++;
    }
    if (!acct_found) send(client_socket, "Error: Account not found.\n", 25, 0);
    close(fd);
}

// --- Manager Feature 2: Assign Loan ---
static void manager_assign_loan(int client_socket) {
    char loanId_str[20], empId[20];
    send(client_socket, "Enter Loan ID to assign: ", 26, 0);
    safe_read(client_socket, loanId_str, sizeof(loanId_str));
    int loanId = atoi(loanId_str);
    
    send(client_socket, "Enter Employee ID to assign to: ", 32, 0);
    safe_read(client_socket, empId, sizeof(empId));

    int fd = open(LOAN_DB, O_RDWR);
    if (fd == -1) {
        send(client_socket, "Error: Could not open loan database.\n", 38, 0); return;
    }
    Loan loan;
    int record_num = 0;
    int loan_found = 0;
    while (read(fd, &loan, sizeof(Loan)) == sizeof(Loan)) {
        if (loan.loanId == loanId) {
            loan_found = 1;
            if (strcmp(loan.status, "Pending") != 0) {
                send(client_socket, "Error: Loan is not pending.\n", 28, 0);
                close(fd); return;
            }
            lock_loan_record(fd, record_num, F_WRLCK);
            lseek(fd, -sizeof(Loan), SEEK_CUR); 
            strcpy(loan.assignedTo, empId);
            write(fd, &loan, sizeof(Loan));
            lock_loan_record(fd, record_num, F_UNLCK); 
            send(client_socket, "Success: Loan assigned.\n", 25, 0);
            break;
        }
        record_num++;
    }
    if (!loan_found) send(client_socket, "Error: Loan ID not found.\n", 26, 0);
    close(fd);
}

// --- Manager Feature 3: Review Feedback ---
static void manager_review_feedback(int client_socket) {
    char feedbackId_str[20], action[256];
    send(client_socket, "Enter Feedback ID to review: ", 29, 0);
    safe_read(client_socket, feedbackId_str, sizeof(feedbackId_str));
    int feedbackId = atoi(feedbackId_str);
    
    send(client_socket, "Enter action taken: ", 21, 0);
    safe_read(client_socket, action, sizeof(action));

    int fd = open(FEEDBACK_DB, O_RDWR);
    if (fd == -1) {
        send(client_socket, "Error: Could not open feedback database.\n", 41, 0); return;
    }
    Feedback fb;
    int record_num = 0;
    int fb_found = 0;
    while (read(fd, &fb, sizeof(Feedback)) == sizeof(Feedback)) {
        if (fb.feedbackId == feedbackId) {
            fb_found = 1;
            lock_feedback_record(fd, record_num, F_WRLCK);
            lseek(fd, -sizeof(Feedback), SEEK_CUR); 
            strcpy(fb.actionTaken, action);
            fb.isReviewed = 1;
            write(fd, &fb, sizeof(Feedback));
            lock_feedback_record(fd, record_num, F_UNLCK); 
            send(client_socket, "Success: Feedback reviewed.\n", 29, 0);
            break;
        }
        record_num++;
    }
    if (!fb_found) send(client_socket, "Error: Feedback ID not found.\n", 30, 0);
    close(fd);
}


// --- Main Manager Menu (FINAL) ---
void handle_manager_menu(int client_socket, User* user) {
    
    while(1) {
        char buffer[1024];
        char menu[] = "\n--- Manager Menu ---\n"
                      "1. Activate Customer Account\n"
                      "2. Deactivate Customer Account\n"
                      "3. Assign Loan Application\n"
                      "4. Review Customer Feedback\n"
                      "5. Change Password\n"
                      "6. Logout\nChoice: ";
        send(client_socket, menu, strlen(menu), 0);
        
        if (safe_read(client_socket, buffer, sizeof(buffer)) <= 0) return;
        
        if (strcmp(buffer, "") == 0) {
            continue; 
        }
        
        int choice = atoi(buffer);

        switch (choice) {
            case 1: manager_set_account_status(client_socket, 1); break;
            case 2: manager_set_account_status(client_socket, 0); break;
            case 3: manager_assign_loan(client_socket); break;
            case 4: manager_review_feedback(client_socket); break;
            case 5: user_change_password(client_socket, user->userId); break;
            case 6: 
                send(client_socket, "Logging out...\n", 15, 0); 
                sleep(1);
                return;
            default: send(client_socket, "Invalid choice.\n", 16, 0);
        }
    }
}