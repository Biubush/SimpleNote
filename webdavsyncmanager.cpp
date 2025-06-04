#include "webdavsyncmanager.h"
#include <QDebug>
#include <QApplication>
#include <QStandardPaths>
#include <QBuffer>
#include <QEventLoop>
#include <QFileInfo>
#include <QDirIterator>

WebDAVSyncManager::WebDAVSyncManager(QObject *parent)
    : QObject(parent)
    , m_webdav(new QWebdav(this))
    , m_dirParser(new QWebdavDirParser(this))
    , m_database(nullptr)
    , m_serverUrl()
    , m_username()
    , m_password()
    , m_remoteFolder()
    , m_port(0)
    , m_useSSL(false)
    , m_autoSync(false)
    , m_syncIntervalMinutes(30)
    , m_syncDirection(TwoWay)
    , m_status(NotConfigured)
    , m_lastSyncTime()
    , m_currentProgress(0)
    , m_timeOffset(0)
{
    // 连接信号槽
    connect(m_webdav, &QWebdav::errorChanged, this, &WebDAVSyncManager::onWebDAVError);
    connect(m_dirParser, &QWebdavDirParser::finished, this, &WebDAVSyncManager::onDirectoryListingFinished);
    connect(m_dirParser, &QWebdavDirParser::errorChanged, this, &WebDAVSyncManager::onWebDAVError);
    
    // 连接自动同步定时器
    connect(&m_autoSyncTimer, &QTimer::timeout, this, &WebDAVSyncManager::onAutoSyncTriggered);
    
    // 加载配置
    loadConfig();
}

WebDAVSyncManager::~WebDAVSyncManager()
{
    // 保存配置
    saveConfig();
}

bool WebDAVSyncManager::isConfigured() const
{
    return !m_serverUrl.isEmpty() && !m_username.isEmpty() && !m_password.isEmpty();
}

WebDAVSyncManager::SyncStatus WebDAVSyncManager::status() const
{
    return m_status;
}

void WebDAVSyncManager::setDatabase(NoteDatabase *db)
{
    m_database = db;
}

void WebDAVSyncManager::configure(const QString &serverUrl, const QString &username, 
                                 const QString &password, const QString &remoteFolder,
                                 int port, bool useSSL)
{
    m_serverUrl = serverUrl;
    m_username = username;
    m_password = password;
    m_remoteFolder = remoteFolder.isEmpty() ? "/" : remoteFolder;
    m_port = port;
    m_useSSL = useSSL;
    
    if (!m_remoteFolder.startsWith("/")) {
        m_remoteFolder = "/" + m_remoteFolder;
    }
    
    if (!m_remoteFolder.endsWith("/")) {
        m_remoteFolder += "/";
    }
    
    // 更新WebDAV连接设置
    QWebdav::QWebdavConnectionType connType = useSSL ? QWebdav::HTTPS : QWebdav::HTTP;
    
    // 修正：在setConnectionSettings中只使用服务器URL，不包含远程文件夹路径
    m_webdav->setConnectionSettings(connType, serverUrl, "", username, password, port);
    
    m_status = isConfigured() ? Idle : NotConfigured;
    emit syncStatusChanged(m_status);
    
    // 保存配置
    saveConfig();
}

bool WebDAVSyncManager::testConnection()
{
    if (!isConfigured()) {
        m_lastError = tr("WebDAV服务器未配置");
        return false;
    }
    
    // 尝试列出根目录
    m_status = Syncing;
    emit syncStatusChanged(m_status);
    emit syncProgress(10, tr("测试WebDAV连接..."));
    
    bool success = m_dirParser->listDirectory(m_webdav, "/");
    
    if (!success) {
        m_status = Error;
        m_lastError = tr("无法连接到WebDAV服务器");
        emit syncStatusChanged(m_status);
        emit syncError(m_lastError);
        return false;
    }
    
    // 这里不实际等待结果，因为列出目录是异步操作
    // 在实际应用中，我们可以使用信号槽机制处理结果
    
    return true;
}

void WebDAVSyncManager::setSyncOptions(bool autoSync, int intervalMinutes, SyncDirection direction)
{
    m_autoSync = autoSync;
    m_syncIntervalMinutes = (intervalMinutes < 1) ? 30 : intervalMinutes;
    m_syncDirection = direction;
    
    // 设置自动同步
    enableAutoSync(m_autoSync);
    
    // 保存设置
    saveConfig();
}

