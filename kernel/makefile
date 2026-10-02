all: kernel

kernel: entry.c kernel.h
	x86_64-elf-gcc -c kernel.c -o kernel.o \
    -ffreestanding \
    -mno-red-zone \
    -mno-mmx \
    -mno-sse \
    -mno-sse2 \
    -mcmodel=kernel \
    -fno-pie \
    -fno-stack-protector \
    -O2 \
    -Wall -Wextra;
	x86_64-elf-ld -T linker.ld kernel.o -o kernel.elf;
