/********************
EFI 부트로더                       
1. 그래픽 모드 설정                 
2. ACPI 테이블 및 시스템 맵 불러오기
5. 제어권 커널로더에게 넘겨주기

export EDK_TOOLS_PATH="$PWD/edk2/BaseTools";
export PACKAGES_PATH="$PWD/edk2:$PWD/edk2-platforms:$PWD/edk2-non-osi";
. ./edk2/edksetup.sh;
*******************/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/FileInfo.h>

EFI_STATUS EFIAPI efi_main(IN EFI_HANDLE imgHandle, IN EFI_SYSTEM_TABLE *sysTable) {
    EFI_SYSTEM_TABLE *gST = sysTable;
    EFI_BOOT_SERVICES *gBS = sysTable->BootServices;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *simpleFileSystem;
    EFI_GUID sfsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_GUID fileInfoGuid = EFI_FILE_INFO_ID;
    EFI_FILE_PROTOCOL *sysDiskRoot;
    EFI_FILE_PROTOCOL *osLoader;
    EFI_STATUS status;
    UINTN osLoaderInfoSize = 0;
    EFI_FILE_INFO *osLoaderInfo;
    UINT8 *osLoaderBase = 0;
    EFI_HANDLE osLoaderImageHandle;

    gST->ConOut->ClearScreen(gST->ConOut);
    gST->ConOut->SetAttribute(gST->ConOut, 0x0E);
    Print(L"Start Initializing\n");
    gST->ConOut->SetAttribute(gST->ConOut, 0x0F);

    // 메모리에 OS LOADER 적재
    // 하드디스크의 EFI 시스템 디스크에 부트로더가 설치되어 있다고 가정
    // TODO: 시스템 디스크 스캔 후 부팅 가능한 efi 파일 모두 불러오고 선택지 생성하기.
    // OS Loader Handle 생성
    status = gBS->LocateProtocol(&sfsGuid, NULL, (void **) &simpleFileSystem); 
    if (EFI_ERROR (status)) { return status; }
    status = simpleFileSystem->OpenVolume(simpleFileSystem, &sysDiskRoot); 
    if (EFI_ERROR (status)) { return status; }
    status = sysDiskRoot->Open(sysDiskRoot, &osLoader, L"EFI\\OS_DEV\\osloader.efi", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR (status)) { return status; }
    Print(L"[1] Read OS Loader  . . . . . . . . OK\n");

    // OS Loader 크기 알아내기
    osLoader->GetInfo(osLoader, &fileInfoGuid, &osLoaderInfoSize, osLoaderInfo);
    status = gBS->AllocatePool(EfiLoaderData, osLoaderInfoSize, (void**) &osLoaderInfo);
    if (EFI_ERROR (status)) { return status; }
    status = osLoader->GetInfo(osLoader, &fileInfoGuid, &osLoaderInfoSize, osLoaderInfo);
    if (EFI_ERROR (status)) { return status; }
    Print(L"- osLoaderSize: %dB OK\n", osLoaderInfo->FileSize);

    // OS Loader 읽어오기
    status = gBS->AllocatePool(EfiLoaderData, osLoaderInfo->FileSize, (void**) &osLoaderBase);
    if (EFI_ERROR (status)) { return status; }
    status = osLoader->SetPosition(osLoader, 0);
    if (EFI_ERROR (status)) { return status; }
    status = osLoader->Read(osLoader, &(osLoaderInfo->FileSize), osLoaderBase);
    if (EFI_ERROR (status)) { return status; }
    Print(L"- osLoaderBaseAddress: 0x%x OK\n", osLoaderBase);
    status = gBS->LoadImage(FALSE, imgHandle, NULL, osLoaderBase, osLoaderInfo->FileSize, &osLoaderImageHandle);
    if (EFI_ERROR (status)) { return status; }
    Print(L"[2] Load OS Loader  . . . . . . . . OK\n");

    status = gBS->StartImage(osLoaderImageHandle, NULL, NULL);
    if (EFI_ERROR (status)) {
        gST->ConOut->SetAttribute(gST->ConOut, 0x0E);
        Print(L"Kernel Loader Failed . . . \n");
        Print(L"Error: %r\n", status);
        while (1);
    }

    return EFI_SUCCESS;
}