void WebDAVSyncManager::setSyncDirection(SyncDirection direction)
{
    m_syncDirection = direction;
    saveConfig();
}

QDateTime WebDAVSyncManager::lastSyncTime() const
{
    return m_lastSyncTime;
}

QString WebDAVSyncManager::lastError() const
{
    return m_lastError;
}

bool WebDAVSyncManager::startSync()
{
    if (!isConfigured()) {
        m_lastError = tr("WebDAV服务器未配置");
        emit syncError(m_lastError);
        return false;
    }
    
    if (m_status == Syncing) {
        m_lastError = tr("同步操作正在进行中");
        emit syncError(m_lastError);
        return false;
    }
    
    if (!m_database) {
        m_lastError = tr("数据库实例未设置");
        emit syncError(m_lastError);
        return false;
    }
    
    // 开始同步过程
    m_status = Syncing;
    m_currentProgress = 0;
    m_remoteFiles.clear();
    m_pendingUploads.clear();
    m_pendingDownloads.clear();
    
    emit syncStatusChanged(m_status);
    emit syncProgress(m_currentProgress, tr("开始同步..."));
    
    // 先获取远程目录结构
    bool result = getRemoteDirectoryStructure();
    
    if (!result) {
        m_status = Error;
        emit syncStatusChanged(m_status);
        emit syncFinished(false);
        return false;
    }
    
    // 实际的同步操作将在onDirectoryListingFinished中完成
    return true;
}

void WebDAVSyncManager::enableAutoSync(bool enable)
{
    m_autoSync = enable;
    
    if (m_autoSync) {
        // 设置自动同步定时器
        int msec = m_syncIntervalMinutes * 60 * 1000;
        m_autoSyncTimer.start(msec);
    } else {
        // 停止自动同步
        m_autoSyncTimer.stop();
    }
    
    saveConfig();
}

void WebDAVSyncManager::setSyncInterval(int minutes)
{
    if (minutes < 1) {
        minutes = 30;
    }
    
    m_syncIntervalMinutes = minutes;
    
    // 如果自动同步已启用，更新定时器
    if (m_autoSync) {
        int msec = m_syncIntervalMinutes * 60 * 1000;
        m_autoSyncTimer.start(msec);
    }
    
    saveConfig();
}

void WebDAVSyncManager::loadConfig()
{
    // 使用单独的配置文件
    QSettings settings(getConfigFilePath(), QSettings::IniFormat);
    
    settings.beginGroup("WebDAV");
    
    m_serverUrl = settings.value("ServerUrl", "").toString();
    m_username = settings.value("Username", "").toString();
    m_password = settings.value("Password", "").toString();
    m_remoteFolder = settings.value("RemoteFolder", "/").toString();
    m_port = settings.value("Port", 0).toInt();
    m_useSSL = settings.value("UseSSL", false).toBool();
    
    m_autoSync = settings.value("AutoSync", false).toBool();
    m_syncIntervalMinutes = settings.value("SyncInterval", 30).toInt();
    m_syncDirection = static_cast<SyncDirection>(settings.value("SyncDirection", TwoWay).toInt());
    m_lastSyncTime = settings.value("LastSyncTime").toDateTime();
    
    settings.endGroup();
    
    // 更新WebDAV连接设置
    if (isConfigured()) {
        QWebdav::QWebdavConnectionType connType = m_useSSL ? QWebdav::HTTPS : QWebdav::HTTP;
        m_webdav->setConnectionSettings(connType, m_serverUrl, "", m_username, m_password, m_port);
        m_status = Idle;
    } else {
        m_status = NotConfigured;
    }
    
    // 如果自动同步已启用，启动定时器
    enableAutoSync(m_autoSync);
}

void WebDAVSyncManager::saveConfig()
{
    // 使用单独的配置文件
    QSettings settings(getConfigFilePath(), QSettings::IniFormat);
    
    settings.beginGroup("WebDAV");
    
    settings.setValue("ServerUrl", m_serverUrl);
    settings.setValue("Username", m_username);
    settings.setValue("Password", m_password);
    settings.setValue("RemoteFolder", m_remoteFolder);
    settings.setValue("Port", m_port);
    settings.setValue("UseSSL", m_useSSL);
    
    settings.setValue("AutoSync", m_autoSync);
    settings.setValue("SyncInterval", m_syncIntervalMinutes);
    settings.setValue("SyncDirection", m_syncDirection);
    settings.setValue("LastSyncTime", m_lastSyncTime);
    
    settings.endGroup();
    
    // 确保设置立即保存
    settings.sync();
}

