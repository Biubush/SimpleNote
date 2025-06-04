#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QCloseEvent>
#include <QMessageBox>
#include <QDir>
#include <QStyle>
#include <QApplication>
#include <QScreen>
#include <QToolBar>
#include <QPushButton>
#include <QFileDialog>
#include <QStandardPaths>
#include <QProcess>
#include <QSizePolicy>
#include <QTimer>
#include <QDateTime>
#include <QSqlDatabase>
#include <QThread>
#include <QCoreApplication>
#include "webdavconfigdialog.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_noteListWidget(new NoteListWidget(this))
    , m_noteEditWidget(new NoteEditWidget(nullptr)) // 保持编辑器独立，不设父窗口
    , m_database(new NoteDatabase(this))
    , m_toolBar(nullptr)
    , m_webdavSyncManager(new WebDAVSyncManager(this))
{
    ui->setupUi(this);
    
    // 设置窗口属性，移除最大化按钮
    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | 
                   Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint | 
                   Qt::WindowMinimizeButtonHint);
    
    // 设置默认编辑窗口属性，使其在主窗口关闭后仍保持显示
    m_noteEditWidget->setProperty("keepAlive", true);
    
    // 设置界面和外观
    setupUI();
    setupAppearance();
    setupToolBar();
    setupMessageBoxStyle(); // 设置消息框样式
    
    // 打开数据库
    if (!m_database->open()) {
        QMessageBox::warning(this, "错误", "无法打开数据库");
    }
    
    // 设置WebDAV同步
    setupWebDAVSync();
    
    // 设置窗口标题
    setWindowTitle("便签");
    
    // 设置信号和槽连接
    setupConnections();
    
    // 加载便签列表
    m_noteListWidget->refreshNoteList();
}

MainWindow::~MainWindow()
{
    delete ui;
    
    // 不再关闭所有打开的便签窗口，它们现在独立存在
    
    // 断开与所有便签窗口的连接，使它们完全独立
    foreach (NoteEditWidget* window, m_openNoteWindows) {
        if (window) {
            // 断开信号连接，使窗口独立运行
            window->disconnect(this);
        }
    }
    
    // 清空窗口映射，但不删除窗口
    m_openNoteWindows.clear();
    
    // 断开默认编辑窗口的连接
    if (m_noteEditWidget) {
        m_noteEditWidget->disconnect(this);
        // 由于m_noteEditWidget没有父窗口，通常需要手动删除
        // 但现在我们希望它独立存在，不再删除它
        // 以避免意外访问已释放的内存，将指针设为nullptr
        m_noteEditWidget = nullptr;
    }
}

void MainWindow::setupUI()
{
    // 将列表窗口设置为中央组件
    setCentralWidget(m_noteListWidget);
    
    // 调整窗口大小
    resize(320, 600);
}

void MainWindow::setupAppearance()
{
    // 设置应用整体样式
    setStyleSheet(
        "QMainWindow { background-color: #F5F5F5; }"
        "QLineEdit { border: 1px solid #CCCCCC; border-radius: 3px; padding: 5px; }"
        "QPushButton { border: 1px solid #CCCCCC; border-radius: 3px; padding: 5px; }"
        "QPushButton:hover { background-color: #E0E0E0; }"
        "QPushButton:pressed { background-color: #D0D0D0; }"
        "QListWidget { border: none; background-color: #F5F5F5; }"
        "QListWidget::item { background-color: #FFFFFF; margin: 2px; border-radius: 3px; padding: 5px; }"
        "QListWidget::item:selected { background-color: #E0E0E0; }"
    );
    
    // 设置窗口图标
    setWindowIcon(QIcon(":/icons/simplenote.png"));
    
    // 居中显示窗口
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }
}

