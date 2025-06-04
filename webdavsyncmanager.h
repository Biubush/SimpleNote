#ifndef WEBDAVSYNCMANAGER_H
#define WEBDAVSYNCMANAGER_H

#include <QObject>
#include <QTimer>
#include <QSettings>
#include <QDateTime>
#include <QMap>
#include <QFile>
#include <QDir>

#include "qwebdav.h"
#include "qwebdavdirparser.h"
#include "notedatabase.h"

class WebDAVSyncManager : public QObject
{
    Q_OBJECT

public:
    enum SyncDirection {
        LocalToRemote, // 本地到远程
        RemoteToLocal, // 远程到本地
        TwoWay         // 双向同步
    };

    enum SyncStatus {
        NotConfigured, // 未配置
        Idle,          // 空闲
        Syncing,       // 正在同步
        Error          // 同步错误
    };

    explicit WebDAVSyncManager(QObject *parent = nullptr);
    ~WebDAVSyncManager();

    // 获取当前配置
    bool isConfigured() const;
    SyncStatus status() const;
    
    // 设置数据库和图片路径
    void setDatabase(NoteDatabase *db);
    
    // 配置连接
    void configure(const QString &serverUrl, const QString &username, 
                  const QString &password, const QString &remoteFolder,
                  int port = 0, bool useSSL = false);
                  
    // 测试连接
    bool testConnection();
    
    // 设置同步选项
    void setSyncOptions(bool autoSync, int intervalMinutes, SyncDirection direction);
    void setSyncDirection(SyncDirection direction);
    
    // 获取上次同步时间
    QDateTime lastSyncTime() const;
    
    // 获取错误信息
    QString lastError() const;
    
    // 获取配置信息
    QString serverUrl() const { return m_serverUrl; }
    int port() const { return m_port; }
    bool useSSL() const { return m_useSSL; }
    QString remoteFolder() const { return m_remoteFolder; }
    QString username() const { return m_username; }
    QString password() const { return m_password; }
    bool autoSync() const { return m_autoSync; }
    int syncInterval() const { return m_syncIntervalMinutes; }
    SyncDirection syncDirection() const { return m_syncDirection; }
    
    // 获取配置文件路径
    static QString getConfigFilePath();
    
    // 备份与恢复配置
    static bool backupConfig(const QString &backupDir);
    static bool restoreConfig(const QString &backupDir);

public slots:
    // 开始同步
    bool startSync();
    
    // 启用/禁用自动同步
    void enableAutoSync(bool enable);
    
    // 设置自动同步间隔（分钟）
    void setSyncInterval(int minutes);
    
    // 加载和保存配置
    void loadConfig();
    void saveConfig();

signals:
    // 同步状态变化信号
    void syncStatusChanged(SyncStatus status);
    void syncProgress(int percent, const QString &message);
    void syncFinished(bool success);
    void syncError(const QString &error);

private slots:
    // 内部处理WebDAV响应的槽
    void onWebDAVError(const QString &error);
    void onDirectoryListingFinished();
    void onFileUploadFinished();
    void onFileDownloadFinished();
    void onAutoSyncTriggered();
    void onFileUploadError(QNetworkReply::NetworkError error);
    void onFileDownloadError(QNetworkReply::NetworkError error);

private:
    // 执行数据库同步
    bool syncDatabase();
    
    // 执行图片同步
    bool syncImages();
    
    // 获取远程目录结构
    bool getRemoteDirectoryStructure();
    
    // 创建远程目录
    void createRemoteDirectory(const QString &path);
    
    // 执行特定方向的同步
    bool performSync(SyncDirection direction);
    
    // 计算本地和远程文件的时间戳差异
    qint64 calculateTimeDiff();
    
    // 辅助函数
    QString localFilePath(const QString &remotePath);
    QString remoteFilePath(const QString &localPath);
    bool shouldUploadFile(const QString &localPath, const QDateTime &remoteModified);
    bool shouldDownloadFile(const QString &remotePath, const QDateTime &localModified);
    
    // 文件上传和下载
    void uploadFile(const QString &relativePath);
    void downloadFile(const QString &remotePath);

private:
    QWebdav *m_webdav;
    QWebdavDirParser *m_dirParser;
    NoteDatabase *m_database;
    
    QString m_serverUrl;
    QString m_username;
    QString m_password;
    QString m_remoteFolder;
    int m_port;
    bool m_useSSL;
    
    bool m_autoSync;
    int m_syncIntervalMinutes;
    QTimer m_autoSyncTimer;
    SyncDirection m_syncDirection;
    SyncStatus m_status;
    
    QDateTime m_lastSyncTime;
    QString m_lastError;
    
    // 同步状态信息
    int m_currentProgress;
    QMap<QString, QDateTime> m_remoteFiles;
    QStringList m_pendingUploads;
    QStringList m_pendingDownloads;
    
    // 本地和远程时间戳差异（用于比较修改时间）
    qint64 m_timeOffset;
};

#endif // WEBDAVSYNCMANAGER_H 