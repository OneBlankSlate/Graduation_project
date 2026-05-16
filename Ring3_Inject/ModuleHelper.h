#pragma once
#include<iostream>
#include<tchar.h>
#include<Windows.h>
#include<TlHelp32.h>
#include<vector> 

HMODULE get_remote_hmodule(unsigned long ProcessIdentify, const char* ModuleName);
BOOL get_remote_module_export(HANDLE ProcessHandle, HMODULE ModuleBase, PIMAGE_EXPORT_DIRECTORY ImageExportDirectory, IMAGE_DOS_HEADER ImageDosHeader, IMAGE_NT_HEADERS ImageNtHeaders);

namespace _MODULE_HELPER_
{


	HMODULE get_remote_hmodule(unsigned long ProcessIdentify, const char* ModuleName)
	{
		MODULEENTRY32W module_entry32 = { 0 };
		HANDLE snap_handle = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, ProcessIdentify);
		char buffer[256] = { 0 };

		module_entry32.dwSize = sizeof(MODULEENTRY32);
		Module32First(snap_handle, &module_entry32);
		do
		{
			size_t i;
			wcstombs_s(&i, buffer, 256, module_entry32.szModule, 256);  //双字转换成单字
			if (!_stricmp(buffer, (const char*)ModuleName))
			{
				//匹配到信息
				CloseHandle(snap_handle);
				return module_entry32.hModule;   //将模块地址返回
			}
			module_entry32.dwSize = sizeof(MODULEENTRY32);  //初始化结构重新扫描
		} while (Module32Next(snap_handle, &module_entry32));

		CloseHandle(snap_handle);
		return NULL;
	}

	BOOL get_remote_module_export(HANDLE ProcessHandle,
		HMODULE ModuleBase, PIMAGE_EXPORT_DIRECTORY ImageExportDirectory, IMAGE_DOS_HEADER ImageDosHeader, IMAGE_NT_HEADERS ImageNtHeaders) {
		PUCHAR buffer;
		PIMAGE_SECTION_HEADER image_section_header;
		int i = 0;
		DWORD virtual_address;

		if (!ImageExportDirectory)
			return FALSE;
		buffer = (PUCHAR)malloc(1000 * sizeof(UCHAR));

		memset(ImageExportDirectory, 0, sizeof(IMAGE_EXPORT_DIRECTORY));

		if (!ReadProcessMemory(ProcessHandle, (void*)ModuleBase, buffer, (SIZE_T)1000, NULL))
			return FALSE;
		image_section_header = (PIMAGE_SECTION_HEADER)(buffer + ImageDosHeader.e_lfanew + sizeof(IMAGE_NT_HEADERS));
		for (i = 0; i < ImageNtHeaders.FileHeader.NumberOfSections; i++, image_section_header++) {
			if (!image_section_header)  continue;
			if (_stricmp((char*)image_section_header->Name, ".edata") == 0) {
				if (!ReadProcessMemory(ProcessHandle, (void*)image_section_header->VirtualAddress, ImageExportDirectory, sizeof(IMAGE_EXPORT_DIRECTORY), NULL))
					continue;
				free(buffer);
				return TRUE;
			}
		}
		virtual_address = ImageNtHeaders.OptionalHeader.DataDirectory[0].VirtualAddress;
		if (!virtual_address)
			return FALSE;
		if (!ReadProcessMemory(ProcessHandle, (void*)((DWORD_PTR)ModuleBase + virtual_address), ImageExportDirectory, sizeof(IMAGE_EXPORT_DIRECTORY), NULL))
			return FALSE;
		free(buffer);
		return TRUE;
	}








}