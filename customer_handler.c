#include "common.h"

// --- HELPER FUNCTION: Removes whitespace ---
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

// --- HELPER FUNCTION: Reads data safely ---
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

// --- Customer Feature 1: View Balance ---
static void customer_view_balance(int client_socket, char* customer_userId) {
    int fd = open(ACCT_DB, O_RDONLY);
    if (fd == -1) {
        send(client_socket, "Error: Could not open account database.\n", 40, 0); return;
    }
    Account acct;
    int record_num = 0;
    int acct_found = 0;
    while (read(fd, &acct, sizeof(Account)) == sizeof(Account)) {
        if (strcmp(acct.accountId, customer_userId) == 0) {
            acct_found = 1;
            lock_account_record(fd, record_num, F_RDLCK); 
            char balance_msg[100];
            snprintf(balance_msg, sizeof(balance_msg), "Your current balance is: %.2f\n", acct.balance);
            send(client_socket, balance_msg, strlen(balance_msg), 0);
            lock_account_record(fd, record_num, F_UNLCK);
            break;
        }
        record_num++;
    }
    if (!acct_found) send(client_socket, "Error: Account not found.\n", 25, 0);
    close(fd);
}

// --- Customer Feature 2 & 3: Deposit & Withdraw ---
void customer_update_balance(int client_socket, char* customer_userId, double amount, const char* type) {
    if (amount <= 0) {
        send(client_socket, "Error: Amount must be positive.\n", 33, 0); return;
    }
    int fd = open(ACCT_DB, O_RDWR);
    if (fd == -1) {
        send(client_socket, "Error: Could not open account database.\n", 40, 0); return;
    }
    Account acct;
    int record_num = 0;
    int acct_found = 0;
    while (read(fd, &acct, sizeof(Account)) == sizeof(Account)) {
        if (strcmp(acct.accountId, customer_userId) == 0) {
            acct_found = 1;
            lock_account_record(fd, record_num, F_WRLCK); 
            
            if (strcmp(type, "Withdraw") == 0 && acct.balance < amount) {
                send(client_socket, "Error: Insufficient funds.\n", 28, 0);
                lock_account_record(fd, record_num, F_UNLCK);
                close(fd);
                return;
            }
            if (!acct.isActive) {
                send(client_socket, "Error: Account is deactivated.\n", 30, 0);
                lock_account_record(fd, record_num, F_UNLCK);
                close(fd);
                return;
            }

            acct.balance += (strcmp(type, "Deposit") == 0) ? amount : -amount;
            lseek(fd, -sizeof(Account), SEEK_CUR); 
            write(fd, &acct, sizeof(Account));
            lock_account_record(fd, record_num, F_UNLCK); 

            char success_msg[100];
            snprintf(success_msg, sizeof(success_msg), "Success. New balance is: %.2f\n", acct.balance);
            send(client_socket, success_msg, strlen(success_msg), 0);
            
            log_transaction(customer_userId, (char*)type, amount, "None");
            break;
        }
        record_num++;
    }
    if (!acct_found) send(client_socket, "Error: Account not found.\n", 25, 0);
    close(fd);
}

