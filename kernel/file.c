#include <cdefs.h>
#include <defs.h>
#include <file.h>
#include <fs.h>
#include <param.h>
#include <sleeplock.h>
#include <spinlock.h>
#include <proc.h>
#include <fcntl.h>
#include <stat.h>

struct devsw devsw[NDEV];
struct file_info kernel_files[NFILE]; 


int fileopen(char *path, int mode) {
    struct proc* current_process = myproc();
    // For lab1, if path does not exist or the mode is not right, return -1
    struct inode* file_inode = namei(path); 
    struct stat istat;
    concurrent_stati(file_inode,&istat);
    if (file_inode == 0 ) {
        return -1;
    }else {
        locki(file_inode); 
    }

    if (mode != O_RDONLY &&  file_inode->type != T_DEV && mode != O_RDWR) { 
        
        unlocki(file_inode);
        return -1; 
    }

//     if ( mode != O_WRONLY && mode != O_RDWR && mode != O_CREATE && 
//       (mode != (O_CREATE | O_RDWR))) {
//     return -1;
//   }


    // First, find a place in process to put the file
    int pid;
    for (pid = 0; pid < NOFILE; pid++) {    // reverse pid = 0 for stdin
        if (current_process->kernel_file_ptr[pid] == NULL) {
            break; 
        }
    }
    if (pid == NOFILE) {    // Cannot find a place in process to store the file_ptr
        unlocki(file_inode);
        return -1;
    }
    // Second, find a place in kernel files, if found, open it
    int kid;
    for (kid = 0; kid < NFILE; kid ++) {
        if (kernel_files[kid].memory_count == 0) { 
            kernel_files[kid].memory_count += 1; 
            kernel_files[kid].inode = file_inode; 
            kernel_files[kid].access_type = mode; 
            kernel_files[kid].path = path; 
            break;  
        }
    }

    unlocki(file_inode);
    if (kid == NFILE) {
        return -1;
    }
    else {
        // Before return, make the process pointer point to the right file
        current_process->kernel_file_ptr[pid] = &kernel_files[kid];  
        current_process->kernel_file_ptr[pid]->curr_offset = 0;
        current_process->kernel_file_ptr[pid]->inPipe = false;
        return pid; 
    }


}



int fileread(int fd, char *buffer, int read_num) {

    struct proc* current_process = myproc();
    if(current_process->kernel_file_ptr[fd] == NULL || read_num < 0 || fd < 0 || fd >= NOFILE) return -1; 

    struct file_info *FILE =  current_process->kernel_file_ptr[fd];

    if(!FILE->inPipe){
        //cprintf(" I came here1\n");


        // int res = 0 ;
        // if (FILE->inode == NULL || FILE->access_type == O_WRONLY ) return -1;
        // int offset = concurrent_readi(FILE->inode, buffer, FILE->curr_offset, read_num);
        // //acquire(&FILE->lock);
        // current_process->kernel_file_ptr[fd]->curr_offset += offset ; 
        // //release(&FILE->lock);
        // return offset ;

        struct sleeplock *lock = &(FILE->inode->lock);
        int res = -1;
        acquiresleep(lock);

        res = readi(FILE->inode, buffer, kernel_files[fd].curr_offset, read_num);

        if(res >= 0){

            kernel_files[fd].curr_offset += res;
        }

        releasesleep(lock);

        return res;


    }else{
        //cprintf(" I came here2\n");
        // if(FILE->pipe_ptr.read_fd!=fd){ 
        //     cprintf("fd = %d\n",fd);
        //     cprintf("read_fd = %d\n",FILE->pipe_ptr.read_fd);
        //     cprintf("write_fd = %d\n",FILE->pipe_ptr.write_fd);
        // cprintf("I reach here\n");
        // return -1;
        // }
        acquire(&FILE->pipe_ptr.lock);
        
        int size = sizeof(FILE->pipe_ptr.buffer);
        int res = 0;

        while(FILE->pipe_ptr.head == FILE->pipe_ptr.tail){
            if(myproc()->killed !=0 ) {
	            release(&FILE->pipe_ptr.lock);
                
	            return -1;
            }

            if(FILE->memory_count == 1){
	        release(&FILE->pipe_ptr.lock);
	        return 0;
            }

            wakeup(&FILE->pipe_ptr.write_fd);
            sleep(&FILE->pipe_ptr.read_fd,&FILE->pipe_ptr.lock);

        }

        for(int i=0; i<read_num; i++){
            if(myproc()->killed !=0) return -1;
            if(FILE->pipe_ptr.head == FILE->pipe_ptr.tail){
	            break;
            }
            buffer[i] = FILE->pipe_ptr.buffer[FILE->pipe_ptr.head%size];
            FILE->pipe_ptr.head++;
            res++;
        }

     wakeup(&FILE->pipe_ptr.write_fd);
     release(&FILE->pipe_ptr.lock);
     
     return res;

    }

}

