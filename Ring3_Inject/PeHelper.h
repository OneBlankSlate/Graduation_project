#pragma once
#include<iostream>
#include<tchar.h>
#include<Windows.h>
#include<TlHelp32.h>
#include<vector> 
#include"ModuleHelper.h"

//
//void* get_remote_proc_address(unsigned long ProcessIdentify, HANDLE ProcessHandle, const char* ModuleName, const char* ProcedureName);
//void release(DWORD* FunctionsAddress, DWORD* NameAddress, WORD* OrdinalAddress);

namespace _PE_HELPER_
{
	void release(DWORD* FunctionsAddress, DWORD* NameAddress, WORD* OrdinalAddress)
	{
		free(FunctionsAddress);
		free(NameAddress);
		free(OrdinalAddress);
	}
	void* get_remote_proc_address(unsigned long ProcessIdentify, HANDLE ProcessHandle, const char* ModuleName, const char* ProcedureName) {
		HMODULE module_base = _MODULE_HELPER_::get_remote_hmodule(ProcessIdentify, ModuleName);   //GetModuleHandle
		IMAGE_DOS_HEADER image_dos_header;
		IMAGE_NT_HEADERS image_nt_headers;
		IMAGE_EXPORT_DIRECTORY image_export_directory;
		DWORD* address_of_functions;
		DWORD* address_of_names;
		WORD* address_of_ordinals;
		int i = 0;   
		int j = 0;
		int k = 0;
		DWORD_PTR virtual_address;   //导出表目录的基地址
		DWORD_PTR view_size;         //导出表目录的末尾
		DWORD_PTR function_address;   //函数地址
		DWORD_PTR function_name;   //函数名称
		char read_data[256] = { 0 };
		char forward_info[256] = { 0 };   //转发器信息
		char forward_module_name[256] = { 0 };   //转发模块名称
		char forward_function_name[256] = { 0 };   //转发函数名称
		WORD ordinal;
		DWORD_PTR ordinal_function_address;
		DWORD_PTR ordinal_function_name;
		char buffer[256] = { 0 };

		if (!module_base) return NULL;
		if (!ReadProcessMemory(ProcessHandle, (void*)module_base, &image_dos_header, sizeof(IMAGE_DOS_HEADER), NULL) || image_dos_header.e_magic != IMAGE_DOS_SIGNATURE) return NULL;
		if (!ReadProcessMemory(ProcessHandle, (void*)((DWORD_PTR)module_base + image_dos_header.e_lfanew), &image_nt_headers, sizeof(IMAGE_NT_HEADERS), NULL) || image_nt_headers.Signature != IMAGE_NT_SIGNATURE)  return NULL;
		if (!_MODULE_HELPER_::get_remote_module_export(ProcessHandle, module_base, &image_export_directory, image_dos_header, image_nt_headers))  return NULL;
		address_of_functions = (DWORD*)malloc(image_export_directory.NumberOfFunctions * sizeof(DWORD));
		address_of_names = (DWORD*)malloc(image_export_directory.NumberOfNames * sizeof(DWORD));
		address_of_ordinals = (WORD*)malloc(image_export_directory.NumberOfNames * sizeof(WORD));
		if (!ReadProcessMemory(ProcessHandle, (void*)((DWORD_PTR)module_base + (DWORD_PTR)image_export_directory.AddressOfFunctions), address_of_functions, image_export_directory.NumberOfFunctions * sizeof(DWORD), NULL)) {
			_PE_HELPER_::release(address_of_functions, address_of_names, address_of_ordinals);
			return NULL;
		}
		if (!ReadProcessMemory(ProcessHandle, (void*)((DWORD_PTR)module_base + (DWORD_PTR)image_export_directory.AddressOfNames), address_of_names, image_export_directory.NumberOfNames * sizeof(DWORD), NULL)) {
			_PE_HELPER_::release(address_of_functions, address_of_names, address_of_ordinals);
			return NULL;
		}
		if (!ReadProcessMemory(ProcessHandle, (void*)((DWORD_PTR)module_base + (DWORD_PTR)image_export_directory.AddressOfNameOrdinals), address_of_ordinals, image_export_directory.NumberOfNames * sizeof(WORD), NULL)) {
			_PE_HELPER_::release(address_of_functions, address_of_names, address_of_ordinals);
			return NULL;
		}
		virtual_address = ((DWORD_PTR)module_base + image_nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);  //导出表目录的绝对值
		view_size = (virtual_address + image_nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size);
		for (i = 0; i < image_export_directory.NumberOfNames; ++i) {
			function_address = (DWORD_PTR)module_base + address_of_functions[i];   //导出函数地址
			function_name = (DWORD_PTR)module_base + address_of_names[i];       //导出函数的名称
			memset(read_data, 0, 256);
			if (!ReadProcessMemory(ProcessHandle, (void*)function_name, read_data, 256, NULL)) 		continue;
			if (_stricmp(read_data, (const char*)ProcedureName) != 0) continue;
			if (function_address >= virtual_address && function_address <= virtual_address + view_size) {
				memset(forward_info, 0, 256);
				if (!ReadProcessMemory(ProcessHandle, (void*)function_address, forward_info, 256, NULL))  continue;
				memset(forward_module_name, 0, 256);
				memset(forward_function_name, 0, 256);
				j = 0;
				for (; forward_info[j] != '.'; j++)    //xxx.Sub_5\0
				{
					forward_module_name[j] = forward_info[j];  //解析出转发模块的名称
				}
				j++;
				forward_module_name[j] = '\0';   //xxx\0
				k = 0;
				for (; forward_info[j] != '\0'; j++, k++)
					forward_function_name[k] = forward_info[j];
				k++;
				forward_function_name[k] = '\0';   //Sub_5\0
				strcat_s(forward_module_name, 256, ".dll");  //xxx.dll
				_PE_HELPER_::release(address_of_functions, address_of_names, address_of_ordinals);
				return _PE_HELPER_::get_remote_proc_address(ProcessIdentify, ProcessHandle, forward_module_name, forward_function_name);     //递归调用
			}
			ordinal = address_of_ordinals[i];
			if (ordinal >= image_export_directory.NumberOfNames) {     //索引导出
				return NULL;
			}
			if (ordinal != i) {
				ordinal_function_address = ((DWORD_PTR)module_base + (DWORD_PTR)address_of_functions[ordinal]);
				ordinal_function_name = ((DWORD_PTR)module_base + (DWORD_PTR)address_of_names[ordinal]);
				memset(buffer, 0, 256);
				_PE_HELPER_::release(address_of_functions, address_of_names, address_of_ordinals);
				if (!ReadProcessMemory(ProcessHandle, (void*)ordinal_function_name, buffer, 256, NULL)) return NULL;
				else   return (void*)ordinal_function_address;
			}
			else {      			//正常的函数名称导出
				_PE_HELPER_::release(address_of_functions, address_of_names, address_of_ordinals);
				return (void*)function_address;
			}
		}
		_PE_HELPER_::release(address_of_functions, address_of_names, address_of_ordinals);
		return NULL;
	}


}