#include "common.h"

int main() {
    int fd;

    // --- 1. Create Users ---
    // O_TRUNC = Overwrite the file if it already exists
    fd = open(USER_DB, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { perror("Could not create users.dat"); return 1; }
    
    User admin;
    strcpy(admin.userId, "admin");
    strcpy(admin.password, "admin123");
    strcpy(admin.role, "admin");
    strcpy(admin.name, "Administrator");
    strcpy(admin.phone, "1111111111"); // Added phone
    write(fd, &admin, sizeof(User));
    
    User manager;
    strcpy(manager.userId, "manager");
    strcpy(manager.password, "man123");
    strcpy(manager.role, "manager");
    strcpy(manager.name, "Bank Manager");
    strcpy(manager.phone, "2222222222"); // Added phone
    write(fd, &manager, sizeof(User));

    User emp;
    strcpy(emp.userId, "emp");
    strcpy(emp.password, "emp123");
    strcpy(emp.role, "employee");
    strcpy(emp.name, "Bank Employee");
    strcpy(emp.phone, "3333333333"); // Added phone
    write(fd, &emp, sizeof(User));

    User cust1;
    strcpy(cust1.userId, "cust1");
    strcpy(cust1.password, "cust123");
    strcpy(cust1.role, "customer");
    strcpy(cust1.name, "Alice Smith");
    strcpy(cust1.phone, "4444444444"); // Added phone
    write(fd, &cust1, sizeof(User));
    
    close(fd);
    printf("Created %s\n", USER_DB);

    // --- 2. Create Accounts ---
    fd = open(ACCT_DB, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { perror("Could not create accounts.dat"); return 1; }

    Account acct1;
    strcpy(acct1.accountId, "cust1");
    acct1.balance = 5000.00;
    acct1.isActive = 1;
    write(fd, &acct1, sizeof(Account));
    
    close(fd);
    printf("Created %s\n", ACCT_DB);

    // --- 3. Create Empty Files for Loans, Transactions, Feedback ---
    fd = open(LOAN_DB, O_WRONLY | O_CREAT | O_TRUNC, 0644); close(fd);
    printf("Created empty %s\n", LOAN_DB);
    
    fd = open(TRANSACTION_DB, O_WRONLY | O_CREAT | O_TRUNC, 0644); close(fd);
    printf("Created empty %s\n", TRANSACTION_DB);
    
    fd = open(FEEDBACK_DB, O_WRONLY | O_CREAT | O_TRUNC, 0644); close(fd);
    printf("Created empty %s\n", FEEDBACK_DB);

    printf("Data files created successfully.\n");
    return 0;
}