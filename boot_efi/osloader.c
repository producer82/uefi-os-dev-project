/*****************
 * OS Loader
 * 1. 시스템 사양 충족 확인
 * 2. 커널 불러오기
 * 3. 페이징 테이블 재설정
 * 4. 그래픽 모드 설정
 * 5. ACPI Table 정보 불러오기
 * 6. 커널로 점프
 ****************/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/FileInfo.h>
#include <Library/BaseLib.h>
#include "boot.h"

// 64bit ELF 헤더
typedef struct ELF_ElfHeader {
    UINT8 ident[16];
    UINT16 type;
    UINT16 machine;
    UINT32 version;
    UINT64 entry;
    UINT64 phoff;
    UINT64 shoff;
    UINT32 flags;
    UINT16 ehsize;
    UINT16 phentsize;
    UINT16 phnum;
    UINT16 shentsize;
    UINT16 shnum;
    UINT16 shstrndx;
}ELF_ElfHeader;

// 64bit ELF 프로그램 헤더
typedef struct ELF_ProgramHeader {
    UINT32 type;
    UINT32 flags;
    UINT64 offset;
    UINT64 vaddr;
    UINT64 paddr;
    UINT64 filesz;
    UINT64 memsz;
    UINT64 align;
}ELF_ProgramHeader;

EFI_STATUS status;

EFI_STATUS EFIAPI SetPageTable(IN EFI_FILE_INFO *kernelInfo, IN EFI_PHYSICAL_ADDRESS kernelBaseAddress) {
    // 커널시작주소 + 커널 크기부터 페이징테이블배치
    /****************************************
    / PML4 (1개) 
    / PDPT (하위로더영역 1 + 상위커널영역 1) +
    / PDT 테이블 (하위로더영역 4 + 상위커널영역 1) + 
    / 페이지 테이블 (하위로더영역 2048개(4096MB) + 상위커널영역 32개(64MB))
    / 페이지 엔트리 -> 2048 * 512개
    / 1개 + 2개 + 5개 + (2048개 + n개)
    / 상위 영역 빠그러진거 고쳐야함
    ****************************************/

    //PML4 테이블 생성
    PageTable *PML4 = (PageTable *)kernelBaseAddress;
    PageTable *PDPT = (PML4 + 512);
    PageTable *PDT = (PDPT + 512*2);
    PageTable *PT = (PDT + 512*5);

    Print(L"- PML4 Table: 0x%x\n", PML4);
    Print(L"- PDPT Table: 0x%x\n", PDPT);
    Print(L"- PDT Table: 0x%x\n", PDT);
    Print(L"- PT Table: 0x%x\n", PT);

    // 하위 로더 영역 PML4E
    PML4[0].attr = 0b00000011;
    PML4[0].avl1 = 0;
    PML4[0].address = (UINT64)(PDPT) >> 12;
    PML4[0].avl2 = 0;
    PML4[0].nx = 0;
    
    // 상위 커널 영역 PML4E
    PML4[383].attr = 0b00000011;
    PML4[383].avl1 = 0;
    PML4[383].address = (UINT64)(PDPT + 512) >> 12;
    PML4[383].avl2 = 0;
    PML4[383].nx = 0;

    // 하위 로더 영역 PDPT
    for (UINT8 i = 0; i < 4; i++) {
        PDPT[i].attr = 0b00000011;
        PDPT[i].avl1 = 0;
        PDPT[i].address = (UINT64)(PDT + 512*i) >> 12;
        PDPT[i].avl2 = 0;
        PDPT[i].nx = 0;
    }

    // 상위 커널 영역 PDPT
    (PDPT + 512)[0].attr = 0b00000011;
    (PDPT + 512)[0].avl1 = 0;
    (PDPT + 512)[0].address = (UINT64)(PDT + 512*4) >> 12;
    (PDPT + 512)[0].avl2 = 0;
    (PDPT + 512)[0].nx = 0;

    // 하위 로더 영역 PDT
    for (UINT8 i = 0; i < 4; i++) {
       for (UINT16 j = 0; j < 512; j++) {
            PDT[j + i*512].attr = 0b00000011;
            PDT[j + i*512].avl1 = 0;
            PDT[j + i*512].address = (UINT64)(PT + 512*j + 512*512*i) >> 12;
            PDT[j + i*512].avl2 = 0;
            PDT[j + i*512].nx = 0;
       }
    }

    // 상위 커널 영역 PDT
    for (UINT8 i = 0; i < 32; i++) {
        (PDT + 512*4)[0].attr = 0b00000011;
        (PDT + 512*4)[0].avl1 = 0;
        (PDT + 512*4)[0].address = (UINT64)((PT + 2048*512) + 512*i) >> 12;
        (PDT + 512*4)[0].avl2 = 0;
        (PDT + 512*4)[0].nx = 0;
    }

    // 하위 로더 영역 PT
    for (UINT16 i = 0; i < 2048; i++) {
        for (UINT16 j = 0; j < 512; j++) {
            PT[j + i*512].attr = 0b00000011;
            PT[j + i*512].avl1 = 0;
            PT[j + i*512].address = (UINT64)(0x1000 * (i*512 + j)) >> 12;
            PT[j + i*512].avl2 = 0;
            PT[j + i*512].nx = 0;
        }
    }

    // 상위 커널 영역 PT
    for (UINT16 i = 0; i < 32; i++) {
        for (UINT16 j = 0; j < 512; j++) {
            PT[(2048*512)+(j + i*512)].attr = 0b00000011;
            PT[(2048*512)+(j + i*512)].avl1 = 0;
            PT[(2048*512)+(j + i*512)].address = (kernelBaseAddress + (0x1000 * (i*512 + j))) >> 12;
            PT[(2048*512)+(j + i*512)].avl2 = 0;
            PT[(2048*512)+(j + i*512)].nx = 0;
        }
    }

    AsmWriteCr3((UINTN)PML4);

    return EFI_SUCCESS;
}   

