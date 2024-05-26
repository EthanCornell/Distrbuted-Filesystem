# Distributed Filesystem HOWTOUSE

This document provides step-by-step instructions on how to compile, set up, and run the distributed file server and client.

## Prerequisites

- Ensure you have a C compiler (e.g., `gcc`) installed on your system.
- Ensure you have `make` installed for managing the build process.

## Files and Directories

- `ufs.h`: Header file containing file system structures and definitions.
- `udp.c`: Server implementation for handling UDP requests.
- `mkfs.c`: Utility for creating and initializing the file system image.
- `mfs.h`: Header file for client library function prototypes.
- `mfs.c`: Client library implementation.
- `client.c`: Client application for testing the file system.
- `Makefile`: Makefile for compiling the project.

## Compilation

1. Open a terminal and navigate to the project directory.
2. Run the following command to compile the project:
   ```sh
   make
   ```

This command will generate the following binaries:
- `server`: The distributed file server.
- `mkfs`: Utility to create and initialize the file system image.
- `client`: The client application.
- `libmfs.so`: The client library.

## Creating a File System Image

Before running the server, you need to create a file system image. Run the following command:
```sh
make create_fs_image
```

This command will create a file system image file named `fs_image.img` initialized with the necessary file system structures.

## Running the Server

To start the server, run the following command:
```sh
make run_server
```

This command will start the server on port `12345` and use `fs_image.img` as the file system image.

## Running the Client

To run the client, use the following command:
```sh
make run_client
```

This command will:
1. Create a directory named `client_directory`.
2. Copy `libmfs.so` and `client` into `client_directory`.
3. Set `LD_LIBRARY_PATH` to include `client_directory`.
4. Run the `client` executable.

## Testing the Client

The client will perform several file system operations, including:
- Creating a directory named `newdir` in the root directory.
- Looking up the inode number of `newdir`.
- Creating a file named `newfile` in `newdir`.
- Writing to `newfile`.
- Reading from `newfile`.
- Shutting down the server.

The client will print the results of each operation to the console.

## Cleaning Up

To clean up the compiled binaries and object files, run:
```sh
make clean
```

This command will remove all generated files, including `server`, `mkfs`, `client`, `libmfs.so`, and object files.

## Troubleshooting

If you encounter issues, ensure that:
- The server is running before starting the client.
- The `fs_image.img` file is created and accessible.
- The correct port number is used.
- `LD_LIBRARY_PATH` is set correctly for the client to find `libmfs.so`.

For detailed error messages, check the server and client logs.

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

## Author

Ethan Huang

Feel free to contact the author for any further questions or support.