void MainWindow::setupToolBar()
{
    // 创建工具栏
    m_toolBar = new QToolBar(this);
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);
    m_toolBar->setIconSize(QSize(16, 16));
    m_toolBar->setStyleSheet(
        "QToolBar { border: none; background-color: #F5F5F5; spacing: 5px; padding: 2px; }"
        "QToolButton { border: 1px solid transparent; border-radius: 3px; padding: 2px; background-color: transparent; }"
        "QToolButton:hover { background-color: #2196F3; border: 1px solid #1E88E5; color: white; }"
        "QToolButton:pressed { background-color: #1976D2; color: white; }"
    );
    
    // 创建导出和导入按钮
    m_exportAction = new QAction(QIcon::fromTheme("document-save", QApplication::style()->standardIcon(QStyle::SP_DialogSaveButton)), "", this);
    m_importAction = new QAction(QIcon::fromTheme("document-open", QApplication::style()->standardIcon(QStyle::SP_DialogOpenButton)), "", this);
    
    // 创建WebDAV配置和同步按钮
    m_webdavConfigAction = new QAction(QIcon::fromTheme("network-server", QApplication::style()->standardIcon(QStyle::SP_ComputerIcon)), "", this);
    m_syncAction = new QAction(QIcon::fromTheme("view-refresh", QApplication::style()->standardIcon(QStyle::SP_BrowserReload)), "", this);
    
    // 设置图标为自动使用系统主题颜色
    m_exportAction->setProperty("iconVisibleInMenu", true);
    m_importAction->setProperty("iconVisibleInMenu", true);
    m_webdavConfigAction->setProperty("iconVisibleInMenu", true);
    m_syncAction->setProperty("iconVisibleInMenu", true);
    
    // 设置提示文本
    m_exportAction->setToolTip("导出便签数据");
    m_importAction->setToolTip("导入便签数据");
    m_webdavConfigAction->setToolTip("WebDAV云同步设置");
    m_syncAction->setToolTip("立即同步");
    
    // 添加到工具栏
    m_toolBar->addAction(m_exportAction);
    m_toolBar->addAction(m_importAction);
    
    // 添加一个分隔符
    m_toolBar->addSeparator();
    
    // 添加WebDAV相关按钮
    m_toolBar->addAction(m_webdavConfigAction);
    m_toolBar->addAction(m_syncAction);
    
    // 添加一个伸缩项，将按钮推到工具栏右侧
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_toolBar->addWidget(spacer);
    
    // 设置工具栏位置
    addToolBar(Qt::TopToolBarArea, m_toolBar);
    
    // 连接信号和槽
    connect(m_exportAction, &QAction::triggered, this, &MainWindow::exportDatabase);
    connect(m_importAction, &QAction::triggered, this, &MainWindow::importDatabase);
    connect(m_webdavConfigAction, &QAction::triggered, this, &MainWindow::showWebDAVConfigDialog);
    connect(m_syncAction, &QAction::triggered, this, &MainWindow::manualSync);
    
    // 默认禁用同步按钮，需要先配置WebDAV
    m_syncAction->setEnabled(false);
}

void MainWindow::setupConnections()
{
    // 当选择便签时，在编辑区域显示
    connect(m_noteListWidget, &NoteListWidget::noteSelected, this, &MainWindow::onNoteSelected);
    
    // 当点击新建按钮时，创建新便签
    connect(m_noteListWidget, &NoteListWidget::createNewNote, this, &MainWindow::onCreateNewNote);
    
    // 当默认便签保存时，刷新列表
    connect(m_noteEditWidget, &NoteEditWidget::noteSaved, this, &MainWindow::onNoteSaved);
    
    // 当默认便签删除时，刷新列表
    connect(m_noteEditWidget, &NoteEditWidget::noteDeleted, this, &MainWindow::onNoteDeleted);
    
    // 当默认编辑窗口关闭时处理
    connect(m_noteEditWidget, &NoteEditWidget::closed, this, &MainWindow::onEditWindowClosed);
    
    // WebDAV同步管理器连接
    connect(m_webdavSyncManager, &WebDAVSyncManager::syncStatusChanged, this, &MainWindow::onSyncStatusChanged);
    connect(m_webdavSyncManager, &WebDAVSyncManager::syncProgress, this, &MainWindow::onSyncProgress);
    connect(m_webdavSyncManager, &WebDAVSyncManager::syncFinished, this, &MainWindow::onSyncFinished);
    connect(m_webdavSyncManager, &WebDAVSyncManager::syncError, this, &MainWindow::onSyncError);
}

