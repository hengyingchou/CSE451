//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include <cdefs.h>
#include <defs.h>
#include <fcntl.h>
#include <file.h>
#include <fs.h>
#include <mmu.h>
#include <param.h>
#include <proc.h>
#include <sleeplock.h>
#include <spinlock.h>
#include <stat.h>

int sys_dup(void) {
  // LAB1
   int fd; 
  if (argint(0, &fd) < 0) {
    return -1;
  }
  return filedup(fd);

}

int sys_read(void) {
  // LAB1
  int fd;
  char *buffer;
  int num_byte;
  if(argint(0, &fd) < 0 || argstr(1, &buffer) < 0 || 
      argint(2, &num_byte) < 0 || argptr(1, &buffer, num_byte) < 0){

    return -1;

  }else{

    return fileread(fd, buffer, num_byte);

  } 
}

int sys_write(void) {
  

  int fd; 
  char *buffer;
  int write_num; 

   if(argint(0,&fd)<0|| argint(2, &write_num) < 0|| 
     argptr(1, &buffer, write_num) < 0||  argstr(1,&buffer)<0)
    return -1;
  return filewrite(fd, buffer, write_num);

}

int sys_close(void) {
  // LAB1
  int fd; 
  if (argint(0, &fd) < 0) {
    return -1;
  }
  return fileclose(fd);

  return -1;
}

int sys_fstat(void) {
  // LAB1
  int fd; 
  struct stat *stats; 
  if (argint(0, &fd) < 0) {
    return -1;
  }
  if(argptr(1, (char**)(&stats), sizeof(stats)) == -1) {  // make sure that the range is aviliable
    return -1;
  }
    
  return filestat(fd, stats);

}

int sys_open(void) {
  //LAB1
  
  char *path;
  int mode;


  if(argstr(0,&path) < 0 || argint(1,&mode) < 0 || mode == O_CREATE){
      return -1;
  }

  // struct inode *ptr = namei(path);
  //  if (mode != O_RDONLY && mode != O_WRONLY && mode != O_RDWR && mode != O_CREATE && 
  //     (mode != (O_CREATE | O_RDWR))) {
  //   return -1;
  // }
  // if ((mode & 0xF00) == O_CREATE && ptr == NULL) {
  //   cprintf("sys_open file creattion\n");
  //   ptr =filecreate(path);
  //   cprintf("\ninode file dev %d inum %d\n",ptr->dev,ptr->inum);
  // }

  return fileopen(path, mode);

  
//  return -1;

}

int sys_exec(void) {
  // LAB2

  // char* path;
  // char *arg[MAXARG];
  // int address;
  // int end = 0;
  // int location;

  //  if(argstr(0, &path) < 0 )return -1;
  //   cprintf(1,"this is me\n");
  //  if((argptr(0, &arg, 8) < 0)) return -1;

  // for( ; end < MAXARG ; end++){

  //   if(fetchint(address + 8*end, &location) < 0 || (fetchstr(location, &arg[end]) < 0) ) return -1;
  //   if(arg[end] == NULL) return exec(end, path, arg);

  // }
  // return -1;

  //return exec(path, arg);

  char *path; //path to executable file
  char *args[MAXARG]; // array of strings for arguments - 1-D array of pointers to char - max constant

  // arg0 points to invalid or unmapped address
  if(argstr(0, &path) < 0)
     return -1;

  if(argptr(0, &path, strlen(path)) < 0)
    return -1;
 
  int address;
  if(argint(1, &address) < 0)
    return -1;

  //Cycle through until null argument found
  for(int i=0; i< MAXARG; i++) {

    //get address of each argument
    int aa;
    if(fetchint(address + 8*i, &aa) < 0) //if invalid
      return -1;

    //get argument using its address
    if(fetchstr(aa, &args[i]) < 0)
      return -1;

    if(args[i] == NULL) {
       return exec(i, path, args);
    }
  } // end for loop

  return -1; //error 

}

int sys_pipe(void) {
  // LAB2
  // pipe fds
  int *fds;
  // check 
  if(argptr(0, (char**) &fds, sizeof(int) *2) < 0){
    return -1;
  }
  return pipe(fds);
}

int sys_unlink(void) {
  // LAB 4
  return -1;
}
