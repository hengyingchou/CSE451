# Lab 3: Address Space Management

## Overview
For this lab, we focus on how to let our operating system be more flexible by the followings:

 - Adding shell support for running various programs after booting os. 
 - Dynamically grow stack and heap memory for allowing users to run different programs and request more resources. 
 - Modifying fork() so that it only copies the necessary memory to increase performance. 

Although the lab listed sbrk before shell, we believe it is more reasonable to implement shell first since it is more straightforward and it is more like a "set-up" process rather than coding compared to other exercises. 

## In-depth Analysis and Implementation

### Shell: 
Shell is a process that receives user input to run a program and perform the output. Before this lab, our operating system is like a script which we need to detrmine which process to run in the source code before booting OS. After changing the entry point of our OS to a shell, we can run any process after booting OS. 

The implementation is in the followings: 

- Change the code in `kernel/initcode.S` from "/lab3init\0" to "/init\0", this tells the operating system to run `init.c` rather than `lab3init.c` after booting. `init.c` is already implemented which will fork() a shell process (also implemented) and allows us to run different programs from there. 
- Test our changes using cat, echo, grep, ls, wc along with | (i.e. pipe) to see if our implementation is correct. 

### sbrk:
When a process is running and needs to put some variables in the memory, it allocates some spaces in the heap memory. Since memory are given to the process by OS and heap memory is not infinite, we want to have a system call to allocate more heap memory to a process. sbrk is the system call that increases the heap size of the process and determines if the process needs more memory from the OS. 

The implementation is in the followings: 

- Takes in one argument `n`, ask to increase heap by `n` bytes.  
- Check of the currenet number of pages can satisfied the need of increase n bytes.
- If the previous check fails (i.e. we need more physical memory), ask OS to allocate more pages by calling `kalloc`. If `kalloc` returns 0, means that we cannot find a unused page to fufill this request.
- Map the new pages to process address space using `vregionaddmap`.
- Return the old bytes of heap if success, returns -1 if failed. Note that if failed, we need to restore to the state before calling `sbrk`.

### Grow user stack:
Just like the heap, in some situations, we might want to increase the stack memory. However, not like the heap, where the programmer explicitly calls `malloc` or `new` to allocate new memory, stack growth is implicitly requested from procedure calls. Thus, stack growth is done by analyzing hardware exceptions. We know that when we are accessing the stack, the compiler will generate a virtual address, either using it to find a local variable or setting up the end of the stack frame. After generating virtual address, it will be mapped to physical memory. At this point, if we cannot do this mapping or the access type is incorrect, a hardware page fault will arise. Hereby, we can detect this page fault and determine if that comes from a stack growth request. 

Hardware page fault is captured in `trap.c` in the `default` switch statement. We can firstly see if it comes from the user since only the user asks for the stack growth. Then, we see what address is it asking. From the lab spec, we know that we need at most 10 pages for the stack. If an address is more than 10 pages distance from the stack base, we can know that this is not a stack growth and want to do other things with it. Otherwise, give a new page to stack and map to the correct virtual space. 

The implementation is in the followings: 

- Fix `trap.c::trap()::switch::default`. 
- If a hardware fault arises, check if the last three bits in `tf->err` to determine if it is a page fault
- If so, check if the address that the program trying to access is bigger than stack base + 10. If so, it is a normal page fault and we can do nothing on it.
- If all is fine, allocate a new page to stack using similar logic to `sbrk`
- After doing all steps above, return from trap(). The control of CPU will be hand back to the user program and all register values will be restored. Thus, the user program can continue excecuting. 

### Copy-on-write fork:
When we do a fork(), all memory pages are copied from parent to child. Since a fork() is followed by exec() by many times, copying a memory that will soon be overwrite is not necessary. Thus, we what to implement a copy-on-write fork which the child only copy the necessary memory if it is modified. To implement this, we let both the child and the parent process points to the same phycial pages and set both of its vpages to be read-only. If any process want to write into a read-only page, `trap()` will be invoke. We can capture this error and see if we want to copy the ppage and hand it to the child process. 

There will be issues to implement the copy-on-write fork this way: 
1. vpage might already be a read-only page before fork(). We want to distinguish this so we do not accidentally unlock a read-only page after coping it. 
2. Both parent and child process might want to write to a vpage that points to the same physical page. If not handle properly, it might create 3 ppage for 2 vpages. Thus, we want to have a lock to prevent this. 
3. Child might call fork(). In this case, a physical page might have more then two references and we need a counter variable to determine when to release the copy-on-write read-only page. 

The implementation is in the followings: 

- Fix `fork()` and `trap.c::trap()::switch::default`.
- Instead of giving the child process as many new phyiscal pages as the parent, its vpages will points to the same physical pages as it parent. 
- Change vpages in both processes to be read-only.
- If any processes wants to write in to the copy-on-write read-only page, we assign a new pthysical page to it and copy the content of the original physical page. 
- For a physical page, if only one vpage is referencing to it, that vpage should be released from copy-on-write read-only. 

## Risk Analysis
1. Why we want the operating system to design which page for a stack or heap. Can we just hand the process a bounch of ppages and that the process decide how to use it?
2. For growing user stack, what should the last 3 bit of tf->err be? I can see that b2 should be 1 since the call is made from user, but b1 could be either 1 or 0 since both write and read could occur as stated in the lab doc `Whenever a user application issues an instruction that
reads or writes to the user stack (e.g., creating a stack frame, accessing localvariables)`. Also, should b0 be 1 since it is a page protected issue? 
