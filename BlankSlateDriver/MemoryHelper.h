#pragma once
#include<fltKernel.h>
#include<wdm.h>

#define ALIGN_SIZE(Size,Align) (Size+Align-1)/Align*Align

#ifdef _WIN64
//#define USER_ADDRESS_END 0x000007fffffeffff  ´íÎóµÄ
#define USER_ADDRESS_END 0x00007FFF'FFFFFFFF
#define SYSTEM_ADDRESS_START 0xffff080000000000
#else
#define USER_ADDRESS_END 0x7ffeffff
#define SYSTEM_ADDRESS_START 0x80000000
#endif

#define CloseWriteProtect() \
        _asm{cli}\
        _asm{mov eax,cr0}\
        _asm{and eax,~0x10000}\
        _asm{mov cr0,eax}
#define OpenWriteProtect() \
        _asm{mov eax,cr0}\
        _asm{or eax,0x10000}\
        _asm{mov cr0,eax}\
        _asm{sti}



#define AllocatePoolWithTag(_POOL_TYPE,ViewSize)  ExAllocatePoolWithTag(_POOL_TYPE,ViewSize,'00xE')
#define FreePoolWithTag(VirtualAddress)  ExFreePoolWithTag(VirtualAddress,'00xE')
NTSYSAPI NTSTATUS NTAPI ZwMapViewOfSection(
    _In_ HANDLE SectionHandle,
    _In_ HANDLE ProcessHandle,
    _Outptr_result_bytebuffer_(*ViewSize) PVOID* BaseAddress,
    _In_ ULONG_PTR ZeroBits,
    _In_ SIZE_T CommitSize,
    _Inout_opt_ PLARGE_INTEGER SectionOffset,
    _Inout_ PSIZE_T ViewSize,
    _In_ SECTION_INHERIT InheritDisposition,
    _In_ ULONG AllocationType,
    _In_ ULONG Win32Protect
);

BOOLEAN MapFileInKernelSpace(WCHAR* FullPath, PVOID* VirtualAddress, PSIZE_T ViewSize);
NTSTATUS VirtualProtect(PMDL* Mdl, PVOID VirtualAddress1, PSIZE_T ViewSize, PVOID* VirtualAddress2);
NTSTATUS UnVirtualProtect(PMDL Mdl, PVOID VirtualAddress2);
//NTSTATUS PsDumpProcessModule(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);
NTSTATUS SafeCopyMemoryR32R0(ULONG_PTR Source, ULONG_PTR Destination, ULONG ViewSize);
NTSTATUS SafeCopyMemoryR02R3(ULONG_PTR Source, ULONG_PTR Destination, ULONG ViewSize);



