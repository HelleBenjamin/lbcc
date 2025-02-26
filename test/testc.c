
void printc(char c);

short main() {
    printc('a');
    __vmexit 100;
}

void printc(char c) {
    asm("out r3");
}
