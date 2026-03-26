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
        DWORD dwError = GetLastError();
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
            hService = OpenServiceA(hSCManager, serviceName, SERVICE_ALL_ACCESS);
            if (hService == NULL) {
                goto cleanup;
            }
        }
        else {
            goto cleanup;
        }
    }

    // 3. 启动驱动服务
    if (!StartServiceA(hService, 0, NULL)) {
        DWORD dwError = GetLastError();
        if (dwError != ERROR_SERVICE_ALREADY_RUNNING) {
            goto cleanup;
        }
        else {
        }
    }

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
        return FALSE;
    }

    hService = OpenServiceA(hSCManager, pServiceName, SERVICE_ALL_ACCESS);
    if (hService == NULL) {
        goto cleanup;
    }

    // 停止服务
    if (!ControlService(hService, SERVICE_CONTROL_STOP, &serviceStatus)) {
        DWORD dwError = GetLastError();
    }

    // 删除服务
    if (!DeleteService(hService)) {
        DWORD dwError = GetLastError();
        goto cleanup;
    }
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