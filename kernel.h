#pragma once

#define PROCS_MAX 8 //maximum number of processes

#define PROC_UNUSED  0 //unused process control structure
#define PROC_RUNNABLE 1 //runable process

#define PANIC(fmt,...) \
    do { \
        printf("PANIC: %s:%d: " fmt "\n" ,__FILE__,__LINE__, ##__VA_ARGS__); \
        while(1){} \
    } while(0)


struct process {
    int pid; //process ID
    int state; //process state: PROC_UNUSED or PROC_RUNNABLE
    vaddr_t sp; //stack pointer
    uint8_t stack[8192]; //kernel stack
};

__attribute__((naked)) void switch_context(uint32_t *prev_sp,
                                            uint32_t *next_sp) {

    __asm__ __volatile__(
        //save callee-saved registers onto the current process's stack
        "addi sp, sp, -13 *4\n" //Allocate stack space for 13 4-byte registers
        "sw ra, 0 * 4(sp)\n" //save callee-saved registers only
        "sw s0, 1 * 4(sp)\n"
        "sw s1, 2 * 4(sp)\n"
        "sw s2, 3 * 4(sp)\n"
        "sw s3, 4 * 4(sp)\n"
        "sw s4, 5 * 4(sp)\n"
        "sw s5, 6 * 4(sp)\n"
        "sw s6, 7 * 4(sp)\n"
        "sw s7, 8 * 4(sp)\n"
        "sw s8, 9 * 4(sp)\n"
        "sw s9, 10 * 4(sp)\n"
        "sw s10, 11 * 4(sp)\n"
        "sw s11, 12 * 4(sp)\n"

        //Switch the stack pointer
        "sw sp, (a0)\n" //*prev_sp=sp;
        "lw sp, (a1)\n" //switch stack pointer (sp) here

        //Restore callee-saved registers from the next process's stack
        "lw ra,  0  * 4(sp)\n"  // Restore callee-saved registers only
        "lw s0,  1  * 4(sp)\n"
        "lw s1,  2  * 4(sp)\n"
        "lw s2,  3  * 4(sp)\n"
        "lw s3,  4  * 4(sp)\n"
        "lw s4,  5  * 4(sp)\n"
        "lw s5,  6  * 4(sp)\n"
        "lw s6,  7  * 4(sp)\n"
        "lw s7,  8  * 4(sp)\n"
        "lw s8,  9  * 4(sp)\n"
        "lw s9,  10 * 4(sp)\n"
        "lw s10, 11 * 4(sp)\n"
        "lw s11, 12 * 4(sp)\n"
        "addi sp, sp , 13 * 4\n" // recover stack pointer
        "ret\n"  
    );
}

struct process procs[PROCS_MAX]; //all process control structure

struct process *create_process(uint32_t pc){
    //find an unused process control structure
    struct process *proc=NULL;
    int i;
    for(i=0; i< PROCS_MAX ;i++){
        if (procs[i].state==PROC_UNUSED){
            proc =&procs[i];
            break;
        }
    }

    if (!proc){
        PANIC("no free process slots");
    }

    //stack callee-saved registers
    //There registers values will be restored in the first context switch in swithc_context
    uint32_t *sp=(uint32_t*)&proc->stack[sizeof(proc->stack)];
    *--sp=0; //s11
    *--sp=0; //s10
    *--sp=0; //s9
    *--sp=0; //s8
    *--sp=0; //s7
    *--sp=0; //s6
    *--sp=0; //s5
    *--sp=0; //s4
    *--sp=0; //s3
    *--sp=0; //s2
    *--sp=0; //s1
    *--sp=0; //s0
    *--sp=(uint32_t)pc; //ra

    //init
    proc->pid=i+1;
    proc->state=PROC_RUNNABLE;
    proc->sp=(uint32_t)sp;
    return proc;

}


struct sbiret {
    long error;
    long value;
};

struct trap_frame {
    uint32_t ra;
    uint32_t gp;
    uint32_t tp;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t t3;
    uint32_t t4;
    uint32_t t5;
    uint32_t t6;
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t a4;
    uint32_t a5;
    uint32_t a6;
    uint32_t a7;
    uint32_t s0;
    uint32_t s1;
    uint32_t s2;
    uint32_t s3;
    uint32_t s4;
    uint32_t s5;
    uint32_t s6;
    uint32_t s7;
    uint32_t s8;
    uint32_t s9;
    uint32_t s10;
    uint32_t s11;
    uint32_t sp;
}__attribute__((packed));

#define READ_CSR(reg) \
    ({ \
        unsigned long __tmp; \
        __asm__ __volatile__("csrr %0, " #reg : "=r"(__tmp)); \
        __tmp; \
    })

#define WRITE_CSR(reg,value) \
    do{ \
        uint32_t __tmp=(value); \
        __asm__ __volatile__("csrw " #reg ", %0" ::"r"(__tmp)); \
    }while(0)