void MainWindow::onNoteSelected(const Note &note)
{
    // 打开或激活便签窗口
    openOrActivateNoteWindow(note);
}

void MainWindow::onCreateNewNote()
{
    // 新建便签都使用默认编辑窗口
    // 如果默认窗口不可见，显示它
    if (!m_noteEditWidget->isVisible()) {
        m_noteEditWidget->createNewNote();
        // 确保keepAlive属性已设置
        if (!m_noteEditWidget->property("keepAlive").toBool()) {
            m_noteEditWidget->setProperty("keepAlive", true);
        }
    } else {
        // 如果窗口已显示，激活它并创建新便签
        m_noteEditWidget->activateWindow();
        m_noteEditWidget->raise();
        m_noteEditWidget->createNewNote();
    }
}

void MainWindow::onNoteSaved(const Note &note)
{
    // 刷新便签列表
    m_noteListWidget->refreshNoteList();
    
    // 如果是一个新便签被保存，更新映射中的窗口引用
    // 如果便签ID从无效(-1)变为有效，需要将该窗口添加到映射
    int noteId = note.id();
    if (noteId > 0) {
        // 检查该便签是否已经存在于映射中
        bool exists = false;
        foreach (NoteEditWidget* window, m_openNoteWindows) {
            if (window && window->getNoteId() == noteId) {
                exists = true;
                break;
            }
        }
        
        // 如果便签不在映射中，但是使用的是默认窗口，那么不添加到映射
        // 默认窗口(m_noteEditWidget)用于创建新便签，不加入多窗口管理
    }
}

void MainWindow::onNoteDeleted(int noteId)
{
    // 从已打开窗口映射中移除
    if (m_openNoteWindows.contains(noteId)) {
        // 窗口会在关闭事件中自行删除
        m_openNoteWindows.remove(noteId);
    }
    
    // 刷新便签列表
    m_noteListWidget->refreshNoteList();
}

void MainWindow::onEditWindowClosed()
{
    // 默认编辑窗口关闭后刷新列表，确保所有更改都被显示
    m_noteListWidget->refreshNoteList();
}

void MainWindow::onNoteEditWindowClosed(int noteId)
{
    // 从映射中移除已关闭的窗口
    if (m_openNoteWindows.contains(noteId)) {
        // 窗口对象已经被删除，不需要手动删除
        m_openNoteWindows.remove(noteId);
    }
    
    // 刷新便签列表
    m_noteListWidget->refreshNoteList();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 不再强制关闭其他便签窗口，让它们独立存在
    
    // 如果默认编辑器有未保存的更改，保存它们
    if (m_noteEditWidget && m_noteEditWidget->hasChanges()) {
        m_noteEditWidget->saveChanges();
    }
    
    // 断开与所有便签窗口的连接，使它们完全独立
    foreach (NoteEditWidget* window, m_openNoteWindows) {
        if (window) {
            // 断开信号连接，使窗口独立运行
            window->disconnect(this);
        }
    }
    
    // 同样断开默认编辑窗口的连接
    if (m_noteEditWidget) {
        m_noteEditWidget->disconnect(this);
    }
    
    event->accept();
}