// /EFI/OS_DEV/kernel.elf를 임시 버퍼로 읽어온 후 파일에 대한 정보를 저장함
EFI_STATUS EFIAPI GetKernelInfo(EFI_FILE_INFO *kernelInfo, EFI_FILE_PROTOCOL *kernel) {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *gSFS;
    EFI_FILE_PROTOCOL *sysDiskRoot;
    EFI_GUID sfsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_GUID fileInfoGuid = EFI_FILE_INFO_ID;
    UINTN kernelInfoSize = 0;

    // 디스크에서 커널 읽어오기
    status = gBS->LocateProtocol(&sfsGuid, NULL, (void **) &gSFS); 
    if (EFI_ERROR (status)) { return status; }
    status = gSFS->OpenVolume(gSFS, &sysDiskRoot); 
    if (EFI_ERROR (status)) { return status; }
    status = sysDiskRoot->Open(sysDiskRoot, &kernel, L"EFI\\OS_DEV\\kernel.elf", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR (status)) { return status; }
    Print(L"- Read Kernel from Disk . . . . . . . . OK\n");

    // 커널 크기를 Byte 단위로 변수에 저장
    kernel->GetInfo(kernel, &fileInfoGuid, &kernelInfoSize, kernelInfo);
    status = gBS->AllocatePool(EfiLoaderData, kernelInfoSize, (void**) &kernelInfo);
    if (EFI_ERROR (status)) { return status; }
    status = kernel->GetInfo(kernel, &fileInfoGuid, &kernelInfoSize, kernelInfo);
    if (EFI_ERROR (status)) { return status; }
    Print(L"- kernelSize: %dB OK\n", kernelInfo->FileSize);
    
    return EFI_SUCCESS
}

// Higher-Half 영역에 커널을 위치시킴
EFI_STATUS EFIAPI SetKernelToMemory(EFI_FILE_PROTOCOL *kernel, EFI_FILE_INFO *kernelInfo) {
    EFI_PHYSICAL_ADDRESS *kernelBuffer;
    ELF_ElfHeader *elfHeader;
    ELF_ProgramHeader *programHeader;
    UINTN size;

    // 버퍼 할당
    status = kernel->Read(kernel, kernelInfo->FileSize, (void *)kernelBuffer);
    if (EFI_ERROR (status)) { return status; }

    elfHeader = (ELF_ElfHeader *)kernelBuffer;

    // magic number 검사
    if (elfHeader->ident[0] == 0x7F 
        && elfHeader->ident[1] == "E"
        && elfHeader->ident[2] == "L" 
        && elfHeader->ident[3] == "F") {
            Print(L"- Kernel Validation . . . . . . . . OK\n");
    }
    else {
        Print(L"- Kernel Validation . . . . . . . . FAIL\n");
        while(1);
    }

    programHeader = (ELF_ProgramHeader *)((UINT8 *)elfHeader + elfHeader->phoff);

    for (UINT8 i = 0; i < elfHeader->phnum; i++) {
        if(programHeader->type == )
    }
    
}