// --- Customer Feature 4: Transfer Funds ---
static void customer_transfer_funds(int client_socket, char* from_userId) {
    char to_userId[20], amount_str[20];
    send(client_socket, "Enter User ID to transfer to: ", 30, 0);
    safe_read(client_socket, to_userId, sizeof(to_userId));
    send(client_socket, "Enter Amount to transfer: ", 27, 0);
    safe_read(client_socket, amount_str, sizeof(amount_str));
    double amount = atof(amount_str);

    if (amount <= 0) {
        send(client_socket, "Error: Amount must be positive.\n", 33, 0); return;
    }
    if (strcmp(from_userId, to_userId) == 0) {
        send(client_socket, "Error: Cannot transfer to yourself.\n", 36, 0); return;
    }

    int fd = open(ACCT_DB, O_RDWR);
    if (fd == -1) {
        send(client_socket, "Error: Could not open account database.\n", 40, 0); return;
    }

    Account from_acct, to_acct;
    int from_rec = -1, to_rec = -1;
    int record_num = 0;
    Account temp_acct;

    lseek(fd, 0, SEEK_SET);
    while (read(fd, &temp_acct, sizeof(Account)) == sizeof(Account)) {
        if (strcmp(temp_acct.accountId, from_userId) == 0) from_rec = record_num;
        if (strcmp(temp_acct.accountId, to_userId) == 0) to_rec = record_num;
        if (from_rec != -1 && to_rec != -1) break;
        record_num++;
    }

    if (from_rec == -1) {
        send(client_socket, "Error: Your account not found.\n", 30, 0); close(fd); return;
    }
    if (to_rec == -1) {
        send(client_socket, "Error: Recipient account not found.\n", 37, 0); close(fd); return;
    }

    lock_account_record(fd, (from_rec < to_rec) ? from_rec : to_rec, F_WRLCK);
    lock_account_record(fd, (from_rec > to_rec) ? from_rec : to_rec, F_WRLCK);

    lseek(fd, from_rec * sizeof(Account), SEEK_SET);
    read(fd, &from_acct, sizeof(Account));
    lseek(fd, to_rec * sizeof(Account), SEEK_SET);
    read(fd, &to_acct, sizeof(Account));

    if (!from_acct.isActive) {
        send(client_socket, "Error: Your account is deactivated.\n", 36, 0);
    } else if (!to_acct.isActive) {
        send(client_socket, "Error: Recipient account is deactivated.\n", 41, 0);
    } else if (from_acct.balance < amount) {
        send(client_socket, "Error: Insufficient funds.\n", 28, 0);
    } else {
        from_acct.balance -= amount;
        to_acct.balance += amount;

        lseek(fd, from_rec * sizeof(Account), SEEK_SET);
        write(fd, &from_acct, sizeof(Account));
        lseek(fd, to_rec * sizeof(Account), SEEK_SET);
        write(fd, &to_acct, sizeof(Account));

        log_transaction(from_userId, "Transfer", amount, to_userId);
        
        char success_msg[100];
        snprintf(success_msg, sizeof(success_msg), "Success. New balance is: %.2f\n", from_acct.balance);
        send(client_socket, success_msg, strlen(success_msg), 0);
    }

    lock_account_record(fd, (from_rec > to_rec) ? from_rec : to_rec, F_UNLCK);
    lock_account_record(fd, (from_rec < to_rec) ? from_rec : to_rec, F_UNLCK);
    close(fd);
}

// --- Customer Feature 5: Apply for Loan ---
static void customer_apply_loan(int client_socket, char* customer_userId) {
    char amount_str[20];
    send(client_socket, "Enter Loan Amount: ", 21, 0);
    safe_read(client_socket, amount_str, sizeof(amount_str));
    double amount = atof(amount_str);
    if (amount <= 0) {
        send(client_socket, "Error: Loan amount must be positive.\n", 37, 0); return;
    }
    Loan new_loan;
    new_loan.loanId = (int)time(NULL); 
    strcpy(new_loan.userId, customer_userId);
    new_loan.amount = amount;
    strcpy(new_loan.status, "Pending");
    strcpy(new_loan.assignedTo, "None");
    int fd = open(LOAN_DB, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        send(client_socket, "Error: Could not open loan database.\n", 38, 0); return;
    }
    lock_file(fd, F_WRLCK);
    write(fd, &new_loan, sizeof(Loan));
    lock_file(fd, F_UNLCK);
    close(fd);
    
    char success_msg[100];
    snprintf(success_msg, sizeof(success_msg), "Loan app submitted. Your new Loan ID is: %d\n", new_loan.loanId);
    send(client_socket, success_msg, strlen(success_msg), 0);
}

// --- Customer Feature 6: Add Feedback ---
static void customer_add_feedback(int client_socket, char* customer_userId) {
    char msg_buffer[256];
    send(client_socket, "Enter Feedback Message: ", 25, 0);
    safe_read(client_socket, msg_buffer, sizeof(msg_buffer));

    Feedback new_feedback;
    new_feedback.feedbackId = (int)time(NULL);
    strcpy(new_feedback.userId, customer_userId);
    strcpy(new_feedback.message, msg_buffer);
    new_feedback.isReviewed = 0;
    strcpy(new_feedback.actionTaken, "None");

    int fd = open(FEEDBACK_DB, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        send(client_socket, "Error: Could not open feedback database.\n", 41, 0); return;
    }
    lock_file(fd, F_WRLCK);
    write(fd, &new_feedback, sizeof(Feedback));
    lock_file(fd, F_UNLCK);
    close(fd);
    
    char success_msg[100];
    snprintf(success_msg, sizeof(success_msg), "Feedback submitted. Your new Feedback ID is: %d\n", new_feedback.feedbackId);
    send(client_socket, success_msg, strlen(success_msg), 0);
}