void MainWindow::openOrActivateNoteWindow(const Note &note)
{
    int noteId = note.id();
    
    // 检查便签是否已经打开
    if (m_openNoteWindows.contains(noteId)) {
        // 如果已打开，激活对应窗口
        NoteEditWidget *window = m_openNoteWindows[noteId];
        if (window) {
            window->activateWindow();
            window->raise();
            return;
        }
    }
    
    // 创建一个新的便签编辑窗口
    NoteEditWidget *newWindow = new NoteEditWidget(nullptr); // 不设置父窗口，使其成为独立窗口
    
    // 连接信号和槽
    connect(newWindow, &NoteEditWidget::noteSaved, this, &MainWindow::onNoteSaved);
    connect(newWindow, &NoteEditWidget::noteDeleted, this, &MainWindow::onNoteDeleted);
    
    // 使窗口关闭时能通知主窗口
    connect(newWindow, &NoteEditWidget::closed, [this, noteId]() {
        onNoteEditWindowClosed(noteId);
    });
    
    // 设置keepAlive属性，使其在主窗口关闭后仍然存活
    newWindow->setProperty("keepAlive", true);
    
    // 加载便签
    newWindow->setNote(note);
    
    // 将窗口添加到映射
    m_openNoteWindows[noteId] = newWindow;
}

void MainWindow::closeAllNoteWindows()
{
    // 关闭并删除所有打开的便签窗口
    foreach (NoteEditWidget* window, m_openNoteWindows) {
        if (window) {
            // 断开所有连接，避免触发信号
            window->disconnect();
            
            // 如果有未保存内容，保存它
            if (window->hasChanges()) {
                window->saveChanges();
            }
            
            // 关闭窗口并删除
            window->close();
            delete window;
        }
    }
    
    // 清空映射
    m_openNoteWindows.clear();
}

