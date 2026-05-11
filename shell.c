#include "user.h"

#define CMDLINE_MAX 128
#define ASCII_BS '\b'
#define ASCII_DEL 0x7f

static int is_backspace(char ch) {
    return ch == ASCII_BS || ch == ASCII_DEL;
}

static void erase_last_char_on_screen(void) {
    putchar('\b');
    putchar(' ');
    putchar('\b');
}

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
        char cmdline[CMDLINE_MAX];
        int len=0;
        
        for(;;) {
            char ch=getchar();
            

            if (ch=='\r') { //newline character is \r in the debug qemu terminal
                printf("\n");
                cmdline[len]=0;
                break;
            }

            if (is_backspace(ch)) {
                if (len>0) {
                    len--;
                    erase_last_char_on_screen();
                }
                continue;
            }

            if (len==CMDLINE_MAX-1){ 
                printf("command line too long\n");
                goto prompt;
            }

            cmdline[len]=ch;
            len++;
            putchar(ch);
        }            

        if (strcmp(cmdline,"hello")==0){
            printf("Hello world from shell!\n");
        }
        else if (strcmp(cmdline,"exit")==0){
            exit();
        }
        else if(strcmp(cmdline,"readfile")==0) {
            char buf[128];
            int len=readfile("hello.txt",buf,sizeof(buf));
            buf[len]='\0';
            printf("%s\n",buf);
        }
        else if (strcmp(cmdline,"writefile")==0){
            writefile("hello.txt","Hello from shell!\n",19);
        }
        else{
            printf("Unkown command: %s\n",cmdline);
        }
    }
}