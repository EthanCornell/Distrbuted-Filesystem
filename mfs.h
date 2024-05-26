#ifndef MFS_H
#define MFS_H

#define MFS_DIRECTORY (0)
#define MFS_REGULAR_FILE (1)
#define MFS_BLOCK_SIZE (4096)
#define BUFFER_SIZE 1024


typedef struct {
    int type;   // MFS_DIRECTORY or MFS_REGULAR
    int size;   // bytes
    unsigned int direct[14]; // pointers to data blocks
} MFS_Stat_t;

int MFS_Init(char *hostname, int port);
int MFS_Lookup(int pinum, char *name);
int MFS_Stat(int inum, MFS_Stat_t *m);
int MFS_Write(int inum, char *buffer, int block);
int MFS_Read(int inum, char *buffer, int block);
int MFS_Creat(int pinum, int type, char *name);
int MFS_Unlink(int pinum, char *name);
int MFS_Shutdown();

#endif // MFS_H