void MainWindow::exportDatabase()
{
    // 关闭数据库连接
    m_database->close();
    
    // 完全移除数据库连接，确保释放文件锁定
    QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    
    // 等待短暂时间确保文件完全释放
    QThread::msleep(500);
    
    // 获取数据库目录
    QString dbDir = NoteDatabase::getDatabaseDir();
    
    // 获取当前日期时间并格式化为字符串
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    
    // 选择保存位置（文件名中包含时间戳）
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) 
                         + "/SimpleNote_Backup_" + timestamp + ".zip";
    QString savePath = QFileDialog::getSaveFileName(this, "导出便签数据", 
                                                 defaultPath,
                                                 "ZIP文件 (*.zip)");
    
    if (savePath.isEmpty()) {
        // 重新打开数据库
        m_database->open();
        return;
    }
    
    // 确保文件扩展名是.zip
    if (!savePath.endsWith(".zip", Qt::CaseInsensitive)) {
        savePath += ".zip";
    }
    
    // 显示进度对话框
    QMessageBox progressMsg(this);
    progressMsg.setWindowTitle("导出中");
    progressMsg.setText("正在导出便签数据...\n请稍候。");
    progressMsg.setStandardButtons(QMessageBox::NoButton);
    progressMsg.setIcon(QMessageBox::Information);
    
    // 创建一个定时器，用于在对话框显示后启动导出过程
    QTimer::singleShot(200, this, [this, dbDir, savePath, &progressMsg]() {
        bool success = false;
        // 使用Qt内置的压缩功能创建一个压缩包
        QProcess zipProcess;
        
    #ifdef Q_OS_WIN
        // Windows平台使用PowerShell的压缩命令
        // 确保同时包含数据库文件和images文件夹
        QString command = "powershell.exe";
        QStringList args;
        
        // 创建临时目录用于存放要压缩的文件
        QString tempDir = QDir::tempPath() + "/SimpleNoteTemp_" + QDateTime::currentDateTime().toString("yyyyMMddHHmmss");
        
        // 备份WebDAV配置到临时目录
        bool webDAVConfigBacked = WebDAVSyncManager::backupConfig(tempDir);
        
        // 构建PowerShell命令：创建临时目录，复制文件到临时目录，压缩，然后清理
        args << "-Command" << QString(
            "try {\n"
            "    # 创建临时目录\n"
            "    if (!(Test-Path -Path \"%1\")) { New-Item -Path \"%1\" -ItemType Directory -Force }\n"
            "    \n"
            "    # 复制数据库文件到临时目录（如果存在）\n"
            "    if (Test-Path -Path \"%2\\notes.db\") { \n"
            "        Copy-Item -Path \"%2\\notes.db\" -Destination \"%1\\\" -Force\n"
            "    }\n"
            "    \n"
            "    # 复制图片文件夹到临时目录（如果存在）\n"
            "    if (Test-Path -Path \"%2\\images\") { \n"
            "        Copy-Item -Path \"%2\\images\" -Destination \"%1\\\" -Recurse -Force\n"
            "    }\n"
            "    \n"
            "    # 检查临时目录是否为空\n"
            "    if (@(Get-ChildItem -Path \"%1\" -Force).Count -gt 0) {\n"
            "        # 压缩临时目录的内容\n"
            "        Compress-Archive -Path \"%1\\*\" -DestinationPath \"%3\" -Force\n"
            "    } else {\n"
            "        Write-Error \"No files found to export\"\n"
            "        exit 1\n"
            "    }\n"
            "    \n"
            "    # 清理临时目录\n"
            "    if (Test-Path -Path \"%1\") { \n"
            "        Remove-Item -Path \"%1\" -Recurse -Force\n"
            "    }\n"
            "    \n"
            "    exit 0\n"
            "} catch {\n"
            "    Write-Error $_.Exception.Message\n"
            "    exit 1\n"
            "}\n"
        ).arg(tempDir, dbDir, savePath);
    #else
        // Linux/Mac平台使用zip命令
        QString command = "bash";
        QStringList args;
        
        // 创建临时脚本文件来执行命令
        QString scriptPath = QDir::tempPath() + "/simplenote_export.sh";
        QFile script(scriptPath);
        
        // 创建临时目录用于存放要压缩的文件
        QString tempDir = QDir::tempPath() + "/SimpleNoteTemp_" + QDateTime::currentDateTime().toString("yyyyMMddHHmmss");
        
        // 备份WebDAV配置到临时目录
        bool webDAVConfigBacked = WebDAVSyncManager::backupConfig(tempDir);
        
        if (script.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&script);
            out << "#!/bin/bash\n"
                << "set -e\n"  // 出错时立即退出
                << "# 创建临时目录\n"
                << "mkdir -p \"" << tempDir << "\"\n"
                << "# 复制数据库文件和图片到临时目录\n"
                << "if [ -f \"" << dbDir << "/notes.db\" ]; then\n"
                << "    cp \"" << dbDir << "/notes.db\" \"" << tempDir << "/\"\n"
                << "fi\n"
                << "if [ -d \"" << dbDir << "/images\" ]; then\n"
                << "    cp -r \"" << dbDir << "/images\" \"" << tempDir << "/\"\n"
                << "fi\n"
                << "# 检查临时目录是否为空\n"
                << "if [ -z \"$(ls -A \"" << tempDir << "\")\" ]; then\n"
                << "    echo \"No files found to export\"\n"
                << "    exit 1\n"
                << "fi\n"
                << "# 创建压缩包\n"
                << "cd \"" << tempDir << "\"\n"
                << "zip -r \"" << savePath << "\" * -q\n"
                << "# 清理临时目录\n"
                << "rm -rf \"" << tempDir << "\"\n";
            script.close();
            
            QFile::setPermissions(scriptPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
            args << scriptPath;
        } else {
            // 如果无法创建脚本文件，显示错误
            QMessageBox::warning(this, "导出失败", "无法创建临时脚本文件。");
            progressMsg.done(0);
            // 重新打开数据库
            m_database->open();
            return;
        }
    #endif
        
        zipProcess.start(command, args);
        zipProcess.waitForFinished(-1);
        
        // 清理临时脚本文件
    #ifndef Q_OS_WIN
        if (QFile::exists(scriptPath)) {
            QFile::remove(scriptPath);
        }
    #endif
        
        // 关闭进度对话框
        progressMsg.done(0);
        
        // 重新打开数据库
        m_database->open();
        
        if (zipProcess.exitCode() == 0) {
            QMessageBox::information(this, "导出成功", "便签数据已成功导出至:\n" + savePath);
            success = true;
        } else {
            QMessageBox::warning(this, "导出失败", "无法导出便签数据。\n错误信息: " + 
                                QString(zipProcess.readAllStandardError()));
        }
    });
    
    // 显示进度对话框（会阻塞直到done被调用）
    progressMsg.exec();
}

