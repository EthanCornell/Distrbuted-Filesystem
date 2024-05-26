# Compiler and compiler flags
CC = gcc
CFLAGS = -Wall -Wextra -fPIC
LDFLAGS = -shared

# Source files
MFS_SRC = mfs.c
SERVER_SRC = udp.c
MKFS_SRC = mkfs.c
CLIENT_SRC = client.c

# Header files
HEADERS = ufs.h mfs.h

# Output files
LIBMFS = libmfs.so
SERVER = server
MKFS = mkfs
CLIENT = client

# Object files
MFS_OBJ = $(MFS_SRC:.c=.o)
SERVER_OBJ = $(SERVER_SRC:.c=.o)
MKFS_OBJ = $(MKFS_SRC:.c=.o)
CLIENT_OBJ = $(CLIENT_SRC:.c=.o)

# Default target
all: $(LIBMFS) $(SERVER) $(MKFS) $(CLIENT)

# Compile the client library
$(LIBMFS): $(MFS_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

# Compile the server
$(SERVER): $(SERVER_OBJ)
	$(CC) -o $@ $^

# Compile the file system image creator
$(MKFS): $(MKFS_OBJ)
	$(CC) -o $@ $^

# Compile the client
$(CLIENT): $(CLIENT_OBJ) $(LIBMFS)
	$(CC) -o $@ $^ -L. -lmfs -Wl,-rpath,.

# Compile object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f $(LIBMFS) $(SERVER) $(MKFS) $(CLIENT) $(MFS_OBJ) $(SERVER_OBJ) $(MKFS_OBJ) $(CLIENT_OBJ)
	rm -rf client_directory/
	rm -f fs_image.img

# Run the server (example usage)
run_server:
	./server 12345 fs_image.img

# Create a file system image (example usage)
create_fs_image:
	./mkfs -f fs_image.img -d 32 -i 32

# Run the client (example usage)
run_client: $(CLIENT)
	mkdir -p client_directory
	cp $(LIBMFS) client_directory/
	cp $(CLIENT) client_directory/
	cd client_directory && ./client
	
# Set LD_LIBRARY_PATH and run the client	
# run_client: $(CLIENT)
# 	export LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:. && ./client

.PHONY: all clean run_server create_fs_image run_client
