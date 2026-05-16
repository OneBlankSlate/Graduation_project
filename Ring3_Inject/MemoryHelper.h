#pragma once
#include<iostream>
#include<tchar.h>
#include<Windows.h>
#include"Common.h"




namespace _MEMORY_HELPER_
{
	BOOL IsBadReadPtr(CONST VOID* lp, UINT_PTR cb)
	{
		char* end_address;
		char* start_address;
		ULONG page_size;
		page_size = PAGE_SIZE;
		if (cb != 0){
			if (lp == NULL) {
				return TRUE;
			}
			start_address = (char*)lp;
			end_address = start_address + cb - 1;
			if (end_address < start_address){
				return TRUE;
			}
			else
			{
				__try
				{
					*(volatile CHAR*)start_address;    //获得当前页面是否能读
					//获得当前虚拟地址的所属页的基地址
					start_address = (PCHAR)((ULONG_PTR)start_address & (~((LONG)page_size - 1)));


					end_address = (PCHAR)((ULONG_PTR)end_address & (~((LONG)page_size - 1)));

					while (start_address != end_address)
					{
						start_address = start_address + page_size;
						*(volatile CHAR*)start_address;
					}
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
					return TRUE;
				}
			}
		}
		return FALSE;
	}

}

