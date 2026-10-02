all: bootloader kernel Disk.img clean

bootloader: ./boot_efi/makefile
	cd ./boot_efi; $(MAKE);

kernel: ./kernel/makefile
	cd ./kernel; $(MAKE);

Disk.img: ./boot_efi/bootx64.efi ./kernel/kernel.elf
	dd if=/dev/zero of=disk.img bs=1k count=1440;
	mformat -i disk.img -f 1440 ::;
	mmd -i disk.img ::/EFI;
	mmd -i disk.img ::/EFI/OS_DEV;
	mmd -i disk.img ::/EFI/BOOT;
	mcopy -i disk.img ./boot_efi/bootx64.EFI ::/EFI/BOOT;
	mcopy -i disk.img ./boot_efi/osloader.EFI ::/EFI/OS_DEV;
	mcopy -i disk.img ./kernel/kernel.ELF ::/EFI/OS_DEV;

.PHONY: clean

clean:
	rm -rf ./boot_efi/bootx64.efi ./boot_efi/osloader.efi ./kernel/kernel.elf
