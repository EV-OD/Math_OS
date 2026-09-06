NASM = nasm
CC = gcc
LD = ld

ASM_DIR = asm
SRC_DIR = src
INC_DIR = include
INC_DIR2 = lib
BUILD_DIR = build

LIBGCC := $(shell gcc -m32 -print-libgcc-file-name)
CFLAGS = -m32 -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -c -I$(INC_DIR) -I$(INC_DIR2)
NASMFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld

C_SRCS = $(wildcard $(SRC_DIR)/*.c)
ASM_SRCS = $(wildcard $(ASM_DIR)/*.asm)

C_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))
ASM_OBJS = $(patsubst $(ASM_DIR)/%.asm,$(BUILD_DIR)/asm_%.o,$(ASM_SRCS))
OBJS = $(ASM_OBJS) $(C_OBJS)

KERNEL = mykernel.bin
ISO = myos.iso
LOG_DIR = logs
LOG_FILE = $(LOG_DIR)/os.log

all: $(ISO)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/asm_%.o: $(ASM_DIR)/%.asm | $(BUILD_DIR)
	$(NASM) $(NASMFLAGS) $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $(KERNEL) $(LIBGCC)

$(ISO): $(KERNEL)
	mkdir -p isodir/boot/grub
	cp $(KERNEL) isodir/boot/$(KERNEL)
	printf 'set timeout=0\nset default=0\nmenuentry "My Bare Metal OS" {\n  multiboot /boot/$(KERNEL)\n  boot\n}\n' > isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) isodir

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -device virtio-gpu-pci -chardev stdio,id=s0,signal=off,mux=on -serial chardev:s0

run-headless: $(ISO)
	mkdir -p $(LOG_DIR)
	: > $(LOG_FILE)
	qemu-system-i386 -cdrom $(ISO) -device virtio-gpu-pci -serial file:$(LOG_FILE) -display none

clean:
	rm -rf $(BUILD_DIR) $(KERNEL) $(ISO)
	rm -rf isodir
.PHONY: all run run-headless clean
