#include "webdavconfigdialog.h"
#include "ui_webdavconfigdialog.h"

WebDAVConfigDialog::WebDAVConfigDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WebDAVConfigDialog),
    m_syncManager(nullptr)
{
    ui->setupUi(this);
    
    // 连接信号槽
    connect(ui->testConnectionButton, &QPushButton::clicked, this, &WebDAVConfigDialog::onTestConnection);
    connect(ui->syncNowButton, &QPushButton::clicked, this, &WebDAVConfigDialog::onSyncNowClicked);
    
    // 同步方向选择
    connect(ui->twoWayRadio, &QRadioButton::toggled, this, &WebDAVConfigDialog::updateSyncDirectionFromUI);
    connect(ui->uploadRadio, &QRadioButton::toggled, this, &WebDAVConfigDialog::updateSyncDirectionFromUI);
    connect(ui->downloadRadio, &QRadioButton::toggled, this, &WebDAVConfigDialog::updateSyncDirectionFromUI);
    
    // 初始化UI
    ui->syncProgressBar->setValue(0);
    ui->syncProgressBar->setFormat("%p% %v");
    ui->connectionStatusLabel->setText(tr("未配置"));
    ui->lastSyncLabel->setText(tr("上次同步: 从未同步"));
    ui->syncStatusLabel->setText(tr("同步状态: 未配置"));
    
    // 设置合理的默认值
    ui->serverUrlEdit->setText("");
    ui->portSpinBox->setValue(0);
    ui->sslCheckBox->setChecked(true);
    ui->remoteFolderEdit->setText("/");
    ui->usernameEdit->setText("");
    ui->passwordEdit->setText("");
    ui->autoSyncCheckBox->setChecked(false);
    ui->syncIntervalSpinBox->setValue(30);
    ui->twoWayRadio->setChecked(true);
    
    // 同步按钮初始禁用
    ui->syncNowButton->setEnabled(false);
    
    setWindowTitle(tr("WebDAV云同步配置"));
}

WebDAVConfigDialog::~WebDAVConfigDialog()
{
    delete ui;
}

void WebDAVConfigDialog::setSyncManager(WebDAVSyncManager *syncManager)
{
    if (!syncManager)
        return;
        
    // 解除旧的连接
    if (m_syncManager) {
        disconnect(m_syncManager, &WebDAVSyncManager::syncStatusChanged, this, &WebDAVConfigDialog::onSyncStatusChanged);
        disconnect(m_syncManager, &WebDAVSyncManager::syncProgress, this, &WebDAVConfigDialog::onSyncProgress);
        disconnect(m_syncManager, &WebDAVSyncManager::syncFinished, this, &WebDAVConfigDialog::onSyncFinished);
        disconnect(m_syncManager, &WebDAVSyncManager::syncError, this, &WebDAVConfigDialog::onSyncError);
    }
    
    m_syncManager = syncManager;
    
    // 连接新的信号槽
    connect(m_syncManager, &WebDAVSyncManager::syncStatusChanged, this, &WebDAVConfigDialog::onSyncStatusChanged);
    connect(m_syncManager, &WebDAVSyncManager::syncProgress, this, &WebDAVConfigDialog::onSyncProgress);
    connect(m_syncManager, &WebDAVSyncManager::syncFinished, this, &WebDAVConfigDialog::onSyncFinished);
    connect(m_syncManager, &WebDAVSyncManager::syncError, this, &WebDAVConfigDialog::onSyncError);
    
    // 更新UI
    updateUIFromSettings();
}

