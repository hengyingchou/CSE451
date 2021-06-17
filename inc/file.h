#pragma once

#include <extent.h>
#include <sleeplock.h>
#include<fs.h>
// in-memory copy of an inode
struct inode {
  uint dev;  // Device number
  uint inum; // Inode number
  int ref;   // Reference count
  int valid; // Flag for if node is valid
  struct sleeplock lock;

  short type; // copy of disk inode
  short devid;
  uint size;
  struct extent data[EXTENT_N];
};

// table mapping device ID (devid) to device functions
struct devsw {
  int (*read)(struct inode *, char *, int);
  int (*write)(struct inode *, char *, int);
};

struct pipe{
  struct spinlock lock; // pipe lock 
  int read_fd;          // fd for read
  int write_fd;         // fd for write
  int head;
  int tail; 
  char buffer[2048];

};

struct file_info{
  struct spinlock lock;
  int memory_count;
  struct inode * inode;
  int curr_offset;
  int access_type;
  char* path;
  bool using ;
  // parameter for file for pipe
  bool inPipe;          // assign file as pipe
  struct pipe pipe_ptr; // pointer to the pipe
};

struct file_dec {
  struct file_info* kern_file; 
  bool using ;
};

extern struct devsw devsw[];

// Device ids
enum {
  CONSOLE = 1,
};

int fileopen(char *path, int mode);
int fileread(int fd, char *buffer, int num_byte);
int filewrite(int fd, char *buffer, int write_num);
int fileclose(int fd);
int filestat(int fd, struct stat *stats);
int filedup(int fd);
int pipe(int *fds);

//struct inode* filecreate(char*path);




