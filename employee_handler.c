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

// --- Employee Feature 1: Add New Customer ---
static void employee_add_customer(int client_socket) {
    User new_user;
    Account new_account;
    strcpy(new_user.role, "customer");
    send(client_socket, "Enter New Customer User ID: ", 28, 0);
    safe_read(client_socket, new_user.userId, sizeof(new_user.userId));
    send(client_socket, "Enter New Customer Password: ", 29, 0);
    safe_read(client_socket, new_user.password, sizeof(new_user.password));
    send(client_socket, "Enter New Customer Name: ", 25, 0);
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

    strcpy(new_account.accountId, new_user.userId);
    new_account.balance = 0.0; 
    new_account.isActive = 1;  
    int acct_fd = open(ACCT_DB, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (acct_fd == -1) {
        send(client_socket, "Error: Could not open account DB.\n", 34, 0); return;
    }
    lock_file(acct_fd, F_WRLCK); 
    write(acct_fd, &new_account, sizeof(Account));
    lock_file(acct_fd, F_UNLCK); 
    close(acct_fd);
    send(client_socket, "Success: New customer added.\n", 29, 0);
}

// --- Employee Feature 2: Modify Customer Details ---
static void employee_modify_customer(int client_socket) {
    char target_userId[20];
    send(client_socket, "Enter Customer User ID to modify: ", 35, 0);
    safe_read(client_socket, target_userId, sizeof(target_userId));

    int fd = open(USER_DB, O_RDWR);
    if (fd == -1) {
        send(client_socket, "Error: Could not open user database.\n", 37, 0); return;
    }
    User user;
    int record_num = 0;
    int user_found = 0;
    while (read(fd, &user, sizeof(User)) == sizeof(User)) {
        if (strcmp(user.userId, target_userId) == 0 && strcmp(user.role, "customer") == 0) {
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
            send(client_socket, "Success: Customer details updated.\n", 34, 0);
            break;
        }
        record_num++;
    }
    if (!user_found) send(client_socket, "Error: Customer not found.\n", 27, 0);
    close(fd);
}

// --- Employee Feature 3/4: Approve/Reject Loans ---
static void employee_process_loan(int client_socket, char* employeeId, const char* newStatus) {
    char loanId_str[20];
    send(client_socket, "Enter Loan ID to process: ", 27, 0);
    safe_read(client_socket, loanId_str, sizeof(loanId_str));
    int loanId = atoi(loanId_str);

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
            if (strcmp(loan.assignedTo, employeeId) != 0) {
                send(client_socket, "Error: Loan is not assigned to you.\n", 36, 0);
                close(fd); return;
            }
            
            lock_loan_record(fd, record_num, F_WRLCK);
            lseek(fd, -sizeof(Loan), SEEK_CUR); 
            strcpy(loan.status, newStatus);
            write(fd, &loan, sizeof(Loan));
            lock_loan_record(fd, record_num, F_UNLCK); 

            send(client_socket, "Success: Loan status updated.\n", 30, 0);
            
            if (strcmp(newStatus, "Approved") == 0) {
                customer_update_balance(client_socket, loan.userId, loan.amount, "Deposit");
            }
            break;
        }
        record_num++;
    }
    if (!loan_found) send(client_socket, "Error: Loan ID not found.\n", 26, 0);
    close(fd);
}

// --- Employee Feature 5: View Assigned Loans ---
static void employee_view_assigned_loans(int client_socket, char* employeeId) {
    int fd = open(LOAN_DB, O_RDONLY);
    if (fd == -1) {
        send(client_socket, "No loans found.\n", 17, 0); return;
    }
    send(client_socket, "\n--- Your Assigned Loans ---\n", 30, 0);
    Loan loan;
    char loan_line[256];
    int found = 0;
    lock_file(fd, F_RDLCK);
    lseek(fd, 0, SEEK_SET);
    while (read(fd, &loan, sizeof(Loan)) == sizeof(Loan)) {
        if (strcmp(loan.assignedTo, employeeId) == 0) {
            found = 1;
            snprintf(loan_line, sizeof(loan_line), "ID %d: User %s, Amount %.2f, Status: %s\n", 
                loan.loanId, loan.userId, loan.amount, loan.status);
            send(client_socket, loan_line, strlen(loan_line), 0);
        }
    }
    lock_file(fd, F_UNLCK);
    close(fd);
    if (!found) send(client_socket, "No loans assigned to you.\n", 28, 0);
}

// --- Employee Feature 6: View Customer Transactions ---
static void employee_view_customer_transactions(int client_socket) {
    char target_userId[20];
    send(client_socket, "Enter Customer User ID to view transactions: ", 47, 0);
    safe_read(client_socket, target_userId, sizeof(target_userId));
    
    customer_view_transactions(client_socket, target_userId);
}

// --- Main Employee Menu (FINAL) ---
void handle_employee_menu(int client_socket, User* user) {
    
    while(1) {
        char buffer[1024];
        char menu[] = "\n--- Employee Menu ---\n"
                      "1. Add New Customer\n"
                      "2. Modify Customer Details\n"
                      "3. Approve Loan\n"
                      "4. Reject Loan\n"
                      "5. View Assigned Loan Applications\n"
                      "6. View Customer Transactions\n"
                      "7. Change Password\n"
                      "8. Logout\nChoice: ";
        send(client_socket, menu, strlen(menu), 0);
        
        if (safe_read(client_socket, buffer, sizeof(buffer)) <= 0) return;
        
        if (strcmp(buffer, "") == 0) {
            continue; 
        }
        
        int choice = atoi(buffer);

        switch (choice) {
            case 1: employee_add_customer(client_socket); break;
            case 2: employee_modify_customer(client_socket); break;
            case 3: employee_process_loan(client_socket, user->userId, "Approved"); break;
            case 4: employee_process_loan(client_socket, user->userId, "Rejected"); break;
            case 5: employee_view_assigned_loans(client_socket, user->userId); break;
            case 6: employee_view_customer_transactions(client_socket); break;
            case 7: user_change_password(client_socket, user->userId); break;
            case 8: 
                send(client_socket, "Logging out...\n", 15, 0); 
                sleep(1);
                return;
            default: send(client_socket, "Invalid choice.\n", 16, 0);
        }
    }
}