void WebDAVConfigDialog::accept()
{
    if (!m_syncManager)
        return QDialog::accept();
    
    // 保存配置
    QString serverUrl = ui->serverUrlEdit->text().trimmed();
    QString username = ui->usernameEdit->text().trimmed();
    QString password = ui->passwordEdit->text();
    QString remoteFolder = ui->remoteFolderEdit->text().trimmed();
    int port = ui->portSpinBox->value();
    bool useSSL = ui->sslCheckBox->isChecked();
    
    // 基本验证
    if (serverUrl.isEmpty()) {
        QMessageBox::warning(this, tr("配置错误"), tr("请输入服务器地址。"));
        ui->serverUrlEdit->setFocus();
        return;
    }
    
    if (username.isEmpty()) {
        QMessageBox::warning(this, tr("配置错误"), tr("请输入用户名。"));
        ui->usernameEdit->setFocus();
        return;
    }
    
    if (password.isEmpty()) {
        QMessageBox::warning(this, tr("配置错误"), tr("请输入密码。"));
        ui->passwordEdit->setFocus();
        return;
    }
    
    // 配置同步管理器
    m_syncManager->configure(serverUrl, username, password, remoteFolder, port, useSSL);
    
    // 配置同步选项
    bool autoSync = ui->autoSyncCheckBox->isChecked();
    int syncInterval = ui->syncIntervalSpinBox->value();
    WebDAVSyncManager::SyncDirection direction = WebDAVSyncManager::TwoWay;
    
    if (ui->uploadRadio->isChecked()) {
        direction = WebDAVSyncManager::LocalToRemote;
    } else if (ui->downloadRadio->isChecked()) {
        direction = WebDAVSyncManager::RemoteToLocal;
    }
    
    m_syncManager->setSyncOptions(autoSync, syncInterval, direction);
    
    QDialog::accept();
}

void WebDAVConfigDialog::onTestConnection()
{
    if (!m_syncManager)
        return;
    
    // 保存当前配置到临时同步管理器
    QString serverUrl = ui->serverUrlEdit->text().trimmed();
    QString username = ui->usernameEdit->text().trimmed();
    QString password = ui->passwordEdit->text();
    QString remoteFolder = ui->remoteFolderEdit->text().trimmed();
    int port = ui->portSpinBox->value();
    bool useSSL = ui->sslCheckBox->isChecked();
    
    // 基本验证
    if (serverUrl.isEmpty()) {
        QMessageBox::warning(this, tr("配置错误"), tr("请输入服务器地址。"));
        ui->serverUrlEdit->setFocus();
        return;
    }
    
    if (username.isEmpty()) {
        QMessageBox::warning(this, tr("配置错误"), tr("请输入用户名。"));
        ui->usernameEdit->setFocus();
        return;
    }
    
    if (password.isEmpty()) {
        QMessageBox::warning(this, tr("配置错误"), tr("请输入密码。"));
        ui->passwordEdit->setFocus();
        return;
    }
    
    // 禁用UI
    setUIEnabled(false);
    
    // 更新状态
    ui->connectionStatusLabel->setText(tr("正在测试连接..."));
    
    // 配置同步管理器
    m_syncManager->configure(serverUrl, username, password, remoteFolder, port, useSSL);
    
    // 测试连接
    bool testStarted = m_syncManager->testConnection();
    
    if (!testStarted) {
        ui->connectionStatusLabel->setText(tr("连接测试失败: %1").arg(m_syncManager->lastError()));
        setUIEnabled(true);
    }
}

void WebDAVConfigDialog::onSyncNowClicked()
{
    if (!m_syncManager || !m_syncManager->isConfigured())
        return;
    
    // 禁用UI
    setUIEnabled(false);
    
    // 启动同步
    bool syncStarted = m_syncManager->startSync();
    
    if (!syncStarted) {
        ui->syncStatusLabel->setText(tr("同步启动失败: %1").arg(m_syncManager->lastError()));
        setUIEnabled(true);
    }
}

void WebDAVConfigDialog::updateSyncDirectionFromUI()
{
    if (!m_syncManager)
        return;
    
    WebDAVSyncManager::SyncDirection direction = WebDAVSyncManager::TwoWay;
    
    if (ui->uploadRadio->isChecked()) {
        direction = WebDAVSyncManager::LocalToRemote;
    } else if (ui->downloadRadio->isChecked()) {
        direction = WebDAVSyncManager::RemoteToLocal;
    }
    
    m_syncManager->setSyncDirection(direction);
}

void WebDAVConfigDialog::updateUIFromSettings()
{
    if (!m_syncManager)
        return;
    
    // 更新服务器设置
    ui->serverUrlEdit->setText(m_syncManager->serverUrl());
    ui->portSpinBox->setValue(m_syncManager->port());
    ui->sslCheckBox->setChecked(m_syncManager->useSSL());
    ui->remoteFolderEdit->setText(m_syncManager->remoteFolder());
    ui->usernameEdit->setText(m_syncManager->username());
    ui->passwordEdit->setText(m_syncManager->password());
    
    // 更新同步选项
    ui->autoSyncCheckBox->setChecked(m_syncManager->autoSync());
    ui->syncIntervalSpinBox->setValue(m_syncManager->syncInterval());
    
    // 更新同步方向
    WebDAVSyncManager::SyncDirection direction = m_syncManager->syncDirection();
    ui->twoWayRadio->setChecked(direction == WebDAVSyncManager::TwoWay);
    ui->uploadRadio->setChecked(direction == WebDAVSyncManager::LocalToRemote);
    ui->downloadRadio->setChecked(direction == WebDAVSyncManager::RemoteToLocal);
    
    // 更新状态信息
    updateSyncStatusUI();
    
    // 启用/禁用同步按钮
    ui->syncNowButton->setEnabled(m_syncManager->isConfigured());
}

