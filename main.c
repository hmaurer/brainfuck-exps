#include <stdio.h>
#include <sys/mman.h>
#include <string.h>

void emit(int *offset, char *buffer, char *code, int n) {
    memcpy(buffer+*offset, code, n);
    *offset += n;
}

void write_long(int offset, char *buffer, long v) {
    char *p = (char*)&v;
    for (int i = 0; i < 8; i++) {
        buffer[offset+i] = p[i];
    }
}

void write_int(int offset, char *buffer, int v) {
    char *p = (char*)&v;
    for (int i = 0; i < 4; i++) {
        buffer[offset+i] = p[i];
    }
}

void block(int *offset, char *buffer) {
    int soffset = *offset;
    char c;
    while ((c = getchar()) != EOF) {
        switch (c) {
            case '>': {
                unsigned char code[3] = {
                    0x49, 0xFF, 0xC0 // inc r8
                };
                emit(offset, buffer, (char*)code, sizeof(code));
                break;
            }
            case '<': {
                unsigned char code[3] = {
                    0x49, 0xFF, 0xC8 // dec r8
                };
                emit(offset, buffer, (char*)code, sizeof(code));
                break;
            }
            case '+': {
                unsigned char code[10] = {
                    0x43, 0x8A, 0x04, 0x01, // mov al,BYTE PTR [r9+r8*1]
                    0xFE, 0xC0, // inc al
                    0x43, 0x88, 0x04, 0x01 // mov BYTE PTR [r9+r8*1], al
                };
                emit(offset, buffer, (char*)code, sizeof(code));
                break;
            }
            case '-': {
                unsigned char code[10] = {
                    0x43, 0x8A, 0x04, 0x01, // mov al,BYTE PTR [r9+r8*1]
                    0xFE, 0xC8, // inc al
                    0x43, 0x88, 0x04, 0x01 // mov BYTE PTR [r9+r8*1], al
                };
                emit(offset, buffer, (char*)code, sizeof(code));
                break;
            }
            case '.': {
                unsigned char code[27] = {
                     0x48, 0xC7, 0xC0, 0x04, 0x00, 0x00, 0x02, // mov rax,0x2000004
                     0x48, 0xC7, 0xC7, 0x01, 0x00, 0x00, 0x00, // mov rdi,0x1
                     0x4B, 0x8D, 0x34, 0x08, // lea rsi,[r8+r9*1]
                     0x48, 0xC7, 0xC2, 0x01, 0x00, 0x00, 0x00, // mov rdx,0x1
                     0x0F, 0x05 // syscall
                };
                emit(offset, buffer, (char*)code, sizeof(code));
                break;
            }
            case ',':
                break;
            case '[': {
                unsigned char code[11] = {
                    0x43, 0x80, 0x3C, 0x01, 0x00, // cmp [r9+r8], 0
                    0x0F, 0x84, // conditional near jump if equal
                    0x00, 0x00, 0x00, 0x00 // placeholder for delta, to be completed when closing loop
                };
                emit(offset, buffer, (char*)code, sizeof(code));
                // start a new block
                block(offset, buffer);
                break;
            }
            case ']': {
                unsigned char code[5] = {
                    0xE9, // unconditional near jump backward
                    0x00, 0x00, 0x00, 0x00 // placeholder for delta
                };
                // fill conditional jump delta
                write_int(1, (char*)code, soffset - (*offset+5) - 11);
                // emit unconditional jump
                emit(offset, buffer, (char*)code, sizeof(code));
                // fill unconditional jump delta
                write_int(soffset - 4, (char*)buffer, *offset - soffset);
                // return back to parent block
                return;
            }
        }
    }
}

unsigned char buffer[1024*10];
unsigned char mem[1024*10];

int main(void) {
    int offset = 0;

    unsigned char prologue[17] = {
        0x49, 0xC7, 0xC0, 0x00, 0x00, 0x00, 0x00, // mov r8, 0x0
        0x49, 0xB9, // mov r9, ...
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 // address of memory block
    };
    write_long(9, (char*)prologue, (long)&mem);
    emit(&offset, (char*)buffer, (char*)prologue, sizeof(prologue));

    block(&offset, (char*)buffer);

    unsigned char epilogue[1] = {
        0xc3 // ret
    };
    emit(&offset, (char*)buffer, (char*)epilogue, sizeof(epilogue));

    void *p = mmap(0, sizeof(buffer), PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0);
    memcpy(p, buffer, sizeof(buffer));
    void (*func)() = p;

    func();

    printf("\nMemory dump:\n");    
    for (int i = 0; i < 20; i++) {
        printf("%.2x ", mem[i]);
    }
    printf("\n");
}
