#include <iostream>
#include <Windows.h>
#include <tchar.h>
using namespace std;

int _tmain() {
    // 1. 打开目标进程（替换PID为实际进程ID）
    DWORD pid = 7608; // 示例PID
    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, pid);
    if (hProcess == NULL) {
        _tprintf(_T("OpenProcess failed: %d\n"), GetLastError());
        _gettchar();
        return 1;
    }

    // 2. 在目标进程分配内存
    PVOID Base = VirtualAllocEx(hProcess, NULL, 100, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (Base == NULL) {
        _tprintf(_T("VirtualAllocEx failed: %d\n"), GetLastError());
        CloseHandle(hProcess);
        _gettchar();
        return 1;
    }

    // 3. 写入数据（注意Unicode字符串的字节数）
    wchar_t buffer[] = L"helloworld";
    SIZE_T bytesToWrite = sizeof(buffer); // 自动计算包含终止符的字节数
    SIZE_T bytesWritten = 0;

    if (WriteProcessMemory(hProcess, Base, buffer, bytesToWrite, &bytesWritten)) {
        _tprintf(_T("WriteProcessMemory success! Wrote %d bytes.\n"), bytesWritten);
    }
    else {
        _tprintf(_T("WriteProcessMemory failed: %d\n"), GetLastError());
    }

    // 4. 清理
    VirtualFreeEx(hProcess, Base, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    _gettchar();
    return 0;
}