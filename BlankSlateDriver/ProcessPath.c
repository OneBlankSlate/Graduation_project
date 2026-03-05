#include"ProcessPath.h"
#include"ProcessHelper.h"

NTSTATUS PsGetProcessPath(PPROCESS_PATH_REQUEST ProcessPathRequest)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PEPROCESS EProcess = NULL;
	if (ProcessPathRequest->ProcessIdentity == NULL)
	{
		Status = STATUS_UNSUCCESSFUL;
		return Status;
	}
	//Id得EProcess
	Status = PsLookupProcessByProcessId(ProcessPathRequest->ProcessIdentity, &EProcess);
	if (Status != STATUS_SUCCESS)
	{
		return STATUS_UNSUCCESSFUL;
	}
	//获取完整路径
	if (GetProcessFullPathByEProcess(EProcess, ProcessPathRequest->ProcessPath, MAX_PATH) == TRUE)
	{
		return STATUS_SUCCESS;
	}
	return STATUS_UNSUCCESSFUL;
}
BOOLEAN GetProcessFullPathByEProcess(PVOID EProcess, WCHAR* ProcessFullPath, ULONG ProcessFullPathLength)
{
	BOOLEAN IsOk = FALSE;
	KPROCESSOR_MODE PreviousMode;
	HANDLE ProcessHandle = NULL;
	ULONG HandleAttributes = 0;
	if (PsIsRealProcess(EProcess) == TRUE)
	{
		//当前线程的模式
		PreviousMode = PsGetCurrentThreadPreviousMode();
		//句柄都是4的倍数   且ring0的句柄值均以8开头   0x80000004    0x00000004
		//x86  0x800007d8
		//x64  0xffffffff80000868
		HandleAttributes = (PreviousMode == KernelMode ? OBJ_KERNEL_HANDLE : 0);
		//通过对象体获得对象句柄
		if (NT_SUCCESS(ObOpenObjectByPointer(EProcess, HandleAttributes, NULL, PROCESS_QUERY_INFORMATION, *PsProcessType, PreviousMode, &ProcessHandle)))
		{
			PVOID BufferData = NULL;
			ULONG ReturnLength = 0;
			if (ZwQueryInformationProcess(ProcessHandle, ProcessImageFileName, BufferData, ReturnLength, &ReturnLength) == STATUS_INFO_LENGTH_MISMATCH)
			{
				if (BufferData = ExAllocatePool(PagedPool, ReturnLength))
				{
					if (NT_SUCCESS(ZwQueryInformationProcess(ProcessHandle, ProcessImageFileName, BufferData, ReturnLength, &ReturnLength)))
					{
						HANDLE FileHandle = NULL;
						OBJECT_ATTRIBUTES ObjectAttributes;
						IO_STATUS_BLOCK IoStatusBlock;
						InitializeObjectAttributes(&ObjectAttributes, (PUNICODE_STRING)BufferData, OBJ_CASE_INSENSITIVE | HandleAttributes, NULL, NULL);
						if (NT_SUCCESS(ZwOpenFile(&FileHandle, FILE_READ_ATTRIBUTES | SYNCHRONIZE, &ObjectAttributes, &IoStatusBlock, FILE_SHARE_READ, FILE_SYNCHRONOUS_IO_NONALERT)))
						{
							PFILE_OBJECT FileObject;
							//通过句柄获得对象
							if (NT_SUCCESS(ObReferenceObjectByHandle(FileHandle, FILE_READ_ATTRIBUTES, *IoFileObjectType, PreviousMode, (PVOID*)&FileObject, NULL)))
							{
								POBJECT_NAME_INFORMATION ObjectNameInfo;
								//通过文件对象获得文件绝对路径
								if (NT_SUCCESS(IoQueryFileDosDeviceName(FileObject, &ObjectNameInfo)))
								{
									if (((UNICODE_STRING*)ObjectNameInfo)->MaximumLength < ProcessFullPathLength)
									{
										memcpy(ProcessFullPath, ((UNICODE_STRING*)ObjectNameInfo)->Buffer, ((UNICODE_STRING*)ObjectNameInfo)->MaximumLength);
									}
									else
									{
										memcpy(ProcessFullPath, ((UNICODE_STRING*)ObjectNameInfo)->Buffer, ProcessFullPathLength);
									}
									IsOk = TRUE;
								}
								ObDereferenceObject(FileObject);
							}
							ZwClose(FileHandle);
						}
					}
					ExFreePool(BufferData);
				}
			}
			ZwClose(ProcessHandle);
		}
	}
	return IsOk;
}
BOOLEAN GetProcessFullPathByPeb(PVOID EProcess, WCHAR* ProcessFullPath, ULONG ProcessFullPathLength)
{
	PPEB Peb = NULL;
	KAPC_STATE ApcState;
	if (!PsIsRealProcess(EProcess))
	{
		return FALSE;
	}
	Peb = PsGetProcessPeb(EProcess);
	if (Peb == NULL)
	{
		return FALSE;
	}
	//进行上下背景文的切换
	KeStackAttachProcess(EProcess, &ApcState);
	__try
	{
		if (Peb->ProcessParameters->ImagePathName.MaximumLength < ProcessFullPathLength)
		{
			RtlCopyMemory(ProcessFullPath, Peb->ProcessParameters->ImagePathName.Buffer, Peb->ProcessParameters->ImagePathName.MaximumLength);
		}
		else
		{
			RtlCopyMemory(ProcessFullPath, Peb->ProcessParameters->ImagePathName.Buffer, ProcessFullPathLength);
		}
	}
	except(EXCEPTION_EXECUTE_HANDLER)
	{

	}
	KeUnstackDetachProcess(&ApcState);
	return TRUE;
}