int filewrite(int fd, char *buffer, int write_num) {
    struct proc* current_process = myproc();
    if(current_process->kernel_file_ptr[fd] == NULL || fd < 0 || fd >= NOFILE) return -1;
    struct file_info* FILE = current_process->kernel_file_ptr[fd];
    
    if(!FILE -> inPipe){  
        // if this file is not in pipe, just regularly write.
        //cprintf("I reach here\n");
        int res = 0;
        if( FILE -> inode == NULL || FILE ->access_type == O_RDONLY ) return -1;
        struct sleeplock *lock = &(FILE->inode->lock);
        acquiresleep(lock);

            res = writei(FILE->inode, buffer, FILE->curr_offset, write_num);
            if (res >= 0) {
                FILE->curr_offset += res;
                FILE->inode->size += res;
                //updatei(FILE->inode);
            }


        releasesleep(lock);
        return res;
        //return concurrent_writei( FILE->inode, buffer , FILE->curr_offset, write_num);
    }else{
        // if this file is in pipe.
        acquire(&FILE->pipe_ptr.lock);
        int res = 0;
        int size = sizeof(FILE->pipe_ptr.buffer);
        for(int i = 0; i < write_num; i++){

            while((FILE->pipe_ptr.tail - FILE->pipe_ptr.head)==size){
	        wakeup(&FILE->pipe_ptr.read_fd);
	        sleep(&FILE->pipe_ptr.write_fd, &FILE->pipe_ptr.lock);
            
            } 
        
            if(FILE -> memory_count == 1){
	        release(&FILE->pipe_ptr.lock);
	        return -1;
            }

            FILE->pipe_ptr.buffer[FILE->pipe_ptr.tail%size] = buffer[i];
            FILE->pipe_ptr.tail++;
            res++;
        }


        wakeup(&FILE->pipe_ptr.read_fd);
        release(&FILE->pipe_ptr.lock);
        return res;


    }
}

int fileclose(int fd) {
    struct proc* current_process = myproc(); 
    struct file_info *file; 
    if (fd < 0 || fd >= NOFILE || current_process->kernel_file_ptr[fd] == NULL) { 
        // this porcess does not use this pd or the file is write only
        return -1; 
    } 
    else {

        file = current_process->kernel_file_ptr[fd];

    }
    file->memory_count -= 1;
    if (file->memory_count == 0) {
        file->path = 0; 
        irelease(file->inode); 
        file->inode = 0;
        file->curr_offset = 0;
        file->access_type = 0;
    }
    current_process->kernel_file_ptr[fd] = NULL;
    return 0; 


}

int filedup(int fd) {
    struct proc* current_process = myproc(); 
    if (fd < 0 || fd >= NOFILE || current_process->kernel_file_ptr[fd] == NULL) {
        return -1; 
    }
    // else {
    //     struct file_info  *file = current_process->kernel_file_ptr[fd]; 
    // }

    // Find a new place to point to the same file_info
    int pid;
    for (pid = 0; pid < NOFILE; pid++) {
        if (current_process->kernel_file_ptr[pid] == NULL) {
            current_process->kernel_file_ptr[pid] = current_process->kernel_file_ptr[fd]; 
            current_process->kernel_file_ptr[pid]->memory_count += 1; 
            return pid;
        }
    }
    return -1;
}

int filestat(int fd, struct stat *stats) {
    struct proc* current_process = myproc(); 
    struct file_info *file; 
    if (fd < 0 || fd >= NOFILE || current_process->kernel_file_ptr[fd] == NULL) { // this porcess does not use this pd or the file is write only
        return -1; 
    } 
    else {
        file = current_process->kernel_file_ptr[fd];
    }
    concurrent_stati(file->inode, stats); 
    return 0;
}

int pipe(int *fds){
    // current process
    struct proc *current_process = myproc();
    
    // create a pipe
    struct pipe *current_pip = NULL;

    // find a open space in the kernal file table and use it as a pipe
    int kid = 0;
    for(kid = 0 ; kid < NFILE ; kid++){
        if(kernel_files[kid].memory_count == 0){
            // assign pip in file information as true
            kernel_files[kid].inPipe = true;
            // assign as Read and write system
            kernel_files[kid].access_type = O_RDWR;
            current_pip = &kernel_files[kid].pipe_ptr;
            break;
        }
    }
    
    if(kid == NFILE) return -1;

    int id_pipe = 0;
    for(int pid = 0 ; pid < NOFILE ; pid++){
        if(current_process->kernel_file_ptr[pid] == NULL && id_pipe <2){
            fds[id_pipe] = pid;
            current_process->kernel_file_ptr[pid] = &kernel_files[kid];
            kernel_files[kid].memory_count += 1;
            id_pipe += 1; 
        }

    }

    // This process does not have enough empty file des for two end of pipe
    if(id_pipe < 2){
        kernel_files[kid].memory_count = 0;
        kfree((char*) current_pip);
        return -1;
    }

    current_pip->read_fd = fds[0];
    current_pip->write_fd = fds[1];
    current_pip->head = 0;
    current_pip->tail = 0;
    
    return 0;

}