// 그래픽 모드를 설정하고 그 정보를 graphicsInfo에 저장
EFI_STATUS EFIAPI SetGraphicsMode(GraphicsInfo *graphicsInfo) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *gopInfo;
    UINTN gopSizeOfInfo = 0;
    UINTN maxResolution = 0;
    UINT8 maxResolutionMode = 0;
    BOOLEAN isThereDirectFrameBuffer = FALSE;
    BOOLEAN isThereBasicResolution = FALSE;
    
    status = gBS->LocateProtocol(&gopGuid, NULL, (void **)&gop);
    if (EFI_ERROR (status)) { return status; }
    
    // FHD로 해상도 변경 시도
    for (UINT8 i = 0; i < gop->Mode->MaxMode; i++) {
        status = gop->QueryMode(gop, i, &gopSizeOfInfo, &gopInfo);
        if (EFI_ERROR (status)) { return status; }
    
        // DirectFrameBuffer를 지원하는 모드가 존재하는지 확인
        if (gopInfo->PixelFormat == PixelRedGreenBlueReserved8BitPerColor || gopInfo->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
            isThereDirectFrameBuffer = TRUE;
            // 1920x1080 해상도의 모드가 있는지 확인
            if (gopInfo->HorizontalResolution == 1920 && gopInfo->VerticalResolution == 1080) {
                isThereBasicResolution = TRUE;
                status = gop->SetMode(gop, i);
                if (EFI_ERROR (status)) { return status; }
                break; 
            }
        }
    }
    
    // DirectFrameBuffer를 지원하지 않는 경우 부팅 중단
    if (isThereDirectFrameBuffer == TRUE) {
        Print(L"- Check Available Graphics Mode . . . . . . . . %d OK\n", gop->Mode->MaxMode);
    }
    else {
        Print(L"- Check Available Graphics Mode . . . . . . . . FAIL\n");
        while(1);
    }
    
    // FHD 해상도가 존재하지 않는 경우 최대 해상도로 변경 시도
    if (isThereBasicResolution == FALSE) {
        for (UINT8 i = 0; i < gop->Mode->MaxMode; i++) {
            status = gop->QueryMode(gop, i, &gopSizeOfInfo, &gopInfo);
            if (EFI_ERROR (status)) { return status; }
            
            // DirectFrameBuffer를 지원하는 모드 중 최대 해상도를 가진 모드 판별
            if (gopInfo->PixelFormat == PixelRedGreenBlueReserved8BitPerColor || gopInfo->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
                if (maxResolution <= gopInfo->VerticalResolution * gopInfo->HorizontalResolution) {
                    maxResolution = gopInfo->VerticalResolution * gopInfo->HorizontalResolution;
                    maxResolutionMode = i;
                }
            }
        }
        gop->SetMode(gop, maxResolutionMode);
    }
    
    graphicsInfo->HoriziontalResolution = gop->Mode->Info->HorizontalResolution;
    graphicsInfo->VerticalResolution = gop->Mode->Info->VerticalResolution;
    graphicsInfo->FrameBufferBase = gop->Mode->FrameBufferBase;
    graphicsInfo->FrameBufferSize = gop->Mode->FrameBufferSize;
    graphicsInfo->PixelFormat = gop->Mode->Info->PixelFormat;
    graphicsInfo->PixelPerScanLine = gop->Mode->Info->PixelsPerScanLine;

    return EFI_SUCCESS;
}

// 메모리 크기를 MB 단위로 계산하여 UINTN으로 반환하는 함수
UINT64 EFIAPI GetMemorySize(IN EFI_MEMORY_DESCRIPTOR *memMapBase, IN UINTN memMapSize, IN UINTN memMapDescSize) {
    /* 
    아래 타입에 해당하는 메모리를 제외하고 모두 계산
    EfiMemoryMappedIO / EfiMemoryMappedIOPortSpace: 그래픽카드, 메인보드 칩셋 등 장치 제어용 주소 공간 (물리 RAM이 아님)
    EfiUnusableMemory: 메모리 테스트 중 고장/에러가 감지된 불량 RAM 영역
    EfiReservedMemoryType: 시스템 레벨에서 접근 금지된 예약 영역 
    */

    EFI_MEMORY_DESCRIPTOR *memCurrentDescriptor;
    UINTN memDescNum = memMapSize / memMapDescSize;
    UINTN memNumberOfPages = 0;
    
    // 실제로 존재하지 않는 메모리를 제외한 나머지를 합계
    for (UINTN i = 0; i < memDescNum; i++) {
        memCurrentDescriptor = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)memMapBase + (i * memMapDescSize));

        if (memCurrentDescriptor->Attribute == EfiUnusableMemory || 
        memCurrentDescriptor->Attribute == EfiMemoryMappedIO ||
        memCurrentDescriptor->Attribute == EfiMemoryMappedIOPortSpace ||
        memCurrentDescriptor->Attribute == EfiReservedMemoryType) {
            continue;
        }
        else {
            memNumberOfPages += memCurrentDescriptor->NumberOfPages;
        }
    }

    // MB 단위의 메모리 크기를 반환
    return (UINTN)((memNumberOfPages * 4) / 1024);
}

