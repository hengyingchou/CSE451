# Lab 2 Design Doc: Multiprocessing

## Overview
The goal of this lab is to allow multiprocessing in xk. To create this function, xk should be able to allow existence of multiple different process that can run independently from each other. Also, to allow connection between processes, we will also implement pipe that is similar to file system but designed for concurrentcy usage. All functionalities are designed based on classic UNIX system calls, such as fork(), wait(), exit(), exec().  

## In-depth Analysis and Implementation

###  Synchronization issues:
- Should prevent to read or wrirte from inode directly, use concurrent functions instead.
- To prevent data corruption during concurrent excution, we will use locks. Lab2.md demonstrated using spinlock to protect process table. 
- For our own design, we need to protect the shared kernel_opend_file table with lock. We want to use spinlock since it is more secured and we are expected for a short wait.
- Handy functions for setting process state: wake1() can wake up all waiters and sleep() can release a spinlock of a process so that the thread can be reused.

### fork: 
fork() copies the entire process, it also creates a parent-child relationship to define memory mangement resposibilities. 
To implement fork(), we set up following steps:
- Implement code in `proc.c:fork()`
- A new entry in the process table must be created via `allocproc`
- User memory must be duplicated via `vspacecopy`
- The trapframe must be duplicated in the new process
- All the opened files must be duplicated in the new process (not as simple as a memory copy)
- Set the state of the new process to be `RUNNABLE`
- Return 0 in the child, while returning the child's pid in the parent

### wait:
wait() allows a process to wait for the result of it children. It can be handy for memory mangement, for example, since children cannot clean up its own memory after it terminites, wait() function can remaind the parent that a ceratin childern is finished and clean up its memory. 
- Implement code in `proc.c:wait()`
- Scan though the global process table and see if there is a process that has the current process as perent.
- Waits for a child to end its excution, either by eixt() or kill(). (Hangs there if all its child are still excecuting)
- Clear up the end child's memory. 
- Returns -1 if not finding a child or the pid of the ended child process.

### exit:
exit() serves as terminites a process by itself and wake up its parent process by setting the state of parent process as `RUNNABLE`. By exiting a process, this means the memory space of this process can be clear for future processes.
- Implement code in `proc.c: exit()`
- Wakes up its parent 
- The state should be set in `zombie` to indicate that it should be clear.
- After this `zombie` process is cleared by other process (parent or next process), remove it from ptable.
- Returns nothing.

### exec:
exec() is the function that allows a process to run its own code, normally we use fork() to claim a memory space then a exec() to load code instruction.
- Implement code in `exec.c: exec()`.
- Rewrites the memory space of the current process and load code from an executable file using `vspaceloadcode`.
- Set up the initial stack using `vspaceinitstack`.
- Arguments needed for stack should be deep copied using `vspacewritetova`.

### pipe:
Pipe is used for inter process communication. It has similar mechanism as file, thus, we will modify function in `file.c` to introduce pipe function.
- Implement pipe in `file.c`
- Pipe_open(int fid1, int fid2): create two files descriptor in process file table and pointing to a pipe in global file info.
- Pipe_read(int fd, char* buffer, int offset):  acquire a lock and write to pipe. After that, release lock.
- Pipe_write(int fd, char* buf, int offset):  acquire a lock and read to pipe. After that, release lock.
- Need to check whether the file is in pipe. If it is in pipe, we need to use Pipe_read or Pipe_write to read or write the file. 
Otherwise, we use fileopen or fileread from lab 1 to read or write the file. 

## Risk Analysis
- What happened if two file descriptor are pointing to the same file? do we need to change the lock?
- When a parent is terminated before child, who should be response of cleaning up the memory. Now we want to let each process check for unremoved memory, is there a faster way to do this?
