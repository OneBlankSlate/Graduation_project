#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <vector>

// 注入方式定义
#define INJECT_REMOTE_THREAD   0
#define INJECT_APC             1
#define INJECT_HOOK_EIP        2
#define INJECT_SETWINDOWSHOOKEX 3
#define INJECT_REGISTER        4

// ShellCode 定义 - 使用更大缓冲区以容纳长DLL路径
#ifdef _WIN64
// x64 ShellCode 布局:
// [+0]  sub rsp, 28h
// [+4]  lea rcx, [rip+offset]  -> 指向 [+43] DLL路径
// [+11] call [rip+offset]      -> 指向 [+35] LoadLibraryW地址
// [+17] add rsp, 28h
// [+21] jmp [rip+offset]       -> 指向 [+27] 原始RIP
// [+27] 原始RIP (8字节)
// [+35] LoadLibraryW地址 (8字节)
// [+43] DLL路径 (变长)
static UINT8 __ShellCode[0x400] = {
	0x48,0x83,0xEC,0x28,		// [+0] sub rsp, 28h
	0x48,0x8D,0x0D,			// [+4] lea rcx, [rip+offset]
	0x00,0x00,0x00,0x00,		// [+7] RIP-relative offset -> DLL路径
	0xff,0x15,					// [+11] call [rip+offset]
	0x00,0x00,0x00,0x00,		// [+13] RIP-relative offset -> LoadLibrary地址
	0x48,0x83,0xc4,0x28,		// [+17] add rsp, 28h
	0xff,0x25,					// [+21] jmp [rip+offset]
	0x00,0x00,0x00,0x00,		// [+23] RIP-relative offset -> 原始RIP
	0x00,0x00,0x00,0x00,		// [+27] 原始RIP (8字节)
	0x00,0x00,0x00,0x00,		// [+31]
	0x00,0x00,0x00,0x00,		// [+35] LoadLibraryW地址 (8字节)
	0x00,0x00,0x00,0x00,		// [+39]
	// [+43] DLL路径起始
};
#else
static UINT8 __ShellCode[0x400] = {
	0x60,						// [+0] pusha
	0x9c,						// [+1] pushf
	0x68,						// [+2] push
	0x00,0x00,0x00,0x00,		// [+3] DllPath Address
	0xff,0x15,					// [+7] call
	0x00,0x00,0x00,0x00,		// [+9] LoadLibrary Addr
	0x9d,						// [+13] popf
	0x61,						// [+14] popa
	0xff,0x25,					// [+15] jmp
	0x00,0x00,0x00,0x00,		// [+17] jmp eip
	0x00,0x00,0x00,0x00,		// [+21] eip 地址
	0x00,0x00,0x00,0x00,		// [+25] LoadLibrary地址
	0x00,0x00,0x00,0x00		// [+29] DllFullPath
};
#endif

typedef HMODULE(WINAPI* LPFN_LOADLIBRARYW)(LPCWSTR lpLibFileName);

namespace _THREAD_HELPER_
{
	inline BOOL get_thread_id(DWORD ProcessIdentify, std::vector<DWORD>& ThreadIdentifyV)
	{
		HANDLE thread_snap_handle = INVALID_HANDLE_VALUE;
		THREADENTRY32 te32;
		thread_snap_handle = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
		if (thread_snap_handle == INVALID_HANDLE_VALUE) return FALSE;
		te32.dwSize = sizeof(THREADENTRY32);
		if (!Thread32First(thread_snap_handle, &te32)) {
			CloseHandle(thread_snap_handle);
			return FALSE;
		}
		do {
			if (te32.th32OwnerProcessID == ProcessIdentify) {
				ThreadIdentifyV.push_back(te32.th32ThreadID);
			}
		} while (Thread32Next(thread_snap_handle, &te32));
		CloseHandle(thread_snap_handle);
		return TRUE;
	}
}

