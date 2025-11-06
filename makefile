CC = gcc
# Removed -Werror to prevent stopping on non-critical warnings
CFLAGS = -Wall -g 
LDFLAGS = -lpthread        # Link the pthread library 

# List of object files
OBJS = server.o admin_handler.o manager_handler.o customer_handler.o employee_handler.o

# The final executable
TARGET = server

# Default rule: build the target
all: $(TARGET)

# Rule to link the final executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Rule to compile server.c
server.o: server.c common.h
	$(CC) $(CFLAGS) -c server.c

# Rule to compile admin_handler.c
admin_handler.o: admin_handler.c common.h
	$(CC) $(CFLAGS) -c admin_handler.c

# Rule to compile manager_handler.c
manager_handler.o: manager_handler.c common.h
	$(CC) $(CFLAGS) -c manager_handler.c

# Rule to compile customer_handler.c
customer_handler.o: customer_handler.c common.h
	$(CC) $(CFLAGS) -c customer_handler.c

# Rule to compile employee_handler.c
employee_handler.o: employee_handler.c common.h
	$(CC) $(CFLAGS) -c employee_handler.c

# Rule to clean up build files
clean:
	rm -f $(TARGET) $(OBJS) *.dat