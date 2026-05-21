#pragma once

#include <QString>
#include <QMap>
#include <QMutex>

// NT设备路径 → DOS路径转换器
// 将 \Device\HarddiskVolume1\xxx 格式转换为 C:\xxx 格式
class PathConverter
{
public:
    static PathConverter& instance();

    // 将NT设备路径转换为DOS路径，如转换失败则原样返回
    QString ntPathToDosPath(const QString& ntPath);

    // 将DOS路径转换为NT设备路径，如转换失败则原样返回
    QString dosPathToNtPath(const QString& dosPath);

    // 刷新设备映射表（可定期调用以识别新挂载的卷）
    void refreshMapping();

private:
    PathConverter();
    ~PathConverter() = default;
    PathConverter(const PathConverter&) = delete;
    PathConverter& operator=(const PathConverter&) = delete;

    void buildMapping();

    QMap<QString, QString> m_ntToDosMap;   // NT设备名 → DOS盘符  如 "\Device\HarddiskVolume1" → "C:"
    QMutex m_mutex;
};