void WebDAVConfigDialog::onSyncStatusChanged(WebDAVSyncManager::SyncStatus status)
{
    updateSyncStatusUI();
}

void WebDAVConfigDialog::onSyncProgress(int percent, const QString &message)
{
    ui->syncProgressBar->setValue(percent);
    ui->syncStatusLabel->setText(message);
}

void WebDAVConfigDialog::onSyncFinished(bool success)
{
    // 更新状态
    updateSyncStatusUI();
    
    // 启用UI
    setUIEnabled(true);
    
    // 只在同步成功且当前对话框为活动窗口时显示消息框
    if (success && isActiveWindow()) {
        QMessageBox::information(this, tr("同步完成"), tr("同步操作已成功完成。"));
    }
}

void WebDAVConfigDialog::onSyncError(const QString &error)
{
    ui->syncStatusLabel->setText(tr("同步错误: %1").arg(error));
    setUIEnabled(true);
}

void WebDAVConfigDialog::setUIEnabled(bool enabled)
{
    // 禁用/启用UI元素
    ui->serverUrlEdit->setEnabled(enabled);
    ui->portSpinBox->setEnabled(enabled);
    ui->sslCheckBox->setEnabled(enabled);
    ui->remoteFolderEdit->setEnabled(enabled);
    ui->usernameEdit->setEnabled(enabled);
    ui->passwordEdit->setEnabled(enabled);
    ui->testConnectionButton->setEnabled(enabled);
    ui->syncNowButton->setEnabled(enabled && m_syncManager && m_syncManager->isConfigured());
    ui->autoSyncCheckBox->setEnabled(enabled);
    ui->syncIntervalSpinBox->setEnabled(enabled);
    ui->twoWayRadio->setEnabled(enabled);
    ui->uploadRadio->setEnabled(enabled);
    ui->downloadRadio->setEnabled(enabled);
    ui->buttonBox->setEnabled(enabled);
}

void WebDAVConfigDialog::updateSyncStatusUI()
{
    if (!m_syncManager)
        return;
    
    // 更新连接状态
    bool isConfigured = m_syncManager->isConfigured();
    WebDAVSyncManager::SyncStatus status = m_syncManager->status();
    
    // 更新连接状态文本
    if (!isConfigured) {
        ui->connectionStatusLabel->setText(tr("未配置"));
    } else {
        switch (status) {
        case WebDAVSyncManager::Idle:
            ui->connectionStatusLabel->setText(tr("已连接"));
            break;
        case WebDAVSyncManager::Syncing:
            ui->connectionStatusLabel->setText(tr("同步中..."));
            break;
        case WebDAVSyncManager::Error:
            ui->connectionStatusLabel->setText(tr("错误: %1").arg(m_syncManager->lastError()));
            break;
        default:
            ui->connectionStatusLabel->setText(tr("未知状态"));
            break;
        }
    }
    
    // 更新上次同步时间
    QDateTime lastSync = m_syncManager->lastSyncTime();
    if (lastSync.isValid()) {
        ui->lastSyncLabel->setText(tr("上次同步: %1").arg(lastSync.toString("yyyy-MM-dd hh:mm:ss")));
    } else {
        ui->lastSyncLabel->setText(tr("上次同步: 从未同步"));
    }
    
    // 更新同步状态
    switch (status) {
    case WebDAVSyncManager::NotConfigured:
        ui->syncStatusLabel->setText(tr("同步状态: 未配置"));
        break;
    case WebDAVSyncManager::Idle:
        ui->syncStatusLabel->setText(tr("同步状态: 就绪"));
        break;
    case WebDAVSyncManager::Syncing:
        ui->syncStatusLabel->setText(tr("同步状态: 正在同步..."));
        break;
    case WebDAVSyncManager::Error:
        ui->syncStatusLabel->setText(tr("同步状态: 错误 - %1").arg(m_syncManager->lastError()));
        break;
    }
} 