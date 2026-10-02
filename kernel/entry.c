#include "../boot_efi/boot.h"

void KernelEntry(SystemInfo* systemInfo) {
    GraphicsInfo gi = systemInfo->graphicsInfo;
    MemoryInfo mi = systemInfo->memoryInfo;
    UINT32 color = 0;
    UINT32 *frameBufferBase = (UINT32 *)gi.FrameBufferBase;

    if (gi.PixelFormat == 0) {
        color = 0x00ccff00;
    }
    else {
        color = 0xffcc0000;
    }
    
    for (UINTN i = 0; i < (UINTN)(gi.VerticalResolution / 3); i++) {
        for (UINTN j = 0; j < gi.PixelPerScanLine; j++) {
            *(frameBufferBase + ((gi.PixelPerScanLine * i) + j)) = color;
        }
    }
}