QString WebDAVSyncManager::getConfigFilePath()
{
    // 获取应用程序数据目录
    QString appDataDir = NoteDatabase::getDatabaseDir();
    
    // 配置文件存储在数据目录下，但不在images目录中，避免被同步
    return appDataDir + "/webdav_config.ini";
}

bool WebDAVSyncManager::backupConfig(const QString &backupDir)
{
    QString configPath = getConfigFilePath();
    QFile configFile(configPath);
    
    // 如果配置文件不存在，说明没有配置过，不需要备份
    if (!configFile.exists()) {
        return true;
    }
    
    // 确保备份目录存在
    QDir dir;
    if (!dir.exists(backupDir)) {
        if (!dir.mkpath(backupDir)) {
            return false;
        }
    }
    
    // 执行备份
    QString backupPath = backupDir + "/webdav_config.ini";
    return configFile.copy(backupPath);
}

bool WebDAVSyncManager::restoreConfig(const QString &backupDir)
{
    QString configPath = getConfigFilePath();
    QString backupPath = backupDir + "/webdav_config.ini";
    QFile backupFile(backupPath);
    
    // 如果备份文件不存在，不需要还原
    if (!backupFile.exists()) {
        return true;
    }
    
    // 如果当前有配置文件，先删除
    QFile configFile(configPath);
    if (configFile.exists() && !configFile.remove()) {
        return false;
    }
    
    // 执行还原
    return backupFile.copy(configPath);
}

void WebDAVSyncManager::onWebDAVError(const QString &error)
{
    m_lastError = error;
    m_status = Error;
    
    emit syncStatusChanged(m_status);
    emit syncError(m_lastError);
    emit syncFinished(false);
}

void WebDAVSyncManager::onDirectoryListingFinished()
{
    // 目录列表获取完成，执行后续同步操作
    if (m_status != Syncing) {
        return; // 可能是测试连接响应，忽略
    }
    
    m_currentProgress = 20;
    emit syncProgress(m_currentProgress, tr("目录结构获取完成，开始同步..."));
    
    // 更新远程文件列表
    QList<QWebdavItem> items = m_dirParser->getList();
    foreach (const QWebdavItem &item, items) {
        if (!item.isDir()) {
            m_remoteFiles.insert(item.path(), item.lastModified());
        }
    }
    
    // 计算时间戳差异
    m_timeOffset = calculateTimeDiff();
    
    // 执行同步
    bool success = performSync(m_syncDirection);
    
    // 如果没有任何文件需要传输，此时同步已完成
    if (success && m_pendingUploads.isEmpty() && m_pendingDownloads.isEmpty()) {
        m_status = Idle;
        m_lastSyncTime = QDateTime::currentDateTime();
        emit syncStatusChanged(m_status);
        emit syncProgress(100, tr("同步完成"));
        emit syncFinished(true);
    } else if (!success) {
        // 同步出错
        m_status = Error;
        emit syncStatusChanged(m_status);
        emit syncFinished(false);
    }
    // 如果有待传输文件，则在传输完成后发送信号
}

void WebDAVSyncManager::onFileUploadFinished()
{
    // 如果有更多文件待上传，继续上传
    if (!m_pendingUploads.isEmpty()) {
        QString nextFile = m_pendingUploads.takeFirst();
        uploadFile(nextFile);
    } else if (!m_pendingDownloads.isEmpty()) {
        // 上传完成，开始下载
        m_currentProgress = 70;
        emit syncProgress(m_currentProgress, tr("文件上传完成，开始下载..."));
        
        QString firstFile = m_pendingDownloads.takeFirst();
        downloadFile(firstFile);
    } else {
        // 所有文件传输完成
        m_currentProgress = 90;
        emit syncProgress(m_currentProgress, tr("文件传输完成"));
        
        // 更新状态
        m_status = Idle;
        m_lastSyncTime = QDateTime::currentDateTime();
        emit syncStatusChanged(m_status);
        emit syncProgress(100, tr("同步完成"));
        emit syncFinished(true);
    }
}