namespace _MODULE_HELPER_
{
	inline HMODULE get_remote_hmodule(unsigned long ProcessIdentify, const char* ModuleName)
	{
		MODULEENTRY32W module_entry32 = { 0 };
		HANDLE snap_handle = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, ProcessIdentify);
		if (snap_handle == INVALID_HANDLE_VALUE) return NULL;
		char buffer[256] = { 0 };
		module_entry32.dwSize = sizeof(MODULEENTRY32);
		Module32First(snap_handle, &module_entry32);
		do {
			size_t i;
			wcstombs_s(&i, buffer, 256, module_entry32.szModule, 256);
			if (!_stricmp(buffer, (const char*)ModuleName)) {
				CloseHandle(snap_handle);
				return module_entry32.hModule;
			}
			module_entry32.dwSize = sizeof(MODULEENTRY32);
		} while (Module32Next(snap_handle, &module_entry32));
		CloseHandle(snap_handle);
		return NULL;
	}

	inline BOOL get_remote_module_export(HANDLE ProcessHandle,
		HMODULE ModuleBase, PIMAGE_EXPORT_DIRECTORY ImageExportDirectory,
		IMAGE_DOS_HEADER ImageDosHeader, IMAGE_NT_HEADERS ImageNtHeaders)
	{
		PUCHAR buffer;
		PIMAGE_SECTION_HEADER image_section_header;
		int i = 0;
		DWORD virtual_address;
		if (!ImageExportDirectory) return FALSE;
		buffer = (PUCHAR)malloc(1000 * sizeof(UCHAR));
		memset(ImageExportDirectory, 0, sizeof(IMAGE_EXPORT_DIRECTORY));
		if (!ReadProcessMemory(ProcessHandle, (void*)ModuleBase, buffer, (SIZE_T)1000, NULL)) {
			free(buffer);
			return FALSE;
		}
		image_section_header = (PIMAGE_SECTION_HEADER)(buffer + ImageDosHeader.e_lfanew + sizeof(IMAGE_NT_HEADERS));
		for (i = 0; i < ImageNtHeaders.FileHeader.NumberOfSections; i++, image_section_header++) {
			if (!image_section_header) continue;
			if (_stricmp((char*)image_section_header->Name, ".edata") == 0) {
				if (!ReadProcessMemory(ProcessHandle, (void*)image_section_header->VirtualAddress, ImageExportDirectory, sizeof(IMAGE_EXPORT_DIRECTORY), NULL))
					continue;
				free(buffer);
				return TRUE;
			}
		}
		virtual_address = ImageNtHeaders.OptionalHeader.DataDirectory[0].VirtualAddress;
		if (!virtual_address) { free(buffer); return FALSE; }
		if (!ReadProcessMemory(ProcessHandle, (void*)((DWORD_PTR)ModuleBase + virtual_address), ImageExportDirectory, sizeof(IMAGE_EXPORT_DIRECTORY), NULL)) {
			free(buffer);
			return FALSE;
		}
		free(buffer);
		return TRUE;
	}
}

namespace _PE_HELPER_
{
	inline void release(DWORD* FunctionsAddress, DWORD* NameAddress, WORD* OrdinalAddress)
	{
		free(FunctionsAddress);
		free(NameAddress);
		free(OrdinalAddress);
	}

