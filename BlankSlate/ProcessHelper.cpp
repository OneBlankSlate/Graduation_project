#include"ProcessHelper.h"
#include"IoControlHelper.h"

HANDLE GetProcessIdentity(const TCHAR* ImageName)
{
	ULONG BufferLength = 0x1000;
	void* BufferData = NULL;
	NTSTATUS Status = STATUS_INFO_LENGTH_MISMATCH;   //长度不匹配 内存动态申请


	HMODULE ModuleBase = (HMODULE)GetModuleHandle(_T("ntdll.dll"));

	LPFN_NTQUERYSYSTEMINFORMATION NtQuerySystemInformation =
		(LPFN_NTQUERYSYSTEMINFORMATION)GetProcAddress(ModuleBase, "NtQuerySystemInformation");

	if (NtQuerySystemInformation == NULL)
	{
		return NULL;
	}
	//获得当前进程默认堆
	void* HeapHandle = GetProcessHeap();

	HANDLE ProcessIdentity = 0;

	BOOL IsLoop = FALSE;
	BOOL IsOk = FALSE;
	while (!IsLoop)
	{
		//在当前进程的默认堆中
		BufferData = HeapAlloc(HeapHandle, HEAP_ZERO_MEMORY, BufferLength);  //当前进程默认堆申请内存
		if (BufferData == NULL)
		{
			return NULL;
		}

		Status = NtQuerySystemInformation(SystemProcessInformation, BufferData, BufferLength, &BufferLength);
		if (Status == STATUS_INFO_LENGTH_MISMATCH)
		{
			IsOk = TRUE;
			HeapFree(HeapHandle, NULL, BufferData);
			BufferLength *= 2;
		}
		else if (!NT_SUCCESS(Status))   //不是内存不够的报错
		{
			HeapFree(HeapHandle, NULL, BufferData);
			return 0;
		}
		else
		{

			IsOk = FALSE;

			PSYSTEM_PROCESS_INFORMATION SystemProcessInfo = (PSYSTEM_PROCESS_INFORMATION)BufferData;
			while (SystemProcessInfo)
			{

				if (SystemProcessInfo->UniqueProcessId == 0)
				{

				}
				else
				{
					if (_tcsicmp(SystemProcessInfo->ImageName.Buffer, ImageName) == 0)
					{
						ProcessIdentity = SystemProcessInfo->UniqueProcessId;
						IsOk = TRUE;

						break;
					}
				}

				if (!SystemProcessInfo->NextEntryOffset)
				{
					break;
				}

				SystemProcessInfo = (PSYSTEM_PROCESS_INFORMATION)((unsigned char*)SystemProcessInfo + SystemProcessInfo->NextEntryOffset);
			}
			if (BufferData)
			{
				HeapFree(HeapHandle, NULL, BufferData);
			}

		}

		if (ProcessIdentity != 0)
		{
			break;
		}
		else if (!IsOk)
		{
			// Don't continuously search...
			break;
		}
	}

	return ProcessIdentity;
}

BOOL EnumProcess(vector<PROCESS_INFORMATION_ENTRY>& ProcessInfo)
{
	OPERATE_TYPE OperateType = ENUM_PROCESS;
	ULONG NumberOfProcess = 1000;
	PPROCESS_INFORMATIONS v5 = NULL;
	BOOL IsOk = FALSE;
	ProcessInfo.clear();
	do
	{
		ULONG Size = sizeof(PROCESS_INFORMATIONS) + NumberOfProcess * sizeof(PROCESS_INFORMATION_ENTRY);
		v5 = (PPROCESS_INFORMATIONS)malloc(Size);
		memset(v5, 0, Size);
		if (v5)
		{
			IsOk = CommunicateDevice(&OperateType, sizeof(OPERATE_TYPE), (PVOID)v5, Size, NULL);
		}
		NumberOfProcess += 100;
	} while (!IsOk && GetLastError() == ERROR_INSUFFICIENT_BUFFER);
	if (IsOk && v5->NumberOfProcess > 0)
	{
		NumberOfProcess = v5->NumberOfProcess;
		for (ULONG i = 0; i < NumberOfProcess; i++)
		{
			ProcessInfo.push_back(v5->ProcessInfo[i]);
		}
		IsOk = TRUE;
	}
	if (v5)
	{
		free(v5);
		v5 = NULL;
	}
	return IsOk;

}