// 메모리 맵에 필요한 공간을 할당하고 메모리 맵을 불러옴
// 함수에 들어왔다가 나가는 과정에서 메모리 맵이 바뀔 가능성이 있으므로 ALWAYS_INLINE 사용
ALWAYS_INLINE EFI_STATUS EFIAPI GetMemoryInfo(IN MemoryInfo *memoryInfo) {
    // 메모리 맵 생성에 필요한 정보 얻기
    
    status = gBS->GetMemoryMap(&(memoryInfo->MemoryMapSize), memoryInfo->MemoryMapBase, &(memoryInfo->MemoryMapKey), &(memoryInfo->MemoryMapDescSize), &(memoryInfo->MemoryMapDescVer));
    if (status != EFI_BUFFER_TOO_SMALL) { Print(L"- Get Size of Memory Map . . . . . . . . FAIL\n"); return status; }
    memoryInfo->MemoryMapSize += memoryInfo->MemoryMapDescSize * 16; // 디스크립터 16개분의 여유 공간 확보

    // 매모리 맵에 필요한 버퍼 할당
    status = gBS->AllocatePool(EfiLoaderData, memoryInfo->MemoryMapSize, (void**) &(memoryInfo->MemoryMapBase));
    if (EFI_ERROR (status)) { Print(L"- Allocate Memory Map . . . . . . . . Fail\n"); return status; }

    // 메모리 맵 가져오기
    status = gBS->GetMemoryMap(&(memoryInfo->MemoryMapSize), memoryInfo->MemoryMapBase, &(memoryInfo->MemoryMapKey), &(memoryInfo->MemoryMapDescSize), &(memoryInfo->MemoryMapDescVer));
    if (EFI_ERROR (status)) { Print(L"- Get Memory Map . . . . . . . . Fail\n"); return status; }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI efi_main(IN EFI_HANDLE imgHandle, IN EFI_SYSTEM_TABLE *sysTable) {
    EFI_SYSTEM_TABLE *gST = sysTable;
    EFI_BOOT_SERVICES *gBS = sysTable->BootServices;
    
    // 커널 이미지 핸들용
    EFI_PHYSICAL_ADDRESS kernelBaseAddress;
    EFI_FILE_PROTOCOL *kernel;
    EFI_FILE_INFO *kernelInfo;
    // EFI_HANDLE kernelImageHandler;
    
    // 커널 전달용
    MemoryInfo memoryInfo;
    GraphicsInfo graphicsInfo;
    //SystemInfo systemInfo;

    Print(L"[3] Call OS Loader  . . . . . . . . OK\n");
    gST->ConOut->SetAttribute(gST->ConOut, 0x0E);
    status = Print(L"Start Load\n");
    gST->ConOut->SetAttribute(gST->ConOut, 0x0F);
   
    // 그래픽 모드 설정 : 완료
    status = SetGraphicsMode(&graphicsInfo);
    if (EFI_ERROR (status)) { return status; }
    Print(L"- Graphics Mode: %d x %d . . . . . . . . OK\n", graphicsInfo.HoriziontalResolution, graphicsInfo.VerticalResolution);
    
    // 커널 이미지 정보 획득 : 완료
    status = GetKernelInfo(kernelInfo, kernel);
    if (EFI_ERROR (status)) { return status; }

    // 커널 + 페이지 테이블 들어갈 공간 할당 : 완료
    // (UINT64)((kernelInfo->FileSize / 4096) + 0.5)
    status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 2560, &kernelBaseAddress);
    if (EFI_ERROR (status)) { return status; }
    Print(L"- Allocate Memory for Kernel . . . . . . . . OK\n");
    Print(L"- kernelBaseAddress: 0x%x\n", kernelBaseAddress);
    
    // 페이징 설정 : TODO 하이어 하프 오류 수정
    SetPageTable(kernelInfo, kernelBaseAddress);
    Print(L"- Remapping Page Table . . . . . . . . OK\n");

    // 커널 읽어오기
    SetKernelToMemory()

    while(1);

    // 메모리 맵 가져오기
    GetMemoryInfo(&memoryInfo);
    
    status = gBS->ExitBootServices(imgHandle, memoryInfo.MemoryMapKey);
    if (EFI_ERROR (status)) { Print(L"- Call ExitBootServices() . . . . . . . . Fail\n"); return status; }
    
    // 커널로 점프


    return EFI_SUCCESS;
}