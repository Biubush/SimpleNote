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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_noteListWidget(new NoteListWidget(this))
    , m_noteEditWidget(new NoteEditWidget(nullptr)) // 保持编辑器独立，不设父窗口
    , m_database(new NoteDatabase(this))
{
    ui->setupUi(this);
    
    // 设置窗口属性，移除最大化按钮
    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | 
                   Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint | 
                   Qt::WindowMinimizeButtonHint);
    
    // 设置默认编辑窗口属性，使其在主窗口关闭后仍保持显示
    m_noteEditWidget->setProperty("keepAlive", true);
    
    // 创建数据库目录
    QDir dir;
    if (!dir.exists("database")) {
        dir.mkdir("database");
    }
    
    // 设置界面和外观
    setupUI();
    setupAppearance();
    
    // 打开数据库
    if (!m_database->open()) {
        QMessageBox::warning(this, "错误", "无法打开数据库");
    }
    
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
