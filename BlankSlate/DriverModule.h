#pragma once
#include<Windows.h>
#include<iostream>

BOOL LoadDriver(const char* pDriverPath);
BOOL UnloadDriver(const char* pServiceName);
