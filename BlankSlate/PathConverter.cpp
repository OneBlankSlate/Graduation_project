#include "PathConverter.h"
#include <windows.h>

PathConverter& PathConverter::instance()
{
    static PathConverter inst;
    return inst;
}

PathConverter::PathConverter()
{
    buildMapping();
}

void PathConverter::buildMapping()
{
    QMutexLocker locker(&m_mutex);
    m_ntToDosMap.clear();

    // 获取所有逻辑驱动器盘符
    WCHAR driveStrings[256] = { 0 };
    DWORD len = GetLogicalDriveStringsW(256, driveStrings);
    if (len == 0) return;

    // 遍历每个盘符，查询其NT设备名
    WCHAR* current = driveStrings;
    while (*current)
    {
        // 盘符格式如 "C:\0"，取前两个字符 "C:"
        QString dosDrive = QString::fromWCharArray(current, 2);

        // 查询该盘符对应的NT设备名
        WCHAR deviceName[MAX_PATH] = { 0 };
        if (QueryDosDeviceW(dosDrive.toStdWString().c_str(), deviceName, MAX_PATH))
        {
            QString ntDevice = QString::fromWCharArray(deviceName);
            m_ntToDosMap[ntDevice] = dosDrive;
        }

        // 移动到下一个盘符字符串
        current += wcslen(current) + 1;
    }
}

void PathConverter::refreshMapping()
{
    buildMapping();
}

QString PathConverter::ntPathToDosPath(const QString& ntPath)
{
    QMutexLocker locker(&m_mutex);

    // NT路径格式: \Device\HarddiskVolume1\Windows\System32\ntdll.dll
    // 需要匹配最长前缀
    QString bestMatch;
    int bestLen = 0;

    for (auto it = m_ntToDosMap.constBegin(); it != m_ntToDosMap.constEnd(); ++it)
    {
        if (ntPath.startsWith(it.key(), Qt::CaseInsensitive) && it.key().length() > bestLen)
        {
            bestMatch = it.key();
            bestLen = it.key().length();
        }
    }

    if (!bestMatch.isEmpty())
    {
        // 替换NT设备名为DOS盘符
        // \Device\HarddiskVolume1\Windows\... → C:\Windows\...
        return m_ntToDosMap[bestMatch] + ntPath.mid(bestLen);
    }

    // 无法转换，原样返回
    return ntPath;
}

QString PathConverter::dosPathToNtPath(const QString& dosPath)
{
    QMutexLocker locker(&m_mutex);

    // DOS路径格式: C:\Windows\System32\ntdll.dll
    // 需要匹配最长前缀
    QString bestMatch;
    int bestLen = 0;

    for (auto it = m_ntToDosMap.constBegin(); it != m_ntToDosMap.constEnd(); ++it)
    {
        if (dosPath.startsWith(it.value(), Qt::CaseInsensitive) && it.value().length() > bestLen)
        {
            bestMatch = it.key();
            bestLen = it.value().length();
        }
    }

    if (!bestMatch.isEmpty())
    {
        // 替换DOS盘符为NT设备名
        // C:\Windows\... → \Device\HarddiskVolume1\Windows\...
        return bestMatch + dosPath.mid(bestLen);
    }

    // 无法转换，原样返回
    return dosPath;
}
