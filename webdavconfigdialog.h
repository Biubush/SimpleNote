#ifndef WEBDAVCONFIGDIALOG_H
#define WEBDAVCONFIGDIALOG_H

#include <QDialog>
#include <QMessageBox>

#include "webdavsyncmanager.h"

namespace Ui {
class WebDAVConfigDialog;
}

class WebDAVConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WebDAVConfigDialog(QWidget *parent = nullptr);
    ~WebDAVConfigDialog();
    
    // 设置同步管理器
    void setSyncManager(WebDAVSyncManager *syncManager);
    
private slots:
    void accept() override;
    void onTestConnection();
    void onSyncNowClicked();
    void updateSyncDirectionFromUI();
    void updateUIFromSettings();
    
    // WebDAVSyncManager信号处理槽
    void onSyncStatusChanged(WebDAVSyncManager::SyncStatus status);
    void onSyncProgress(int percent, const QString &message);
    void onSyncFinished(bool success);
    void onSyncError(const QString &error);

private:
    Ui::WebDAVConfigDialog *ui;
    WebDAVSyncManager *m_syncManager;
    
    // 测试连接或同步时禁用界面
    void setUIEnabled(bool enabled);
    
    // 更新同步状态UI
    void updateSyncStatusUI();
};

#endif // WEBDAVCONFIGDIALOG_H 