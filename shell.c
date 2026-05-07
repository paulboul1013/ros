#include "user.h"

int syscall(int sysno, int arg0,int arg1,int arg2){
    register int a0 __asm__("a0")=arg0;
    register int a1 __asm__("a1")=arg1;
    register int a2 __asm__("a2")=arg2;
    register int a3 __asm__("a3")=sysno;

    __asm__ __volatile__("ecall"
                        : "=r"(a0)
                        : "r"(a0), "r"(a1), "r"(a2), "r"(a3)
                        : "memory");

    return a0;
}

void main(void) {
    while(1){
prompt:
        printf("> ");
        char cmdline[128];
        for(int i=0;;i++) {
            char ch=getchar();
            putchar(ch);
            if (i==sizeof(cmdline)-1){ 
                printf("command line too long\n");
                goto prompt;
            }
            else if (ch=='\r') { //newline character is \r in the debug qemu terminal
                printf("\n");
                cmdline[i]=0;
                break;
            }else{
                cmdline[i]=ch;
            }
        }            

        if (strcmp(cmdline,"hello")==0){
            printf("Hello world from shell!\n");
        }
        else if (strcmp(cmdline,"exit")==0){
            exit();
        }
        else{
            printf("Unkown command: %s\n",cmdline);
        }
    }
}