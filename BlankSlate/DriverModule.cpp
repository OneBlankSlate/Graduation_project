#include"DriverModule.h"
BOOL LoadDriver(const char* pDriverPath) {
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hService = NULL;
    BOOL bRet = FALSE;
    char serviceName[MAX_PATH];
    char displayName[MAX_PATH];

    // 从路径中提取驱动文件名作为服务名（去掉路径和扩展名）
    const char* pFileName = strrchr(pDriverPath, '\\');
    if (pFileName == NULL) {
        pFileName = pDriverPath;
    }
    else {
        pFileName++; // 跳过反斜杠
    }

    // 复制文件名（去掉 .sys 扩展名）
    strncpy(serviceName, pFileName, MAX_PATH);
    char* pExt = strstr(serviceName, ".sys");
    if (pExt != NULL) {
        *pExt = '\0'; // 截断扩展名
    }

    snprintf(displayName, MAX_PATH, "Driver %s", serviceName);

    // 1. 打开服务控制管理器
    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCManager == NULL) {
        printf("OpenSCManager 失败! 错误代码: %lu\n", GetLastError());
        goto cleanup;
    }

    // 2. 创建驱动服务
    hService = CreateServiceA(
        hSCManager,           // SCManager 数据库句柄
        serviceName,          // 服务名称
        displayName,          // 显示名称
        SERVICE_ALL_ACCESS,   // 访问权限
        SERVICE_KERNEL_DRIVER,// 服务类型：内核驱动
        SERVICE_DEMAND_START, // 启动类型：按需启动
        SERVICE_ERROR_NORMAL, // 错误控制
        pDriverPath,          // 驱动文件路径
        NULL,                 // 加载顺序组
        NULL,                 // 标记 ID
        NULL,                 // 依赖关系
        NULL,                 // 账户名（NULL 表示 LocalSystem）
        NULL                  // 密码
    );

    if (hService == NULL) {
        DWORD dwError = GetLastError();
        if (dwError == ERROR_SERVICE_EXISTS) {
            // 服务已存在，尝试打开
            printf("服务已存在，尝试打开...\n");
            hService = OpenServiceA(hSCManager, serviceName, SERVICE_ALL_ACCESS);
            if (hService == NULL) {
                printf("OpenService 失败! 错误代码: %lu\n", GetLastError());
                goto cleanup;
            }
        }
        else {
            printf("CreateService 失败! 错误代码: %lu\n", dwError);
            goto cleanup;
        }
    }

    // 3. 启动驱动服务
    if (!StartServiceA(hService, 0, NULL)) {
        DWORD dwError = GetLastError();
        if (dwError != ERROR_SERVICE_ALREADY_RUNNING) {
            printf("StartService 失败! 错误代码: %lu\n", dwError);
            goto cleanup;
        }
        else {
            printf("驱动已在运行中。\n");
        }
    }

    printf("驱动加载成功! 服务名: %s\n", serviceName);
    bRet = TRUE;

cleanup:
    // 清理资源
    if (hService != NULL) {
        CloseServiceHandle(hService);
    }
    if (hSCManager != NULL) {
        CloseServiceHandle(hSCManager);
    }

    return bRet;
}

/**
 * @brief 卸载驱动
 * @param pServiceName 服务名称
 * @return TRUE 成功，FALSE 失败
 */
BOOL UnloadDriver(const char* pServiceName) {
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hService = NULL;
    SERVICE_STATUS serviceStatus;
    BOOL bRet = FALSE;

    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCManager == NULL) {
        printf("OpenSCManager 失败! 错误代码: %lu\n", GetLastError());
        return FALSE;
    }

    hService = OpenServiceA(hSCManager, pServiceName, SERVICE_ALL_ACCESS);
    if (hService == NULL) {
        printf("OpenService 失败! 错误代码: %lu\n", GetLastError());
        goto cleanup;
    }

    // 停止服务
    if (!ControlService(hService, SERVICE_CONTROL_STOP, &serviceStatus)) {
        DWORD dwError = GetLastError();
        if (dwError != ERROR_SERVICE_NOT_ACTIVE) {
            printf("ControlService 失败! 错误代码: %lu\n", dwError);
        }
    }

    // 删除服务
    if (!DeleteService(hService)) {
        printf("DeleteService 失败! 错误代码: %lu\n", GetLastError());
        goto cleanup;
    }

    printf("驱动卸载成功! 服务名: %s\n", pServiceName);
    bRet = TRUE;

cleanup:
    if (hService != NULL) {
        CloseServiceHandle(hService);
    }
    if (hSCManager != NULL) {
        CloseServiceHandle(hSCManager);
    }

    return bRet;
}