	inline void* get_remote_proc_address(unsigned long ProcessIdentify, HANDLE ProcessHandle, const char* ModuleName, const char* ProcedureName)
	{
		HMODULE module_base = _MODULE_HELPER_::get_remote_hmodule(ProcessIdentify, ModuleName);
		IMAGE_DOS_HEADER image_dos_header;
		IMAGE_NT_HEADERS image_nt_headers;
		IMAGE_EXPORT_DIRECTORY image_export_directory;
		DWORD* address_of_functions;
		DWORD* address_of_names;
		WORD* address_of_ordinals;
		int i = 0, j = 0, k = 0;
		DWORD_PTR virtual_address;
		DWORD_PTR view_size;
		DWORD_PTR function_address;
		DWORD_PTR function_name;
		char read_data[256] = { 0 };
		char forward_info[256] = { 0 };
		char forward_module_name[256] = { 0 };
		char forward_function_name[256] = { 0 };
		WORD ordinal;
		DWORD_PTR ordinal_function_address;
		DWORD_PTR ordinal_function_name;
		char buffer[256] = { 0 };

		if (!module_base) return NULL;
		if (!ReadProcessMemory(ProcessHandle, (void*)module_base, &image_dos_header, sizeof(IMAGE_DOS_HEADER), NULL) || image_dos_header.e_magic != IMAGE_DOS_SIGNATURE) return NULL;
		if (!ReadProcessMemory(ProcessHandle, (void*)((DWORD_PTR)module_base + image_dos_header.e_lfanew), &image_nt_headers, sizeof(IMAGE_NT_HEADERS), NULL) || image_nt_headers.Signature != IMAGE_NT_SIGNATURE) return NULL;
		if (!_MODULE_HELPER_::get_remote_module_export(ProcessHandle, module_base, &image_export_directory, image_dos_header, image_nt_headers)) return NULL;

		address_of_functions = (DWORD*)malloc(image_export_directory.NumberOfFunctions * sizeof(DWORD));
		address_of_names = (DWORD*)malloc(image_export_directory.NumberOfNames * sizeof(DWORD));
		address_of_ordinals = (WORD*)malloc(image_export_directory.NumberOfNames * sizeof(WORD));

		if (!ReadProcessMemory(ProcessHandle, (void*)((DWORD_PTR)module_base + (DWORD_PTR)image_export_directory.AddressOfFunctions), address_of_functions, image_export_directory.NumberOfFunctions * sizeof(DWORD), NULL)) {
			release(address_of_functions, address_of_names, address_of_ordinals);
			return NULL;
		}
		if (!ReadProcessMemory(ProcessHandle, (void*)((DWORD_PTR)module_base + (DWORD_PTR)image_export_directory.AddressOfNames), address_of_names, image_export_directory.NumberOfNames * sizeof(DWORD), NULL)) {
			release(address_of_functions, address_of_names, address_of_ordinals);
			return NULL;
		}
		if (!ReadProcessMemory(ProcessHandle, (void*)((DWORD_PTR)module_base + (DWORD_PTR)image_export_directory.AddressOfNameOrdinals), address_of_ordinals, image_export_directory.NumberOfNames * sizeof(WORD), NULL)) {
			release(address_of_functions, address_of_names, address_of_ordinals);
			return NULL;
		}

		virtual_address = ((DWORD_PTR)module_base + image_nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
		view_size = (virtual_address + image_nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size);

		for (i = 0; i < image_export_directory.NumberOfNames; ++i) {
			function_address = (DWORD_PTR)module_base + address_of_functions[i];
			function_name = (DWORD_PTR)module_base + address_of_names[i];
			memset(read_data, 0, 256);
			if (!ReadProcessMemory(ProcessHandle, (void*)function_name, read_data, 256, NULL)) continue;
			if (_stricmp(read_data, (const char*)ProcedureName) != 0) continue;

			if (function_address >= virtual_address && function_address <= virtual_address + view_size) {
				memset(forward_info, 0, 256);
				if (!ReadProcessMemory(ProcessHandle, (void*)function_address, forward_info, 256, NULL)) continue;
				memset(forward_module_name, 0, 256);
				memset(forward_function_name, 0, 256);
				j = 0;
				for (; forward_info[j] != '.'; j++) {
					forward_module_name[j] = forward_info[j];
				}
				j++;
				forward_module_name[j] = '\0';
				k = 0;
				for (; forward_info[j] != '\0'; j++, k++)
					forward_function_name[k] = forward_info[j];
				k++;
				forward_function_name[k] = '\0';
				strcat_s(forward_module_name, 256, ".dll");
				release(address_of_functions, address_of_names, address_of_ordinals);
				return get_remote_proc_address(ProcessIdentify, ProcessHandle, forward_module_name, forward_function_name);
			}

			ordinal = address_of_ordinals[i];
			if (ordinal >= image_export_directory.NumberOfNames) {
				release(address_of_functions, address_of_names, address_of_ordinals);
				return NULL;
			}
			if (ordinal != i) {
				ordinal_function_address = ((DWORD_PTR)module_base + (DWORD_PTR)address_of_functions[ordinal]);
				ordinal_function_name = ((DWORD_PTR)module_base + (DWORD_PTR)address_of_names[ordinal]);
				memset(buffer, 0, 256);
				release(address_of_functions, address_of_names, address_of_ordinals);
				if (!ReadProcessMemory(ProcessHandle, (void*)ordinal_function_name, buffer, 256, NULL)) return NULL;
				else return (void*)ordinal_function_address;
			}
			else {
				release(address_of_functions, address_of_names, address_of_ordinals);
				return (void*)function_address;
			}
		}
		release(address_of_functions, address_of_names, address_of_ordinals);
		return NULL;
	}
}

namespace _MEMORY_HELPER_
{
	inline BOOL IsBadReadPtrCustom(CONST VOID* lp, UINT_PTR cb)
	{
		char* end_address;
		char* start_address;
		ULONG page_size = 0x1000;
		if (cb != 0) {
			if (lp == NULL) return TRUE;
			start_address = (char*)lp;
			end_address = start_address + cb - 1;
			if (end_address < start_address) return TRUE;
			else {
				__try {
					*(volatile CHAR*)start_address;
					start_address = (PCHAR)((ULONG_PTR)start_address & (~((LONG)page_size - 1)));
					end_address = (PCHAR)((ULONG_PTR)end_address & (~((LONG)page_size - 1)));
					while (start_address != end_address) {
						start_address = start_address + page_size;
						*(volatile CHAR*)start_address;
					}
				}
				__except (EXCEPTION_EXECUTE_HANDLER) {
					return TRUE;
				}
			}
		}
		return FALSE;
	}
}

// 注入辅助函数
namespace _INJECT_HELPER_
{
	inline void inject_release(PVOID lpAddress, HANDLE ThreadHandle, HANDLE ProcessHandle)
	{
		if (lpAddress != NULL && ProcessHandle != NULL) {
			VirtualFreeEx(ProcessHandle, lpAddress, 0, MEM_RELEASE);
		}
		if (ThreadHandle != NULL && ThreadHandle != INVALID_HANDLE_VALUE) {
			CloseHandle(ThreadHandle);
		}
	}