// --- Customer Feature 7: View Transaction History ---
void customer_view_transactions(int client_socket, char* customer_userId) {
    int fd = open(TRANSACTION_DB, O_RDONLY);
    if (fd == -1) {
        send(client_socket, "No transactions found.\n", 24, 0); return;
    }
    
    send(client_socket, "\n--- Your Transaction History ---\n", 34, 0);
    Transaction trans;
    char trans_line[256];
    int found = 0;

    lock_file(fd, F_RDLCK);
    lseek(fd, 0, SEEK_SET);
    while (read(fd, &trans, sizeof(Transaction)) == sizeof(Transaction)) {
        if (strcmp(trans.userId, customer_userId) == 0 || strcmp(trans.otherUser, customer_userId) == 0) {
            found = 1;
            if (strcmp(trans.type, "Transfer") == 0) {
                if (strcmp(trans.userId, customer_userId) == 0) {
                    snprintf(trans_line, sizeof(trans_line), "ID %d: Sent %.2f to %s\n", trans.transactionId, trans.amount, trans.otherUser);
                } else {
                    snprintf(trans_line, sizeof(trans_line), "ID %d: Received %.2f from %s\n", trans.transactionId, trans.amount, trans.userId);
                }
            } else {
                snprintf(trans_line, sizeof(trans_line), "ID %d: %s of %.2f\n", trans.transactionId, trans.type, trans.amount);
            }
            send(client_socket, trans_line, strlen(trans_line), 0);
        }
    }
    lock_file(fd, F_UNLCK);
    close(fd);

    if (!found) {
        send(client_socket, "No transactions found.\n", 24, 0);
    }
}

// --- Customer Feature 8: View Feedback Status ---
static void customer_view_feedback_status(int client_socket, char* customer_userId) {
    int fd = open(FEEDBACK_DB, O_RDONLY);
    if (fd == -1) {
        send(client_socket, "No feedback found.\n", 21, 0); return;
    }
    
    send(client_socket, "\n--- Your Feedback History ---\n", 31, 0);
    Feedback fb;
    char fb_line[1024]; 
    int found = 0;

    lock_file(fd, F_RDLCK);
    lseek(fd, 0, SEEK_SET);
    while (read(fd, &fb, sizeof(Feedback)) == sizeof(Feedback)) {
        if (strcmp(fb.userId, customer_userId) == 0) {
            found = 1;
            if (fb.isReviewed) {
                snprintf(fb_line, sizeof(fb_line), "ID %d: '%s' -> Reviewed. Action: %s\n", fb.feedbackId, fb.message, fb.actionTaken);
            } else {
                snprintf(fb_line, sizeof(fb_line), "ID %d: '%s' -> Pending Review\n", fb.feedbackId, fb.message);
            }
            send(client_socket, fb_line, strlen(fb_line), 0);
        }
    }
    lock_file(fd, F_UNLCK);
    close(fd);

    if (!found) {
        send(client_socket, "No feedback found.\n", 21, 0);
    }
}


// --- Main Customer Menu (FINAL) ---
void handle_customer_menu(int client_socket, User* user) {
    
    while(1) {
        char buffer[1024];
        char menu[] = "\n--- Customer Menu ---\n"
                      "1. View Account Balance\n"
                      "2. Deposit Money\n"
                      "3. Withdraw Money\n"
                      "4. Transfer Funds\n"
                      "5. Apply for a Loan\n"
                      "6. Add Feedback\n"
                      "7. View Transaction History\n"
                      "8. View Feedback Status\n"
                      "9. Change Password\n"
                      "10. Logout\nChoice: ";
        send(client_socket, menu, strlen(menu), 0);
        
        if (safe_read(client_socket, buffer, sizeof(buffer)) <= 0) return;
        
        if (strcmp(buffer, "") == 0) {
            continue; 
        }
        
        int choice = atoi(buffer);

        switch (choice) {
            case 1: customer_view_balance(client_socket, user->userId); break;
            case 2: {
                char amount_str[20];
                send(client_socket, "Enter Amount to Deposit: ", 25, 0);
                if(safe_read(client_socket, amount_str, sizeof(amount_str)) <= 0) break;
                double amount = atof(amount_str);
                customer_update_balance(client_socket, user->userId, amount, "Deposit");
                break;
            }
            case 3: {
                char amount_str[20];
                send(client_socket, "Enter Amount to Withdraw: ", 26, 0);
                if(safe_read(client_socket, amount_str, sizeof(amount_str)) <= 0) break;
                double amount = atof(amount_str);
                customer_update_balance(client_socket, user->userId, amount, "Withdraw");
                break;
            }
            case 4: customer_transfer_funds(client_socket, user->userId); break;
            case 5: customer_apply_loan(client_socket, user->userId); break;
            case 6: customer_add_feedback(client_socket, user->userId); break;
            case 7: customer_view_transactions(client_socket, user->userId); break;
            case 8: customer_view_feedback_status(client_socket, user->userId); break;
            case 9: user_change_password(client_socket, user->userId); break;
            case 10: 
                send(client_socket, "Logging out...\n", 15, 0); 
                sleep(1);
                return; 
            default: send(client_socket, "Invalid choice.\n", 16, 0);
        }
    } 
}