void WebDAVSyncManager::onFileDownloadFinished()
{
    // 如果有更多文件待下载，继续下载
    if (!m_pendingDownloads.isEmpty()) {
        QString nextFile = m_pendingDownloads.takeFirst();
        downloadFile(nextFile);
    } else {
        // 所有文件下载完成
        m_currentProgress = 90;
        emit syncProgress(m_currentProgress, tr("文件下载完成"));
        
        // 更新状态
        m_status = Idle;
        m_lastSyncTime = QDateTime::currentDateTime();
        emit syncStatusChanged(m_status);
        emit syncProgress(100, tr("同步完成"));
        emit syncFinished(true);
    }
}

void WebDAVSyncManager::onAutoSyncTriggered()
{
    // 自动同步触发，启动同步操作
    qDebug() << "自动同步触发";
    startSync();
}

bool WebDAVSyncManager::syncDatabase()
{
    if (!m_database) {
        m_lastError = tr("数据库实例未设置");
        return false;
    }
    
    // 获取本地数据库路径
    QString dbPath = NoteDatabase::getDatabasePath();
    QFileInfo dbInfo(dbPath);
    
    if (!dbInfo.exists()) {
        m_lastError = tr("本地数据库文件不存在");
        return false;
    }
    
    // 检查远程数据库是否存在 - 在远程文件夹下创建
    QString remoteDbName = m_remoteFolder + "notes.db";
    bool remoteExists = m_remoteFiles.contains(remoteDbName);
    
    if (m_syncDirection == TwoWay || m_syncDirection == LocalToRemote) {
        // 上传数据库
        QDateTime localTime = dbInfo.lastModified();
        
        if (!remoteExists || shouldUploadFile(dbPath, remoteExists ? m_remoteFiles[remoteDbName] : QDateTime())) {
            // 上传数据库前先关闭数据库连接
            bool wasOpen = m_database->isOpen();
            if (wasOpen) {
                m_database->close();
            }
            
            // 上传数据库
            QFile dbFile(dbPath);
            if (dbFile.open(QIODevice::ReadOnly)) {
                QByteArray data = dbFile.readAll();
                dbFile.close();
                
                QNetworkReply *reply = m_webdav->put(remoteDbName, data);
                
                // 等待上传完成
                QEventLoop loop;
                connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
                loop.exec();
                
                if (reply->error() != QNetworkReply::NoError) {
                    m_lastError = tr("数据库上传失败: ") + reply->errorString();
                    reply->deleteLater();
                    
                    // 重新打开数据库
                    if (wasOpen) {
                        m_database->open();
                    }
                    return false;
                }
                
                reply->deleteLater();
            } else {
                m_lastError = tr("无法打开本地数据库文件");
                
                // 重新打开数据库
                if (wasOpen) {
                    m_database->open();
                }
                return false;
            }
            
            // 重新打开数据库
            if (wasOpen) {
                m_database->open();
            }
        }
    } else if (m_syncDirection == RemoteToLocal && remoteExists) {
        // 下载远程数据库
        QNetworkReply *reply = m_webdav->get(remoteDbName);
        
        // 等待下载完成
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        
        if (reply->error() != QNetworkReply::NoError) {
            m_lastError = tr("数据库下载失败: ") + reply->errorString();
            reply->deleteLater();
            return false;
        }
        
        QByteArray data = reply->readAll();
        reply->deleteLater();
        
        // 关闭数据库连接
        bool wasOpen = m_database->isOpen();
        if (wasOpen) {
            m_database->close();
        }
        
        // 保存下载的数据库
        QFile dbFile(dbPath);
        if (dbFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            dbFile.write(data);
            dbFile.close();
        } else {
            m_lastError = tr("无法写入本地数据库文件");
            
            // 重新打开数据库
            if (wasOpen) {
                m_database->open();
            }
            return false;
        }
        
        // 重新打开数据库
        if (wasOpen) {
            m_database->open();
        }
    }
    
    return true;
}