	// 获取LoadLibraryW的地址（kernel32.dll在所有进程中基址相同）
	inline void* GetLoadLibraryWAddr()
	{
		HMODULE hKernel32 = GetModuleHandle(L"kernel32.dll");
		if (hKernel32 == NULL) return NULL;
		return (void*)GetProcAddress(hKernel32, "LoadLibraryW");
	}

	// 远程线程注入
	inline BOOL create_remote_thread_inject(HANDLE ProcessHandle, DWORD ProcessIdentity, const wchar_t* DllPath)
	{
		void* pLoadLibraryW = GetLoadLibraryWAddr();
		if (pLoadLibraryW == NULL) return FALSE;

		LPVOID base_address = VirtualAllocEx(ProcessHandle, NULL, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (base_address == NULL) return FALSE;

		SIZE_T write_length = 0;
		BOOL ret = WriteProcessMemory(ProcessHandle, base_address, DllPath, ((wcslen(DllPath) + 1) * 2), &write_length);
		if (ret == FALSE) {
			VirtualFreeEx(ProcessHandle, base_address, 0, MEM_RELEASE);
			return FALSE;
		}

		HANDLE thread_handle = CreateRemoteThread(ProcessHandle, NULL, NULL, (LPTHREAD_START_ROUTINE)pLoadLibraryW, (LPVOID)base_address, NULL, NULL);
		if (thread_handle == NULL) {
			VirtualFreeEx(ProcessHandle, base_address, 0, MEM_RELEASE);
			return FALSE;
		}

		// 等待远程线程执行完毕
		WaitForSingleObject(thread_handle, INFINITE);

		// 检查线程退出码，LoadLibraryW成功返回非0，失败返回0
		DWORD exit_code = 0;
		GetExitCodeThread(thread_handle, &exit_code);

		CloseHandle(thread_handle);

		if (exit_code == 0) {
			// LoadLibraryW执行失败，DLL未能加载
			return FALSE;
		}

		return TRUE;
	}

	// APC 注入
	inline BOOL apc_inject(HANDLE ProcessHandle, DWORD ProcessIdentity, const wchar_t* DllPath)
	{
		LPVOID virtual_address = NULL;
		std::vector<DWORD> thread_identity;
		LPFN_LOADLIBRARYW LoadLibrary_Pointer = NULL;
		HANDLE thread_handle = NULL;

		virtual_address = VirtualAllocEx(ProcessHandle, NULL, (wcslen(DllPath) + 1) * 2, MEM_COMMIT, PAGE_READWRITE);
		if (virtual_address == NULL) return FALSE;

		if (WriteProcessMemory(ProcessHandle, virtual_address, DllPath, (wcslen(DllPath) + 1) * 2, NULL) == FALSE) {
			VirtualFreeEx(ProcessHandle, virtual_address, 0, MEM_RELEASE);
			return FALSE;
		}

		if (_THREAD_HELPER_::get_thread_id(ProcessIdentity, thread_identity) == FALSE) {
			VirtualFreeEx(ProcessHandle, virtual_address, 0, MEM_RELEASE);
			return FALSE;
		}

		LoadLibrary_Pointer = (LPFN_LOADLIBRARYW)GetLoadLibraryWAddr();
		if (LoadLibrary_Pointer == NULL) {
			VirtualFreeEx(ProcessHandle, virtual_address, 0, MEM_RELEASE);
			return FALSE;
		}

		for (int i = (int)thread_identity.size() - 1; i >= 0; i--) {
			thread_handle = OpenThread(THREAD_SET_CONTEXT, FALSE, (DWORD)thread_identity[i]);
			if (thread_handle) {
				QueueUserAPC((PAPCFUNC)LoadLibrary_Pointer, thread_handle, (ULONG_PTR)virtual_address);
				CloseHandle(thread_handle);
			}
		}

		return TRUE;
	}

	// Hook EIP 注入
	inline BOOL hook_eip_inject(HANDLE ProcessHandle, DWORD ProcessIdentity, const wchar_t* DllPath)
	{
		CONTEXT thread_context = { 0 };
		PVOID virtual_address = NULL;
		std::vector<DWORD> thread_identity;
		HANDLE thread_handle = NULL;
		LPFN_LOADLIBRARYW LoadLibrary_Pointer = NULL;
		UINT8 ShellCodeBuf[0x400];

		// 检查DLL路径长度，确保ShellCode缓冲区足够
		size_t dll_path_bytes = (wcslen(DllPath) + 1) * sizeof(WCHAR);
#ifdef _WIN64
		if (43 + dll_path_bytes > sizeof(ShellCodeBuf)) return FALSE;
#else
		if (29 + dll_path_bytes > sizeof(ShellCodeBuf)) return FALSE;
#endif

		_THREAD_HELPER_::get_thread_id(ProcessIdentity, thread_identity);
		if (thread_identity.empty()) return FALSE;

		// 先拷贝ShellCode模板
		memcpy(ShellCodeBuf, __ShellCode, sizeof(__ShellCode));
		// 清零剩余部分（确保DLL路径区域干净）
		memset(ShellCodeBuf + sizeof(__ShellCode), 0, sizeof(ShellCodeBuf) - sizeof(__ShellCode));

		// 分配可执行内存
		virtual_address = VirtualAllocEx(ProcessHandle, NULL, sizeof(ShellCodeBuf), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (virtual_address == NULL) return FALSE;

		// 获取LoadLibraryW地址
		LoadLibrary_Pointer = (LPFN_LOADLIBRARYW)GetLoadLibraryWAddr();
		if (LoadLibrary_Pointer == NULL) {
			VirtualFreeEx(ProcessHandle, virtual_address, 0, MEM_RELEASE);
			return FALSE;
		}

		// 挂起主线程
		thread_handle = OpenThread(THREAD_ALL_ACCESS, FALSE, thread_identity[0]);
		if (thread_handle == NULL) {
			VirtualFreeEx(ProcessHandle, virtual_address, 0, MEM_RELEASE);
			return FALSE;
		}
		SuspendThread(thread_handle);
		thread_context.ContextFlags = CONTEXT_ALL;
		if (GetThreadContext(thread_handle, &thread_context) == FALSE) {
			ResumeThread(thread_handle);
			CloseHandle(thread_handle);
			VirtualFreeEx(ProcessHandle, virtual_address, 0, MEM_RELEASE);
			return FALSE;
		}

#ifdef _WIN64
		// ============ x64 ShellCode 填充 ============
		// [+43] 写入DLL路径
		memcpy(ShellCodeBuf + 43, DllPath, dll_path_bytes);

		// [+7] lea rcx, [rip+offset]: RIP = 当前指令地址+7, 目标 = virtual_address+43
		// offset = 目标地址 - RIP值 = (virtual_address+43) - (virtual_address+4+7) = 32
		*(PULONG)(ShellCodeBuf + 7) = (ULONG)(43 - 11);  // = 32

		// [+35] 写入LoadLibraryW绝对地址 (8字节)
		*(PULONG_PTR)(ShellCodeBuf + 35) = (ULONG_PTR)LoadLibrary_Pointer;

		// [+13] call [rip+offset]: RIP = 当前指令地址+6, 目标 = virtual_address+35
		// offset = (virtual_address+35) - (virtual_address+11+6) = 18
		*(PULONG)(ShellCodeBuf + 13) = (ULONG)(35 - 17);  // = 18

		// [+27] 写入原始RIP (8字节)
		*(PULONG_PTR)(ShellCodeBuf + 27) = thread_context.Rip;

		// [+23] jmp [rip+offset]: RIP = 当前指令地址+6, 目标 = virtual_address+27
		// offset = (virtual_address+27) - (virtual_address+21+6) = 0
		*(PULONG)(ShellCodeBuf + 23) = (ULONG)(27 - 27);  // = 0
#else
		// ============ x86 ShellCode 填充 ============
		// [+29] 写入DLL路径
		memcpy(ShellCodeBuf + 29, DllPath, dll_path_bytes);

		// [+3] push DllPath地址
		*(PULONG)(ShellCodeBuf + 3) = (ULONG)((ULONG_PTR)virtual_address + 29);

		// [+25] LoadLibrary绝对地址
		*(PULONG)(ShellCodeBuf + 25) = (ULONG)LoadLibrary_Pointer;

		// [+9] call [addr] 间接调用地址
		*(PULONG)(ShellCodeBuf + 9) = (ULONG)((ULONG_PTR)virtual_address + 25);

		// [+21] 保存原始EIP
		*(PULONG)(ShellCodeBuf + 21) = thread_context.Eip;

		// [+17] jmp [addr] 间接跳转地址
		*(PULONG)(ShellCodeBuf + 17) = (ULONG)((ULONG_PTR)virtual_address + 21);
#endif

		// 将ShellCode写入目标进程
		if (!WriteProcessMemory(ProcessHandle, virtual_address, ShellCodeBuf, sizeof(ShellCodeBuf), NULL)) {
			ResumeThread(thread_handle);
			CloseHandle(thread_handle);
			VirtualFreeEx(ProcessHandle, virtual_address, 0, MEM_RELEASE);
			return FALSE;
		}

		// 修改线程上下文，使其执行ShellCode
#ifdef _WIN64
		thread_context.Rip = (ULONG_PTR)virtual_address;
#else
		thread_context.Eip = (ULONG_PTR)virtual_address;
#endif
		if (!SetThreadContext(thread_handle, &thread_context)) {
			ResumeThread(thread_handle);
			CloseHandle(thread_handle);
			VirtualFreeEx(ProcessHandle, virtual_address, 0, MEM_RELEASE);
			return FALSE;
		}
		ResumeThread(thread_handle);
		CloseHandle(thread_handle);
		return TRUE;
	}

	// SetWindowsHookEx 注入
	inline BOOL set_window_hookex_inject(HANDLE ProcessHandle, DWORD ProcessIdentity, const wchar_t* DllPath)
	{
		std::vector<DWORD> thread_identity;
		HHOOK hook_handle = NULL;
		FARPROC HookProc = NULL;
		HMODULE module_base = NULL;

		if (_THREAD_HELPER_::get_thread_id(ProcessIdentity, thread_identity) == FALSE) return FALSE;

		// 在本地加载DLL
		module_base = LoadLibrary(DllPath);
		if (module_base == NULL) return FALSE;

		// 直接获取已知导出函数 Sub_1 的地址
		HookProc = GetProcAddress(module_base, "Sub_1");
		if (HookProc == NULL) {
			FreeLibrary(module_base);
			return FALSE;
		}

		// 安装消息钩子，触发DLL加载到目标进程
		for (int i = 0; i < (int)thread_identity.size(); ++i) {
			hook_handle = SetWindowsHookEx(WH_GETMESSAGE, (HOOKPROC)HookProc, module_base, (DWORD)thread_identity[i]);
			if (hook_handle != NULL) {
				break;
			}
		}

		if (hook_handle == NULL) {
			FreeLibrary(module_base);
			return FALSE;
		}

		// 向目标线程发送消息，触发钩子回调，使DLL被加载到目标进程
		for (int i = 0; i < (int)thread_identity.size(); ++i) {
			PostThreadMessage((DWORD)thread_identity[i], WM_NULL, 0, 0);
		}

		// 等待DLL加载到目标进程
		Sleep(500);

		// 卸载钩子，防止Sub_1被持续调用；DLL已加载到目标进程中，卸载钩子不影响注入状态
		UnhookWindowsHookEx(hook_handle);

		return TRUE;
	}

	// 注册表注入
	inline BOOL register_inject(const wchar_t* DllPath)
	{
		HKEY key_handle = NULL;
		BYTE bufferdata[MAX_PATH] = { 0 };
		LONG isok = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows", 0, KEY_ALL_ACCESS, &key_handle);
		if (isok != ERROR_SUCCESS) return FALSE;

		memcpy(bufferdata, DllPath, (wcslen(DllPath) + 1) * sizeof(WCHAR));
		isok = RegSetValueEx(key_handle, L"AppInit_DLLs", 0, REG_SZ, bufferdata, (DWORD)((wcslen(DllPath) + 1) * sizeof(WCHAR)));

		if (key_handle) RegCloseKey(key_handle);
		return (isok == ERROR_SUCCESS);
	}

	// 统一注入入口
	inline BOOL DoInject(DWORD ProcessId, int InjectMethod, const wchar_t* DllPath)
	{
		HANDLE process_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ProcessId);
		if (process_handle == NULL && InjectMethod != INJECT_REGISTER) return FALSE;

		BOOL result = FALSE;
		switch (InjectMethod)
		{
		case INJECT_REMOTE_THREAD:
			result = create_remote_thread_inject(process_handle, ProcessId, DllPath);
			break;
		case INJECT_APC:
			result = apc_inject(process_handle, ProcessId, DllPath);
			break;
		case INJECT_HOOK_EIP:
			result = hook_eip_inject(process_handle, ProcessId, DllPath);
			break;
		case INJECT_SETWINDOWSHOOKEX:
			result = set_window_hookex_inject(process_handle, ProcessId, DllPath);
			break;
		case INJECT_REGISTER:
			result = register_inject(DllPath);
			break;
		}

		if (process_handle != NULL) CloseHandle(process_handle);
		return result;
	}
}
