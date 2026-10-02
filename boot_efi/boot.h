#include <Uefi.h>

#ifdef __INTELLISENSE__
  #define ALWAYS_INLINE static inline
#else
  #define ALWAYS_INLINE static inline __attribute__((always_inline))
#endif

typedef struct PageTable {
    UINT64 attr : 8;
    UINT64 avl1 : 4;
    UINT64 address : 40;
    UINT64 avl2 : 11;
    UINT64 nx : 1;
} PageTable;

typedef struct GraphicsInfo {
    UINT32 HoriziontalResolution;
    UINT32 VerticalResolution;
    UINT64 FrameBufferBase;
    UINTN FrameBufferSize;
    UINT32 PixelPerScanLine;
    UINT8 PixelFormat;
}GraphicsInfo;

typedef struct MemoryInfo {
    UINT64 MemorySize;
    EFI_MEMORY_DESCRIPTOR *MemoryMapBase;
    UINTN MemoryMapSize;
    UINTN MemoryMapDescSize;
    UINT32 MemoryMapDescVer;
    UINTN MemoryMapKey;
}MemoryInfo;

typedef struct SystemInfo {
    GraphicsInfo graphicsInfo;
    MemoryInfo memoryInfo;
}SystemInfo;