void MainWindow::importDatabase()
{
    // 提示用户注意事项
    QMessageBox::StandardButton reply = QMessageBox::question(this, 
        "导入便签数据", 
        "导入操作将覆盖当前的便签数据。导入成功后程序将自动重启。\n是否继续？",
        QMessageBox::Yes | QMessageBox::No);
        
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    // 选择要导入的ZIP文件
    QString importPath = QFileDialog::getOpenFileName(this, "选择要导入的备份文件",
                                                   QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                                   "ZIP文件 (*.zip)");
    
    if (importPath.isEmpty()) {
        return;
    }
    
    // 保存当前程序路径，用于导入后重启
    QString appPath = QCoreApplication::applicationFilePath();
    QStringList appArgs = QCoreApplication::arguments();
    appArgs.removeFirst(); // 移除程序路径参数
    
    // 关闭数据库连接和所有打开的便签窗口
    m_database->close();
    closeAllNoteWindows();
    
    // 获取数据库目录
    QString dbDir = NoteDatabase::getDatabaseDir();
    
    // 创建临时目录用于解压文件
    QString tempDir = QDir::tempPath() + "/SimpleNoteImportTemp_" + QDateTime::currentDateTime().toString("yyyyMMddHHmmss");
    
    // 显示进度对话框
    QMessageBox progressMsg(this);
    progressMsg.setWindowTitle("导入中");
    progressMsg.setText("正在导入便签数据...\n请稍候。");
    progressMsg.setStandardButtons(QMessageBox::NoButton);
    progressMsg.setIcon(QMessageBox::Information);
    
    // 创建一个定时器，用于在对话框显示后启动导入过程
    QTimer::singleShot(200, this, [this, dbDir, tempDir, importPath, appPath, appArgs, &progressMsg]() {
        bool success = false;
        QProcess unzipProcess;
        
    #ifdef Q_OS_WIN
        // Windows平台使用PowerShell的解压命令，先解压到临时目录
        QString command = "powershell.exe";
        QStringList args;
        
        // 确保临时目录存在
        args << "-Command" << QString("if (!(Test-Path -Path \"%1\")) { New-Item -Path \"%1\" -ItemType Directory -Force }").arg(tempDir);
        unzipProcess.start(command, args);
        unzipProcess.waitForFinished(-1);
        
        // 解压文件到临时目录
        args.clear();
        args << "-Command" << QString(
            "Add-Type -AssemblyName System.IO.Compression.FileSystem;\n"
            "try {\n"
            "    [System.IO.Compression.ZipFile]::ExtractToDirectory(\"%1\", \"%2\");\n"
            "    exit 0;\n"
            "} catch {\n"
            "    Write-Error $_.Exception.Message;\n"
            "    exit 1;\n"
            "}"
        ).arg(importPath, tempDir);
        
        unzipProcess.start(command, args);
        unzipProcess.waitForFinished(-1);
        
        // 检查解压是否成功
        if (unzipProcess.exitCode() == 0) {
            // 确保数据库已完全关闭和释放
            QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
            
            // 等待一小段时间，确保文件解锁
            QThread::msleep(500);
            
            // 从临时目录还原WebDAV配置文件
            WebDAVSyncManager::restoreConfig(tempDir);
            
            // 复制解压的文件到目标位置
            args.clear();
            args << "-Command" << QString(
                "# 确保目标目录存在\n"
                "if (!(Test-Path -Path \"%1\")) { New-Item -Path \"%1\" -ItemType Directory -Force }\n"
                
                "# 删除目标位置的旧文件\n"
                "if (Test-Path -Path \"%1\\notes.db\") { Remove-Item -Path \"%1\\notes.db\" -Force -ErrorAction SilentlyContinue }\n"
                "if (Test-Path -Path \"%1\\images\") { Remove-Item -Path \"%1\\images\" -Recurse -Force -ErrorAction SilentlyContinue }\n"
                
                "# 复制新文件\n"
                "if (Test-Path -Path \"%2\\notes.db\") { Copy-Item -Path \"%2\\notes.db\" -Destination \"%1\" -Force }\n"
                "if (Test-Path -Path \"%2\\images\") { Copy-Item -Path \"%2\\images\" -Destination \"%1\" -Recurse -Force }\n"
                
                "# 清理临时目录\n"
                "if (Test-Path -Path \"%2\") { Remove-Item -Path \"%2\" -Recurse -Force -ErrorAction SilentlyContinue }"
            ).arg(dbDir, tempDir);
            
            unzipProcess.start(command, args);
            unzipProcess.waitForFinished(-1);
            success = (unzipProcess.exitCode() == 0);
        }
    #else
        // Linux/Mac平台使用unzip命令
        QString command = "bash";
        QStringList args;
        
        // 创建一个临时脚本文件来执行命令
        QString scriptPath = QDir::tempPath() + "/simplenote_import.sh";
        QFile script(scriptPath);
        if (script.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&script);
            out << "#!/bin/bash\n"
                << "set -e\n"  // 出错时立即退出
                << "# 创建临时解压目录\n"
                << "mkdir -p \"" << tempDir << "\"\n"
                << "# 解压到临时目录\n"
                << "unzip -o \"" << importPath << "\" -d \"" << tempDir << "\"\n"
                << "# 确保目标目录存在\n"
                << "mkdir -p \"" << dbDir << "\"\n"
                << "# 删除目标位置的旧文件\n"
                << "rm -f \"" << dbDir << "/notes.db\"\n"
                << "rm -rf \"" << dbDir << "/images\"\n"
                << "# 复制新文件\n"
                << "if [ -f \"" << tempDir << "/notes.db\" ]; then\n"
                << "    cp -f \"" << tempDir << "/notes.db\" \"" << dbDir << "/\"\n"
                << "fi\n"
                << "if [ -d \"" << tempDir << "/images\" ]; then\n"
                << "    cp -rf \"" << tempDir << "/images\" \"" << dbDir << "/\"\n"
                << "fi\n"
                << "# 清理临时目录\n"
                << "rm -rf \"" << tempDir << "\"\n";
            script.close();
            
            QFile::setPermissions(scriptPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
            args << scriptPath;
            
            // 确保数据库已完全关闭和释放
            QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
            
            // 等待一小段时间，确保文件解锁
            QThread::msleep(500);
            
            // 从临时目录还原WebDAV配置文件
            WebDAVSyncManager::restoreConfig(tempDir);
            
            unzipProcess.start(command, args);
            unzipProcess.waitForFinished(-1);
            success = (unzipProcess.exitCode() == 0);
            
            // 清理临时脚本文件
            QFile::remove(scriptPath);
        } else {
            // 如果无法创建脚本文件，显示错误
            QMessageBox::warning(this, "导入失败", "无法创建临时脚本文件。");
        }
    #endif
        
        // 关闭进度对话框
        progressMsg.done(0);
        
        // 如果导入成功，重启应用
        if (success) {
            QMessageBox::information(this, "导入成功", "便签数据已成功导入。程序将自动重启以应用更改。");
            
            // 启动新实例
            QProcess::startDetached(appPath, appArgs);
            
            // 关闭当前实例
            QCoreApplication::quit();
        } else {
            // 重新打开数据库
            if (!m_database->isOpen()) {
                m_database->open();
                m_noteListWidget->refreshNoteList();
            }
            
            QMessageBox::warning(this, "导入失败", "无法导入便签数据。\n错误信息: " + 
                               QString(unzipProcess.readAllStandardError()));
        }
    });
    
    // 显示进度对话框（会阻塞直到done被调用）
    progressMsg.exec();
}

