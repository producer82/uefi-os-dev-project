all: boot_efi

boot_efi: bootx64.dsc bootx64.inf osloader.inf bootx64.c osloader.c
	build -a X64 -m ../boot_efi/bootx64.inf -p ../boot_efi/bootx64.dsc; cp ../Build/bootx64/RELEASE_GCC/X64/bootx64.efi ./; 
	build -a X64 -m ../boot_efi/osloader.inf -p ../boot_efi/bootx64.dsc; cp ../Build/bootx64/RELEASE_GCC/X64/osloader.efi ./;