bool WebDAVSyncManager::syncImages()
{
    // 获取本地图片目录
    QString imagesDir = NoteDatabase::getDatabaseDir() + "/images";
    QDir dir(imagesDir);
    
    // 确保远程images目录存在
    createRemoteDirectory(m_remoteFolder + "images/");
    
    // 获取本地图片文件列表
    QMap<QString, QDateTime> localFiles;
    
    if (dir.exists()) {
        // 遍历图片目录下的所有子目录
        QDirIterator it(imagesDir, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString noteDir = it.next();
            QDir noteImgDir(noteDir);
            QString noteDirName = noteImgDir.dirName();
            
            // 确保远程笔记图片目录存在
            createRemoteDirectory(m_remoteFolder + "images/" + noteDirName + "/");
            
            // 获取该目录下的所有图片
            QStringList imgFiles = noteImgDir.entryList(QDir::Files);
            foreach (const QString &imgFile, imgFiles) {
                QString localPath = noteImgDir.absoluteFilePath(imgFile);
                QString relativePath = m_remoteFolder + "images/" + noteDirName + "/" + imgFile;
                
                QFileInfo fileInfo(localPath);
                localFiles.insert(relativePath, fileInfo.lastModified());
                
                // 同步图片
                if (m_syncDirection == TwoWay || m_syncDirection == LocalToRemote) {
                    // 上传图片
                    if (!m_remoteFiles.contains(relativePath) || 
                        shouldUploadFile(localPath, m_remoteFiles[relativePath])) {
                        m_pendingUploads.append(relativePath);
                    }
                }
            }
        }
    }
    
    // 处理远程图片下载
    if (m_syncDirection == TwoWay || m_syncDirection == RemoteToLocal) {
        foreach (const QString &remotePath, m_remoteFiles.keys()) {
            if (remotePath.startsWith(m_remoteFolder + "images/")) {
                QString localPath = localFilePath(remotePath);
                
                if (!QFile::exists(localPath) || 
                    shouldDownloadFile(remotePath, localFiles.value(remotePath))) {
                    // 确保本地目录存在
                    QFileInfo fileInfo(localPath);
                    QDir().mkpath(fileInfo.absolutePath());
                    
                    m_pendingDownloads.append(remotePath);
                }
            }
        }
    }
    
    // 处理待上传和待下载队列
    if (!m_pendingUploads.isEmpty()) {
        QString firstFile = m_pendingUploads.takeFirst();
        uploadFile(firstFile);
    } else if (!m_pendingDownloads.isEmpty()) {
        QString firstFile = m_pendingDownloads.takeFirst();
        downloadFile(firstFile);
    } else {
        // 没有文件需要传输，同步完成
        return true;
    }
    
    return true;
}

bool WebDAVSyncManager::getRemoteDirectoryStructure()
{
    // 获取根目录列表 - 使用远程文件夹路径
    bool result = m_dirParser->listDirectory(m_webdav, m_remoteFolder, true);
    
    if (!result) {
        m_lastError = tr("获取远程目录结构失败");
        emit syncError(m_lastError);
        return false;
    }
    
    return true;
}

void WebDAVSyncManager::createRemoteDirectory(const QString &path)
{
    // 确保路径以/开头
    QString remotePath = path;
    if (!remotePath.startsWith("/")) {
        remotePath = "/" + remotePath;
    }
    
    // 检查目录是否已存在
    foreach (const QWebdavItem &item, m_dirParser->getList()) {
        if (item.isDir() && item.path() == remotePath) {
            return;  // 目录已存在，无需创建
        }
    }
    
    // 创建远程目录
    QNetworkReply *reply = m_webdav->mkdir(remotePath);
    
    // 等待操作完成
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    reply->deleteLater();
}

bool WebDAVSyncManager::performSync(SyncDirection direction)
{
    m_currentProgress = 30;
    emit syncProgress(m_currentProgress, tr("开始数据库同步..."));
    
    // 同步数据库
    bool dbResult = syncDatabase();
    
    if (!dbResult) {
        return false;
    }
    
    m_currentProgress = 50;
    emit syncProgress(m_currentProgress, tr("开始图片同步..."));
    
    // 同步图片
    bool imagesResult = syncImages();
    
    return imagesResult;
}

qint64 WebDAVSyncManager::calculateTimeDiff()
{
    // 计算本地时间和服务器时间的差异
    // 简化实现，实际上可能需要更复杂的处理
    return 0;
}

QString WebDAVSyncManager::localFilePath(const QString &remotePath)
{
    // 从远程路径去掉远程文件夹前缀和前导斜杠
    QString localRelativePath = remotePath;
    
    // 先去掉远程文件夹前缀
    if (localRelativePath.startsWith(m_remoteFolder)) {
        localRelativePath = localRelativePath.mid(m_remoteFolder.length());
    }
    
    // 再去掉可能存在的前导斜杠
    if (localRelativePath.startsWith("/")) {
        localRelativePath = localRelativePath.mid(1);
    }
    
    return NoteDatabase::getDatabaseDir() + "/" + localRelativePath;
}