// 设置全局MessageBox样式
void MainWindow::setupMessageBoxStyle()
{
    // 创建一个全局样式表，应用于所有QMessageBox
    QString messageBoxStyle = 
        "QMessageBox {"
        "  background-color: #F5F5F5;"
        "}"
        "QMessageBox QPushButton {"
        "  min-width: 80px;"
        "  min-height: 24px;"
        "  border: 1px solid #CCCCCC;"
        "  border-radius: 3px;"
        "  padding: 5px;"
        "  background-color: #FFFFFF;"
        "}"
        "QMessageBox QPushButton:hover {"
        "  background-color: #E0E0E0;"
        "}"
        "QMessageBox QPushButton:pressed {"
        "  background-color: #D0D0D0;"
        "}"
        "QMessageBox QLabel {"
        "  color: #333333;"
        "}";
    
    // 应用样式
    qApp->setStyleSheet(qApp->styleSheet() + messageBoxStyle);
}

void MainWindow::setupWebDAVSync()
{
    // 设置数据库给同步管理器
    m_webdavSyncManager->setDatabase(m_database);
    
    // 加载WebDAV配置
    m_webdavSyncManager->loadConfig();
    
    // 根据配置状态更新同步按钮
    m_syncAction->setEnabled(m_webdavSyncManager->isConfigured());
}

