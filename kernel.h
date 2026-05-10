#pragma once

#define PROCS_MAX 8 //maximum number of processes

#define PROC_UNUSED  0 //unused process control structure
#define PROC_RUNNABLE 1 //runable process

#define SATP_SV32 (1u<<31) //enable Sv32 page mode
#define PAGE_V (1<<0) //valid bit (entry is enabled)
#define PAGE_R (1<<1) //readable
#define PAGE_W (1<<2) //writeable
#define PAGE_X (1<<3) //executeable
#define PAGE_U (1<<4) //user (accessible in user mode)
# define PAGE_SIZE 4096

//base virtual address of an application image
//needs to match the starting address defined in user.ld
#define USER_BASE 0x1000000

#define SSTATUS_SPIE (1<<5) //into user-mode allow hw interrupt

#define SCAUSE_ECALL 8 //syscall
#define PROC_EXITED 2

#define SECTOR_SIZE       512
#define VIRTQ_ENTRY_NUM   16
#define VIRTIO_DEVICE_BLK 2
#define VIRTIO_BLK_PADDR  0x10001000
#define VIRTIO_REG_MAGIC         0x00
#define VIRTIO_REG_VERSION       0x04
#define VIRTIO_REG_DEVICE_ID     0x08
#define VIRTIO_REG_PAGE_SIZE     0x28
#define VIRTIO_REG_QUEUE_SEL     0x30
#define VIRTIO_REG_QUEUE_NUM_MAX 0x34
#define VIRTIO_REG_QUEUE_NUM     0x38
#define VIRTIO_REG_QUEUE_PFN     0x40
#define VIRTIO_REG_QUEUE_READY   0x44
#define VIRTIO_REG_QUEUE_NOTIFY  0x50
#define VIRTIO_REG_DEVICE_STATUS 0x70
#define VIRTIO_REG_DEVICE_CONFIG 0x100
#define VIRTIO_STATUS_ACK       1
#define VIRTIO_STATUS_DRIVER    2
#define VIRTIO_STATUS_DRIVER_OK 4
#define VIRTQ_DESC_F_NEXT          1
#define VIRTQ_DESC_F_WRITE         2
#define VIRTQ_AVAIL_F_NO_INTERRUPT 1
#define VIRTIO_BLK_T_IN  0
#define VIRTIO_BLK_T_OUT 1

//virtqueue descriptor table entry
struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

//virtqueue available ring
struct virtq_avail {
    uint16_t flags;
    uint16_t index;
    uint16_t ring[VIRTQ_ENTRY_NUM];
} __attribute__((packed));

//virtqueue used ring entry
struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

//virtqueue used ring
struct virtq_used {
    uint16_t flags;
    uint16_t index;
    struct virtq_used_elem ring[VIRTQ_ENTRY_NUM];
} __attribute__((packed));

//virtqueue
struct virtio_virtq {
    struct virtq_desc descs[VIRTQ_ENTRY_NUM];
    struct virtq_avail avail;
    struct virtq_used used __attribute__((aligned(PAGE_SIZE)));
    int queue_index;
    volatile uint16_t *used_index;
    uint16_t last_used_index;
} __attribute__((packed));

// virtio-blk request
struct virtio_blk_req {
    //Fist descriptor : read-only from the device
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;

    //second descriptor: writable by the device if it's a read operation (VIRTQ_DESC_F_WRITE)
    uint8_t data[512];
    
    //Third descriptor: writable by the device (VIRTQ_DESC_F_WRITE)
    uint8_t status;
} __attribute__((packed));


#define PANIC(fmt,...) \
    do { \
        printf("PANIC: %s:%d: " fmt "\n" ,__FILE__,__LINE__, ##__VA_ARGS__); \
        while(1){} \
    } while(0)

extern char __free_ram[], __free_ram_end[];

paddr_t alloc_pages(uint32_t n){
    static paddr_t next_paddr=(paddr_t)__free_ram;
    paddr_t paddr=next_paddr;
    next_paddr+=n*PAGE_SIZE;

    if (next_paddr > (paddr_t)__free_ram_end){
        PANIC("out of memory");
    }

    memset((void*)paddr,0,n*PAGE_SIZE);
    return paddr;
}

void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags){
    if (!is_aligned(vaddr,PAGE_SIZE)) {
        PANIC("unaligned vaddr %x",vaddr);
    }

    if (!is_aligned(paddr,PAGE_SIZE)){
        PANIC("unaligned paddr %x",paddr);
    }

    uint32_t vpn1=(vaddr>>22) & 0x3ff;
    if ((table1[vpn1] & PAGE_V)==0) {
        //create the 2nd level page table if it doesn't exist
        uint32_t pt_paddr=alloc_pages(1);
        table1[vpn1]=((pt_paddr/PAGE_SIZE)<<10) | PAGE_V;
        //pt_paddr/PAGE_SIZE is phyiscal address convert into the physical page number
        // pt_paddr = 0x80203000
        // PAGE_SIZE = 0x1000
        // pt_paddr / PAGE_SIZE = 0x80203
        /*
            31                         10 9          0
            +----------------------------+------------+
            |            PPN             |   flags    |
            +----------------------------+------------+
        */
    }

    //set the 2nd level page table entry to map the physical page
    uint32_t vpn0=(vaddr>>12) & 0x3ff;
    /*
        31                22 21                12 11          0
        +-------------------+-------------------+-------------+
        |       vpn1        |       vpn0        | page offset |
        +-------------------+-------------------+-------------+
    */

    //get the PPN from first level entry: table1[vpn1]>>10
    // convert into physical address: (table1[vpn1]>>10)*PAGE_SIZE
    uint32_t *table0=(uint32_t*) ((table1[vpn1]>>10)*PAGE_SIZE);

    //setting 2nd entry,complete mapping
    //put the PPN of physical page into the 2nd level page table entry
    //and add flags
    table0[vpn0]=((paddr/PAGE_SIZE)<<10) | flags | PAGE_V;

}

struct process {
    int pid; //process ID
    int state; //process state: PROC_UNUSED or PROC_RUNNABLE
    vaddr_t sp; //stack pointer
    uint32_t *page_table;
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

extern char __kernel_base[];



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