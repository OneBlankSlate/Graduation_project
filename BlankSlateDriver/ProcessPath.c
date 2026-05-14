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
PUNICODE_STRING GetNameByPath(PUNICODE_STRING ImagePath)
{
	// 参数验证
	if (!ImagePath || !ImagePath->Buffer || ImagePath->Length == 0) {
		return NULL;
	}

	// 查找最后一个路径分隔符的位置
	PWCHAR pFileNameStart = NULL;
	PWCHAR pCurrent = ImagePath->Buffer;
	PWCHAR pEnd = (PWCHAR)((PUCHAR)ImagePath->Buffer + ImagePath->Length);

	// 从字符串末尾向前查找最后一个分隔符
	for (PWCHAR p = pEnd - 1; p >= pCurrent; p--) {
		if (*p == L'\\' || *p == L'/') {
			pFileNameStart = p + 1;
			break;
		}
	}

	// 如果没有找到分隔符，整个字符串就是文件名
	if (!pFileNameStart) {
		pFileNameStart = pCurrent;
	}

	// 计算文件名长度（以字符计）
	ULONG nameLengthInChars = 0;
	PWCHAR pTemp = pFileNameStart;

	while (pTemp < pEnd) {
		nameLengthInChars++;
		pTemp++;
	}

	if (nameLengthInChars == 0) {
		return NULL;
	}

	// 计算文件名长度（以字节计）
	ULONG nameLengthInBytes = nameLengthInChars * sizeof(WCHAR);

	// 分配UNICODE_STRING结构及其缓冲区
	ULONG totalAllocSize = sizeof(UNICODE_STRING) + nameLengthInBytes + sizeof(WCHAR);
	PUNICODE_STRING pResult = (PUNICODE_STRING)ExAllocatePoolWithTag(
		NonPagedPoolNx,
		totalAllocSize,
		'name');

	if (!pResult) {
		return NULL;
	}

	// 初始化UNICODE_STRING结构
	RtlZeroMemory(pResult, totalAllocSize);

	// 设置UNICODE_STRING字段
	pResult->Buffer = (PWCHAR)((PUCHAR)pResult + sizeof(UNICODE_STRING));
	pResult->Length = (USHORT)nameLengthInBytes;
	pResult->MaximumLength = (USHORT)(nameLengthInBytes + sizeof(WCHAR));

	// 复制文件名
	RtlCopyMemory(pResult->Buffer, pFileNameStart, nameLengthInBytes);

	// 确保以空字符结尾
	pResult->Buffer[nameLengthInChars] = L'\0';


	return pResult;
}