void MainWindow::showWebDAVConfigDialog()
{
    // 创建并显示WebDAV配置对话框
    WebDAVConfigDialog dialog(this);
    dialog.setSyncManager(m_webdavSyncManager);
    
    if (dialog.exec() == QDialog::Accepted) {
        // 如果配置成功，启用同步按钮
        m_syncAction->setEnabled(m_webdavSyncManager->isConfigured());
    }
}

void MainWindow::manualSync()
{
    if (!m_webdavSyncManager->isConfigured()) {
        QMessageBox::warning(this, tr("同步错误"), tr("请先配置WebDAV服务器"));
        return;
    }
    
    // 开始同步
    m_webdavSyncManager->startSync();
}

void MainWindow::onSyncStatusChanged(WebDAVSyncManager::SyncStatus status)
{
    // 根据同步状态更新UI
    switch (status) {
    case WebDAVSyncManager::Syncing:
        statusBar()->showMessage(tr("正在同步..."));
        m_syncAction->setEnabled(false);
        break;
    case WebDAVSyncManager::Idle:
        statusBar()->showMessage(tr("同步就绪"), 5000);
        m_syncAction->setEnabled(true);
        break;
    case WebDAVSyncManager::Error:
        statusBar()->showMessage(tr("同步错误"), 5000);
        m_syncAction->setEnabled(true);
        break;
    case WebDAVSyncManager::NotConfigured:
        statusBar()->showMessage(tr("WebDAV未配置"), 5000);
        m_syncAction->setEnabled(false);
        break;
    }
}

void MainWindow::onSyncProgress(int percent, const QString &message)
{
    // 显示同步进度
    statusBar()->showMessage(tr("同步: %1% - %2").arg(percent).arg(message));
}

void MainWindow::onSyncFinished(bool success)
{
    if (success) {
        statusBar()->showMessage(tr("同步完成"), 5000);
        
        // 刷新便签列表，显示新同步的便签（如果有）
        m_noteListWidget->refreshNoteList();
    } else {
        statusBar()->showMessage(tr("同步失败"), 5000);
    }
    
    // 重新启用同步按钮
    m_syncAction->setEnabled(true);
}

void MainWindow::onSyncError(const QString &error)
{
    // 显示错误信息
    statusBar()->showMessage(tr("同步错误: %1").arg(error), 5000);
    
    // 可以考虑用更明显的方式显示错误
    QMessageBox::warning(this, tr("同步错误"), error);
    
    // 重新启用同步按钮
    m_syncAction->setEnabled(true);
}
