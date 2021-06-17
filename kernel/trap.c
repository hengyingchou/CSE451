#include <cdefs.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>
#include <param.h>
#include <proc.h>
#include <spinlock.h>
#include <trap.h>
#include <x86_64.h>

// Interrupt descriptor table (shared by all CPUs).
struct gate_desc idt[256];
extern void *vectors[]; // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

int num_page_faults = 0;
int growuserstack(void);



void tvinit(void) {
  int i;

  for (i = 0; i < 256; i++)
    set_gate_desc(&idt[i], 0, SEG_KCODE << 3, vectors[i], KERNEL_PL);
  set_gate_desc(&idt[TRAP_SYSCALL], 1, SEG_KCODE << 3, vectors[TRAP_SYSCALL],
                USER_PL);

  initlock(&tickslock, "time");
}

void idtinit(void) { lidt((void *)idt, sizeof(idt)); }

void trap(struct trap_frame *tf) {
  uint64_t addr;

  if (tf->trapno == TRAP_SYSCALL) {
    if (myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if (myproc()->killed)
      exit();
    return;
  }

  switch (tf->trapno) {
  case TRAP_IRQ0 + IRQ_TIMER:
    if (cpunum() == 0) {
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case TRAP_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case TRAP_IRQ0 + IRQ_IDE + 1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case TRAP_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case TRAP_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case TRAP_IRQ0 + 7:
  case TRAP_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n", cpunum(), tf->cs, tf->rip);
    lapiceoi();
    break;

  default:
    addr = rcr2();

      if (tf->trapno == TRAP_PF) {
      num_page_faults += 1;

      struct vregion *vr;
      struct vpage_info *vpi;
      if(( vr = va2vregion(&myproc()->vspace,addr))!=0 && (vpi = va2vpage_info(vr,addr)) != 0 ){

        struct core_map_entry* entry = (struct core_map_entry *)pa2page(vpi->ppn << PT_SHIFT);

        if(vpi->cow_page==true && entry->reference_count > 1 && vpi->writable == 0){ //references to unwritable page 
          //allocate a page
          char* data = kalloc();
	        if(!data) break;// kalloc fails 

	        memset(data, 0, PGSIZE);
          //copy the data from the copy-on-write page
          memmove(data, P2V(vpi->ppn << PT_SHIFT), PGSIZE);

          acquire_core_map_lock();
          entry -> reference_count--;   	// ref count decrement   
	        vpi->used = 1;                  //page is in use
          vpi->writable = VPI_WRITABLE;   //make vpi writable
	        vpi->present = VPI_PRESENT;     // in physical memory
          vpi->cow_page = false;          //make vpi non_cow_page
          //faulting process start writing to that freshly-allocated page
          vpi->ppn = PGNUM(V2P(data)); 

          release_core_map_lock();

	        vspaceinvalidate(&myproc()->vspace);
	        vspaceinstall(myproc());

          break;
        } else if(vpi->cow_page==true && entry->reference_count == 1 && vpi->writable==0){ //only reference to unwritable page 
	      

	        vpi->writable = VPI_WRITABLE;//make vpi writable
          vpi->cow_page = false;         //make vpi non_cow_page


	        vspaceinvalidate(&myproc()->vspace);
	        vspaceinstall(myproc());
	        break;

        }
      }









       if (addr < SZ_2G && addr >= SZ_2G - 10 * PGSIZE) {
        if (growuserstack() != -1) {
          break;
        }
      }



      if (myproc() == 0 || (tf->cs & 3) == 0) {
        // In kernel, it must be our mistake.
        cprintf("unexpected trap %d from cpu %d rip %lx (cr2=0x%x)\n",
                tf->trapno, cpunum(), tf->rip, addr);
        panic("trap");
      }
      }

    // Assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "rip 0x%lx addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno, tf->err, cpunum(),
            tf->rip, addr);
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if (myproc() && myproc()->state == RUNNING &&
      tf->trapno == TRAP_IRQ0 + IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();
}


int growuserstack(void) {
  // similar to sbrk, but growing stack not heap 
  struct proc *current_process = myproc();
  struct vregion *vrspace_stack = &current_process->vspace.regions[VR_USTACK];
  uint64_t old_virtual_bound = vrspace_stack->va_base - vrspace_stack -> size - PGSIZE;

  //if stack already at max 10 pages
  if(vrspace_stack->size >= 10 * PGSIZE)
    return -1;

  // try to add new page to user stack
  if (vregionaddmap(vrspace_stack, old_virtual_bound, PGSIZE, VPI_PRESENT, VPI_WRITABLE) < 0) return -1;
  vrspace_stack->size += PGSIZE;
  
  vspaceinvalidate(&current_process->vspace);
  return old_virtual_bound;
}

