#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>     // For system calls (read, write, close, lseek)
#include <fcntl.h>      // For file control (open, fcntl)
#include <sys/socket.h> // For sockets
#include <netinet/in.h> // For sockaddr_in
#include <pthread.h>    // For threads
#include <ctype.h>      // For isspace()

#define PORT 8080

// Database files
#define USER_DB "users.dat"
#define ACCT_DB "accounts.dat"
#define LOAN_DB "loans.dat"
#define TRANSACTION_DB "transactions.dat"
#define FEEDBACK_DB "feedback.dat"

// ==== Data Structures ====

typedef struct User {
    char userId[20];
    char password[20];
    char role[20]; // "customer", "employee", "manager", "admin"
    char name[50];
    char phone[11]; // 10 digits + null terminator
} User;

typedef struct Account {
    char accountId[20]; // Same as userId for customers
    double balance;
    int isActive; // 1 for true, 0 for false
} Account;

typedef struct Loan {
    int loanId;
    char userId[20];
    double amount;
    char status[20]; // "Pending", "Approved", "Rejected"
    char assignedTo[20]; // Employee ID, or "None"
} Loan;

typedef struct Transaction {
    int transactionId;
    char userId[20]; // The user who initiated
    char type[20]; // "Deposit", "Withdraw", "Transfer"
    double amount;
    char otherUser[20]; // "None" or the other user in a transfer
} Transaction;

typedef struct Feedback {
    int feedbackId;
    char userId[20];
    char message[256];
    int isReviewed; // 0 for false, 1 for true
    char actionTaken[256];
} Feedback;


// ==== Function Prototypes for Handlers ====
void handle_admin_menu(int client_socket, User* user);
void handle_manager_menu(int client_socket, User* user);
void handle_customer_menu(int client_socket, User* user);
void handle_employee_menu(int client_socket, User* user);


// ==== Utility Function Prototypes ====
void lock_file(int fd, short lock_type);
void lock_user_record(int fd, int record_num, short lock_type);
void lock_account_record(int fd, int record_num, short lock_type);
void lock_loan_record(int fd, int record_num, short lock_type);
void lock_feedback_record(int fd, int record_num, short lock_type);

// Shared helper
void log_transaction(char* userId, char* type, double amount, char* otherUser);

// Shared Customer functions (called by Employee)
void customer_update_balance(int client_socket, char* customer_userId, double amount, const char* type);
void customer_view_transactions(int client_socket, char* customer_userId);

#endif