QString WebDAVSyncManager::remoteFilePath(const QString &localPath)
{
    QString dbDir = NoteDatabase::getDatabaseDir();
    if (localPath.startsWith(dbDir)) {
        return localPath.mid(dbDir.length() + 1);
    }
    return localPath;
}

bool WebDAVSyncManager::shouldUploadFile(const QString &localPath, const QDateTime &remoteModified)
{
    if (remoteModified.isNull()) {
        return true;  // 远程文件不存在
    }
    
    QFileInfo info(localPath);
    QDateTime localModified = info.lastModified();
    
    // 考虑时间差异
    QDateTime adjustedRemoteTime = remoteModified.addSecs(m_timeOffset);
    
    // 如果本地文件更新，则上传
    return localModified > adjustedRemoteTime;
}

bool WebDAVSyncManager::shouldDownloadFile(const QString &remotePath, const QDateTime &localModified)
{
    if (localModified.isNull()) {
        return true;  // 本地文件不存在
    }
    
    QDateTime remoteModified = m_remoteFiles[remotePath];
    
    // 考虑时间差异
    QDateTime adjustedRemoteTime = remoteModified.addSecs(m_timeOffset);
    
    // 如果远程文件更新，则下载
    return adjustedRemoteTime > localModified;
}

void WebDAVSyncManager::uploadFile(const QString &relativePath)
{
    // 确保路径以/开头
    QString remotePath = relativePath;
    if (!remotePath.startsWith("/")) {
        remotePath = "/" + remotePath;
    }
    
    // 检查是否是WebDAV配置文件，如果是则跳过
    QString localPath = localFilePath(remotePath);
    if (localPath == getConfigFilePath()) {
        qDebug() << "跳过上传WebDAV配置文件:" << localPath;
        onFileUploadFinished();
        return;
    }
    
    QFile file(localPath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开文件进行上传:" << localPath;
        // 跳过这个文件
        onFileUploadFinished();
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QNetworkReply *reply = m_webdav->put(remotePath, data);
    
    // 添加正在上传的文件和响应的映射
    connect(reply, &QNetworkReply::finished, this, &WebDAVSyncManager::onFileUploadFinished);
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), 
            this, SLOT(onFileUploadError(QNetworkReply::NetworkError)));
    
    int progress = 60 + (90 - 60) * (m_pendingUploads.size() / (m_pendingUploads.size() + 1.0));
    emit syncProgress(progress, tr("上传文件: %1").arg(remotePath));
}

void WebDAVSyncManager::onFileUploadError(QNetworkReply::NetworkError error)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply) {
        m_lastError = tr("文件上传错误: ") + reply->errorString();
        qDebug() << m_lastError;
        // 继续下一个文件
        onFileUploadFinished();
    }
}

void WebDAVSyncManager::downloadFile(const QString &remotePath)
{
    QString localPath = localFilePath(remotePath);
    
    // 确保目录存在
    QFileInfo fileInfo(localPath);
    QDir().mkpath(fileInfo.absolutePath());
    
    // 确保远程路径以/开头
    QString remoteFilePath = remotePath;
    if (!remoteFilePath.startsWith("/")) {
        remoteFilePath = "/" + remoteFilePath;
    }
    
    QNetworkReply *reply = m_webdav->get(remoteFilePath);
    
    // 添加正在下载的文件和响应的映射
    connect(reply, &QNetworkReply::finished, [this, reply, localPath]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            
            QFile file(localPath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                file.write(data);
                file.close();
            } else {
                qDebug() << "无法写入下载的文件:" << localPath;
            }
        }
        
        reply->deleteLater();
        onFileDownloadFinished();
    });
    
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), 
            this, SLOT(onFileDownloadError(QNetworkReply::NetworkError)));
    
    int progress = 60 + (90 - 60) * (m_pendingDownloads.size() / (m_pendingDownloads.size() + 1.0));
    emit syncProgress(progress, tr("下载文件: %1").arg(remoteFilePath));
}

void WebDAVSyncManager::onFileDownloadError(QNetworkReply::NetworkError error)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply) {
        m_lastError = tr("文件下载错误: ") + reply->errorString();
        qDebug() << m_lastError;
        reply->deleteLater();
        // 继续下一个文件
        onFileDownloadFinished();
    }
} 