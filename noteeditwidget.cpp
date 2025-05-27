#include "noteeditwidget.h"
#include "ui_noteeditwidget.h"
#include <QDateTime>
#include <QMessageBox>
#include <QTextCharFormat>
#include <QFileDialog>
#include <QImageReader>
#include <QDebug>
#include <QScreen>
#include <QClipboard>
#include <QMimeData>
#include <QMenu>
#include <QAction>
#include <QBuffer>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QTextDocument>
#include <QTextImageFormat>
#include <QTextBlock>
#include <QTextFragment>
#include <QTimer>
#include <QTextCursor>
#include <QDir>
#include <QUuid>
#include <QFile>
#include <QFileInfo>
#include <QApplication>
#include <QGuiApplication>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QLabel>
#include <QRegularExpression>

NoteEditWidget::NoteEditWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::NoteEditWidget),
    m_database(new NoteDatabase(this)),
    m_autoSaveTimer(new QTimer(this)),
    m_isNewNote(false),
    m_hasChanges(false),
    m_isLoadingNote(false),
    m_isStayOnTop(false),
    m_imageEventFilter(new ImageEventFilter(this))
{
    ui->setupUi(this);
    
    // 设置窗口属性，允许调整大小，但禁用最大化按钮
    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | 
                  Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint | 
                  Qt::WindowMinimizeButtonHint);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(300, 400);
    
    // 设置自动保存定时器
    m_autoSaveTimer->setInterval(3000); // 3秒钟自动保存
    
    // 确保图片目录存在
    ensureImageDirectoryExists();
    
    setupUI();
    setupConnections();
    setupImageInteractions();
    
    // 打开数据库
    if (!m_database->open()) {
        QMessageBox::warning(this, "错误", "无法打开数据库");
    }
}

NoteEditWidget::~NoteEditWidget()
{
    // 如果有未保存的更改，保存它们
    if (m_hasChanges) {
        saveChanges();
    }
    
    // 清理临时图片
    cleanupUnusedImages();
    
    // 清理缓存的原始图片
    m_originalImages.clear();
    
    delete ui;
}

void NoteEditWidget::setupUI()
{
    // 设置窗口和内容区域的背景色为白色
    setStyleSheet("QWidget { background-color: white; }");
    
    // 按钮样式 - 设置白色背景
    QString buttonStyle = 
        "QPushButton { border: 1px solid #CCCCCC; border-radius: 3px; padding: 3px; background-color: white; }"
        "QPushButton:hover { background-color: #F0F0F0; }"
        "QPushButton:pressed { background-color: #E0E0E0; }"
        "QPushButton:checked { background-color: #D0D0D0; }";
    
    // 标题栏样式 - 设置白色背景
    ui->titleLineEdit->setStyleSheet(
        "QLineEdit { border: none; border-bottom: 1px solid #CCCCCC; "
        "padding: 8px; font-size: 14pt; font-weight: bold; background-color: white; }");
    
    // 内容编辑区样式 - 设置白色背景和美化滚动条
    ui->contentTextEdit->setStyleSheet(
        "QTextEdit { "
        "  border: none; "
        "  padding: 5px; "
        "  background-color: white; "
        "} "
        "QScrollBar:vertical { "
        "  background: transparent; "
        "  width: 8px; "
        "  margin: 0px; "
        "  border-radius: 4px; "
        "} "
        "QScrollBar::handle:vertical { "
        "  background: #CCCCCC; "
        "  min-height: 30px; "
        "  border-radius: 4px; "
        "} "
        "QScrollBar::handle:vertical:hover { "
        "  background: #AAAAAA; "
        "} "
        "QScrollBar::handle:vertical:pressed { "
        "  background: #888888; "
        "} "
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
        "  border: none; "
        "  background: none; "
        "  height: 0px; "
        "} "
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { "
        "  background: none; "
        "} "
        "QScrollBar:horizontal { "
        "  background: transparent; "
        "  height: 8px; "
        "  margin: 0px; "
        "  border-radius: 4px; "
        "} "
        "QScrollBar::handle:horizontal { "
        "  background: #CCCCCC; "
        "  min-width: 30px; "
        "  border-radius: 4px; "
        "} "
        "QScrollBar::handle:horizontal:hover { "
        "  background: #AAAAAA; "
        "} "
        "QScrollBar::handle:horizontal:pressed { "
        "  background: #888888; "
        "} "
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { "
        "  border: none; "
        "  background: none; "
        "  width: 0px; "
        "} "
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { "
        "  background: none; "
        "} ");
    
    // 设置底部按钮样式
    ui->boldButton->setStyleSheet(buttonStyle);
    ui->italicButton->setStyleSheet(buttonStyle);
    ui->underlineButton->setStyleSheet(buttonStyle);
    ui->imageButton->setStyleSheet(buttonStyle);
    ui->deleteButton->setStyleSheet(buttonStyle);
    
    // 设置删除按钮的文本为图标
    ui->deleteButton->setText("✕");
    
    // 创建窗口置顶按钮
    m_stayOnTopButton = new QPushButton(this);
    m_stayOnTopButton->setToolTip("窗口置顶");
    m_stayOnTopButton->setStyleSheet(buttonStyle);
    m_stayOnTopButton->setCheckable(true);
    m_stayOnTopButton->setMaximumSize(30, 30);
    m_stayOnTopButton->setText("📌"); // 使用图钉emoji作为图标
    
    // 添加置顶按钮到布局
    if (ui->horizontalLayout_2->indexOf(m_stayOnTopButton) == -1) {
        ui->horizontalLayout_2->insertWidget(0, m_stayOnTopButton);
    }
    
    // 设置字体样式按钮的文本
    ui->boldButton->setFont(QFont("Arial", 9, QFont::Bold));
    ui->italicButton->setFont(QFont("Arial", 9, QFont::StyleItalic));
    QFont underlineFont("Arial", 9);
    underlineFont.setUnderline(true);
    ui->underlineButton->setFont(underlineFont);
    
    // 确保删除按钮在底部工具栏
    if (ui->horizontalLayout_2->indexOf(ui->deleteButton) == -1) {
        ui->horizontalLayout_2->addWidget(ui->deleteButton);
    }
    
    // 使用调色板明确设置背景色
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::white);
    pal.setColor(QPalette::Base, Qt::white);
    setPalette(pal);
    ui->titleLineEdit->setPalette(pal);
    ui->contentTextEdit->setPalette(pal);
    
    // 设置窗口大小
    resize(450, 550);
    
    // 居中显示窗口
    if (QGuiApplication::screens().size() > 0) {
        QScreen *screen = QGuiApplication::screens().at(0);
        move(screen->geometry().center() - rect().center());
    }
    
    // 更新置顶按钮状态
    updateStayOnTopButton();
    
    // 获取字数统计标签指针
    m_wordCountLabel = findChild<QLabel*>("wordCountLabel");
    if (m_wordCountLabel) {
        m_wordCountLabel->setText("字数：0");
    }
}

void NoteEditWidget::setupConnections()
{
    connect(ui->contentTextEdit, &QTextEdit::textChanged, this, &NoteEditWidget::onContentChanged);
    connect(ui->contentTextEdit, &QTextEdit::textChanged, this, &NoteEditWidget::updateWordCount);
    connect(ui->titleLineEdit, &QLineEdit::textChanged, this, &NoteEditWidget::onTitleChanged);
    connect(ui->boldButton, &QPushButton::clicked, this, &NoteEditWidget::onBoldButtonClicked);
    connect(ui->italicButton, &QPushButton::clicked, this, &NoteEditWidget::onItalicButtonClicked);
    connect(ui->underlineButton, &QPushButton::clicked, this, &NoteEditWidget::onUnderlineButtonClicked);
    connect(ui->deleteButton, &QPushButton::clicked, this, &NoteEditWidget::onDeleteButtonClicked);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &NoteEditWidget::onAutoSaveTimeout);
    connect(m_stayOnTopButton, &QPushButton::clicked, this, &NoteEditWidget::onStayOnTopClicked);
    
    // 安装事件过滤器监听粘贴事件
    ui->contentTextEdit->installEventFilter(this);
    
    // 连接右键菜单
    createContextMenu();
    
    connect(ui->imageButton, &QPushButton::clicked, [this]() {
        QString fileName = QFileDialog::getOpenFileName(this, "插入图片",
                                                        QString(),
                                                        "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)");
        if (!fileName.isEmpty()) {
            QImage image(fileName);
            if (!image.isNull()) {
                // 保存原始图片
                QImage originalImage = image;
                
                // 获取设备像素比，处理Windows屏幕缩放问题
                qreal devicePixelRatio = qApp->devicePixelRatio();
                if (devicePixelRatio > 1.0) {
                    // 确保原始图像保持正确的设备像素比
                    originalImage.setDevicePixelRatio(1.0); // 重置为原始像素
                }
                
                // 调整图片大小
                image = resizeImageToFitWidth(image);
                
                // 保存图片到文件
                QString filePath = saveImageToFile(originalImage, "file");
                
                // 生成资源名称
                static int fileImageCounter = 0;
                QString imageName = QString("file_image_%1").arg(++fileImageCounter);
                
                // 将图片添加到文档资源
                ui->contentTextEdit->document()->addResource(
                    QTextDocument::ImageResource,
                    QUrl(filePath),
                    QVariant(image));
                
                // 计算显示尺寸，考虑设备像素比
                int displayWidth = image.width() / image.devicePixelRatio();
                int displayHeight = image.height() / image.devicePixelRatio();
                
                // 创建图片格式
                QTextImageFormat imageFormat;
                imageFormat.setName(filePath);
                imageFormat.setWidth(displayWidth);
                imageFormat.setHeight(displayHeight);
                
                // 插入图片到光标位置
                ui->contentTextEdit->textCursor().insertImage(imageFormat);
                
                // 记录临时图片映射
                m_tempImages[imageName] = filePath;
                
                // 标记文档已修改
                m_hasChanges = true;
                m_autoSaveTimer->start();
            }
        }
    });
    
    // 当文本编辑器光标位置改变时，更新格式按钮状态
    connect(ui->contentTextEdit, &QTextEdit::cursorPositionChanged, this, &NoteEditWidget::updateFormattingButtons);
}

void NoteEditWidget::setNote(const Note &note)
{
    m_isLoadingNote = true;
    
    // 如果当前有未保存的更改，保存它们
    if (m_hasChanges) {
        saveChanges();
    }
    
    m_currentNote = note;
    m_isNewNote = false;
    
    // 设置标题和内容
    QString title = note.title();
    ui->titleLineEdit->setText(title);
    ui->contentTextEdit->setHtml(note.content());
    
    // 预先显示窗口，确保viewport大小已确定
    show();
    
    // 处理内容中的图片引用
    QTimer::singleShot(10, this, [this]() {
        processContentAfterLoading();
    });
    
    // 更新窗口标题
    setWindowTitle(title.isEmpty() ? "便签" : title);
    
    m_hasChanges = false;
    m_isLoadingNote = false;
    
    // 停止自动保存计时器
    m_autoSaveTimer->stop();
    
    // 避免窗口重叠，给窗口位置添加一个小偏移
    static int offsetCounter = 0;
    if (QGuiApplication::screens().size() > 0) {
        QScreen *screen = QGuiApplication::screens().at(0);
        QRect screenGeometry = screen->geometry();
        
        // 计算基础位置（屏幕中心）
        QPoint basePos = screenGeometry.center() - rect().center();
        
        // 添加偏移（循环使用，避免窗口跑出屏幕）
        const int maxOffset = 200;
        int xOffset = (offsetCounter % 10) * 30;
        int yOffset = ((offsetCounter / 10) % 10) * 30;
        
        // 确保窗口在屏幕内
        QPoint newPos = basePos + QPoint(xOffset, yOffset);
        if (newPos.x() + width() > screenGeometry.right() - 20) {
            newPos.setX(screenGeometry.right() - width() - 20);
        }
        if (newPos.y() + height() > screenGeometry.bottom() - 20) {
            newPos.setY(screenGeometry.bottom() - height() - 20);
        }
        
        move(newPos);
        
        // 更新偏移计数器
        offsetCounter = (offsetCounter + 1) % 100;
    }
    
    // 如果窗口是置顶状态，确保置顶标志生效
    if (m_isStayOnTop) {
        // 保存当前位置
        QRect geometry = this->geometry();
        
        // 应用窗口标志，确保包含置顶标志
        Qt::WindowFlags flags = windowFlags();
        flags |= Qt::WindowStaysOnTopHint;
        setWindowFlags(flags);
        
        // 恢复位置
        setGeometry(geometry);
        show();
    }
    
    activateWindow(); // 确保窗口获得焦点
    
    // 更新字数统计
    updateWordCount();
}

void NoteEditWidget::createNewNote()
{
    m_isLoadingNote = true;
    
    // 如果当前有未保存的更改，保存它们
    if (m_hasChanges) {
        saveChanges();
    }
    
    // 创建新的笔记对象
    m_currentNote = Note();
    m_isNewNote = true;
    
    // 清空表单
    ui->titleLineEdit->clear();
    ui->contentTextEdit->clear();
    
    // 更新窗口标题
    setWindowTitle("新建便签");
    
    m_hasChanges = false;
    m_isLoadingNote = false;
    
    // 设置焦点到标题输入框
    ui->titleLineEdit->setFocus();
    
    // 显示窗口
    show();
    
    // 如果窗口是置顶状态，确保置顶标志生效
    if (m_isStayOnTop) {
        // 保存当前位置
        QRect geometry = this->geometry();
        
        // 应用窗口标志，确保包含置顶标志
        Qt::WindowFlags flags = windowFlags();
        flags |= Qt::WindowStaysOnTopHint;
        setWindowFlags(flags);
        
        // 恢复位置
        setGeometry(geometry);
        show();
    }
    
    activateWindow(); // 确保窗口获得焦点
    
    // 更新字数统计
    updateWordCount();
}

Note NoteEditWidget::getCurrentNote() const
{
    return m_currentNote;
}

bool NoteEditWidget::hasChanges() const
{
    return m_hasChanges;
}

void NoteEditWidget::saveChanges()
{
    if (!m_hasChanges) {
        return;
    }
    
    // 处理内容中的图片引用，确保持久化
    processContentForSaving();
    
    // 获取标题和内容
    QString title = ui->titleLineEdit->text().trimmed();
    QString content = ui->contentTextEdit->toHtml();
    
    // 更新笔记内容
    m_currentNote.setTitle(title);
    m_currentNote.setContent(content);
    m_currentNote.setUpdateTime(QDateTime::currentDateTime());
    
    // 更新窗口标题
    setWindowTitle(title.isEmpty() ? "便签" : title);
    
    // 保存到数据库
    if (m_database->saveNote(m_currentNote)) {
        m_hasChanges = false;
        m_isNewNote = false;
        
        // 获取便签ID
        int noteId = m_currentNote.id();
        
        // 如果有临时图片，将其移动到便签特定目录
        if (!m_tempImages.isEmpty() && noteId > 0) {
            QString noteDirPath = getImageDirectory(noteId);
            QDir dir;
            if (!dir.exists(noteDirPath)) {
                dir.mkpath(noteDirPath);
            }
            
            // 移动临时图片到便签目录
            for (auto it = m_tempImages.begin(); it != m_tempImages.end(); ++it) {
                QString sourcePath = it.value();
                QString fileName = QFileInfo(sourcePath).fileName();
                QString targetPath = QString("%1/%2").arg(noteDirPath, fileName);
                
                // 如果源和目标不同，且目标不存在，则移动文件
                if (sourcePath != targetPath && !QFile::exists(targetPath)) {
                    QFile::copy(sourcePath, targetPath);
                }
            }
            
            // 清空临时图片映射
            m_tempImages.clear();
        }
        
        // 发送保存成功信号
        emit noteSaved(m_currentNote);
    }
}

void NoteEditWidget::closeEvent(QCloseEvent *event)
{
    // 如果有未保存的更改，保存它们
    if (m_hasChanges) {
        saveChanges();
    }
    
    // 发送关闭信号
    emit closed();
    
    // 接受关闭事件
    event->accept();
    
    // 非模态对话框需要在关闭时自行删除，以防内存泄漏
    // 不过只有当窗口彻底独立（没有父窗口）且主窗口已经关闭时才这样做
    if (!parent() && !property("keepAlive").toBool()) {
        deleteLater();
    }
}

void NoteEditWidget::onContentChanged()
{
    if (m_isLoadingNote) {
        return;
    }
    
    m_hasChanges = true;
    m_autoSaveTimer->start(); // 重新启动自动保存计时器
}

void NoteEditWidget::onTitleChanged()
{
    if (m_isLoadingNote) {
        return;
    }
    
    m_hasChanges = true;
    
    // 更新窗口标题
    QString title = ui->titleLineEdit->text().trimmed();
    setWindowTitle(title.isEmpty() ? "便签" : title);
    
    m_autoSaveTimer->start(); // 重新启动自动保存计时器
}

void NoteEditWidget::onBoldButtonClicked()
{
    QTextCharFormat format;
    format.setFontWeight(ui->boldButton->isChecked() ? QFont::Bold : QFont::Normal);
    ui->contentTextEdit->mergeCurrentCharFormat(format);
    ui->contentTextEdit->setFocus();
}

void NoteEditWidget::onItalicButtonClicked()
{
    QTextCharFormat format;
    format.setFontItalic(ui->italicButton->isChecked());
    ui->contentTextEdit->mergeCurrentCharFormat(format);
    ui->contentTextEdit->setFocus();
}

void NoteEditWidget::onUnderlineButtonClicked()
{
    QTextCharFormat format;
    format.setFontUnderline(ui->underlineButton->isChecked());
    ui->contentTextEdit->mergeCurrentCharFormat(format);
    ui->contentTextEdit->setFocus();
}

void NoteEditWidget::onDeleteButtonClicked()
{
    // 如果是新笔记且未保存，直接清空
    if (m_isNewNote && m_currentNote.id() == -1) {
        ui->titleLineEdit->clear();
        ui->contentTextEdit->clear();
        return;
    }
    
    // 确认是否删除
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认删除", "确定要删除这条笔记吗？",
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        int noteId = m_currentNote.id();
        
        // 从数据库中删除
        if (m_database->deleteNote(noteId)) {
            // 删除便签对应的图片文件夹
            QString imageDirPath = getImageDirectory(noteId);
            if (!imageDirPath.isEmpty()) {
                QDir imageDir(imageDirPath);
                if (imageDir.exists()) {
                    // 递归删除目录及其内容
                    imageDir.removeRecursively();
                    qDebug() << "已删除便签图片目录:" << imageDirPath;
                }
            }
            
            // 发送删除信号
            emit noteDeleted(noteId);
            
            // 清空编辑器
            ui->titleLineEdit->clear();
            ui->contentTextEdit->clear();
            
            m_currentNote = Note();
            m_hasChanges = false;
            m_isNewNote = true;
            
            // 关闭窗口
            close();
        }
    }
}

void NoteEditWidget::onAutoSaveTimeout()
{
    if (m_hasChanges) {
        saveChanges();
    }
}

void NoteEditWidget::updateFormattingButtons()
{
    QTextCharFormat format = ui->contentTextEdit->currentCharFormat();
    
    ui->boldButton->setChecked(format.fontWeight() == QFont::Bold);
    ui->italicButton->setChecked(format.fontItalic());
    ui->underlineButton->setChecked(format.fontUnderline());
}

void NoteEditWidget::updateTitleLabel()
{
    QString title = ui->titleLineEdit->text().trimmed();
    if (title.isEmpty()) {
        setWindowTitle("新建便签");
    } else {
        // 如果标题太长，截断并加上省略号
        if (title.length() > 40) {
            setWindowTitle(title.left(40) + "...");
        } else {
            setWindowTitle(title);
        }
    }
}

int NoteEditWidget::getNoteId() const
{
    return m_currentNote.id();
}

// 切换窗口置顶状态
void NoteEditWidget::toggleStayOnTop()
{
    m_isStayOnTop = !m_isStayOnTop;
    
    // 保存当前窗口位置和大小
    QRect geometry = this->geometry();
    
    // 更新窗口标志
    Qt::WindowFlags flags = windowFlags();
    if (m_isStayOnTop) {
        flags |= Qt::WindowStaysOnTopHint;
    } else {
        flags &= ~Qt::WindowStaysOnTopHint;
    }
    
    // 应用新的窗口标志
    setWindowFlags(flags);
    
    // 恢复窗口位置和大小
    setGeometry(geometry);
    
    // 更新按钮状态
    updateStayOnTopButton();
    
    // 重新显示窗口
    show();
}

// 处理置顶按钮点击
void NoteEditWidget::onStayOnTopClicked()
{
    toggleStayOnTop();
}

// 更新置顶按钮状态
void NoteEditWidget::updateStayOnTopButton()
{
    if (m_stayOnTopButton) {
        m_stayOnTopButton->setChecked(m_isStayOnTop);
        if (m_isStayOnTop) {
            m_stayOnTopButton->setStyleSheet("QPushButton { background-color: #FFD700; border: 1px solid #CCCCCC; border-radius: 3px; padding: 3px; }"
                                          "QPushButton:hover { background-color: #FFEB3B; }"
                                          "QPushButton:pressed { background-color: #FFC107; }");
        } else {
            m_stayOnTopButton->setStyleSheet("QPushButton { background-color: white; border: 1px solid #CCCCCC; border-radius: 3px; padding: 3px; }"
                                          "QPushButton:hover { background-color: #F0F0F0; }"
                                          "QPushButton:pressed { background-color: #E0E0E0; }");
        }
    }
}

// 实现事件过滤器
bool NoteEditWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->contentTextEdit) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            
            // 监听Ctrl+V组合键
            if (keyEvent->matches(QKeySequence::Paste)) {
                // 尝试从剪贴板插入图片
                if (insertImageFromClipboard()) {
                    return true; // 事件已处理
                }
                // 如果没有图片，继续默认的粘贴操作
            }
        }
    }
    else if (obj == ui->contentTextEdit->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            QTextCursor cursor = ui->contentTextEdit->cursorForPosition(mouseEvent->pos());
            
            // 检查光标位置是否在图片上
            QTextCharFormat format = cursor.charFormat();
            if (format.isImageFormat()) {
                QTextImageFormat imageFormat = format.toImageFormat();
                QString imagePath = imageFormat.name();
                
                // 获取原始图片
                QImage originalImage;
                
                // 如果缓存了原始图片，从缓存获取
                if (m_originalImages.contains(imagePath)) {
                    originalImage = m_originalImages[imagePath];
                } else {
                    // 尝试从文件加载原图
                    QString localPath = imagePath;
                    if (imagePath.startsWith("file:///")) {
                        localPath = imagePath.mid(8);
                    }
                    
                    if (QFile::exists(localPath)) {
                        originalImage.load(localPath);
                        // 缓存原始图片
                        m_originalImages[imagePath] = originalImage;
                    }
                }
                
                // 获取图片在文档中的位置和大小信息
                QTextBlock block = cursor.block();
                QTextBlock::iterator it;
                
                // 查找图片在文本块中的位置
                bool foundImage = false;
                QRect imageRect;
                for (it = block.begin(); it != block.end() && !foundImage; ++it) {
                    QTextFragment fragment = it.fragment();
                    if (fragment.isValid()) {
                        QTextCharFormat fragFormat = fragment.charFormat();
                        if (fragFormat.isImageFormat()) {
                            QTextImageFormat imgFormat = fragFormat.toImageFormat();
                            if (imgFormat.name() == imagePath) {
                                // 获取图片在文档中的位置
                                int fragmentPosition = fragment.position();
                                QTextCursor imgCursor(ui->contentTextEdit->document());
                                imgCursor.setPosition(fragmentPosition);
                                QRect imgRect = ui->contentTextEdit->cursorRect(imgCursor);
                                
                                // 获取图片的宽高
                                int imgWidth = imgFormat.width();
                                int imgHeight = imgFormat.height();
                                
                                // 构建图片的实际矩形区域
                                imageRect = QRect(imgRect.left(), imgRect.top(), imgWidth, imgHeight);
                                foundImage = true;
                                break;
                            }
                        }
                    }
                }
                
                // 如果找到了图片，并且点击位置在图片的实际区域内
                if (foundImage) {
                    QPoint viewportPos = mouseEvent->pos();
                    if (imageRect.contains(viewportPos)) {
                if (!originalImage.isNull()) {
                    // 显示图片查看器
                    showImageViewer(originalImage);
                    return true; // 事件已处理
                }
                    }
                }
                
                // 如果点击位置不在图片的实际区域内，或者找不到图片，让事件继续传递，允许在图片周围编辑文本
                return false;
            }
        }
        else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            QTextCursor cursor = ui->contentTextEdit->cursorForPosition(mouseEvent->pos());
            
            // 检查光标位置是否在图片上
            QTextCharFormat format = cursor.charFormat();
            if (format.isImageFormat()) {
                QTextImageFormat imageFormat = format.toImageFormat();
                QString imagePath = imageFormat.name();
                
                // 获取图片在文档中的位置和大小信息
                QTextBlock block = cursor.block();
                QTextBlock::iterator it;
                
                // 查找图片在文本块中的位置
                bool foundImage = false;
                QRect imageRect;
                for (it = block.begin(); it != block.end() && !foundImage; ++it) {
                    QTextFragment fragment = it.fragment();
                    if (fragment.isValid()) {
                        QTextCharFormat fragFormat = fragment.charFormat();
                        if (fragFormat.isImageFormat()) {
                            QTextImageFormat imgFormat = fragFormat.toImageFormat();
                            if (imgFormat.name() == imagePath) {
                                // 获取图片在文档中的位置
                                int fragmentPosition = fragment.position();
                                QTextCursor imgCursor(ui->contentTextEdit->document());
                                imgCursor.setPosition(fragmentPosition);
                                QRect imgRect = ui->contentTextEdit->cursorRect(imgCursor);
                                
                                // 获取图片的宽高
                                int imgWidth = imgFormat.width();
                                int imgHeight = imgFormat.height();
                                
                                // 构建图片的实际矩形区域
                                imageRect = QRect(imgRect.left(), imgRect.top(), imgWidth, imgHeight);
                                foundImage = true;
                                break;
                            }
                        }
                    }
                }
                
                // 如果找到了图片，并且鼠标位置在图片的实际区域内
                if (foundImage && imageRect.contains(mouseEvent->pos())) {
                // 设置手型光标
                ui->contentTextEdit->viewport()->setCursor(Qt::PointingHandCursor);
                return true; // 事件已处理
                } else {
                    // 恢复默认光标
                    ui->contentTextEdit->viewport()->setCursor(Qt::IBeamCursor);
                }
            } else {
                // 恢复默认光标
                ui->contentTextEdit->viewport()->setCursor(Qt::IBeamCursor);
            }
        }
    }
    
    return QWidget::eventFilter(obj, event);
}

// 获取编辑器可用宽度
int NoteEditWidget::getAvailableWidth()
{
    // 获取编辑器的可用宽度（考虑内边距和滚动条）
    return ui->contentTextEdit->viewport()->width() - 20; // 减去一些边距以提供更好的视觉效果
}

// 调整图片大小以适应窗口宽度
QImage NoteEditWidget::resizeImageToFitWidth(const QImage &image)
{
    int availableWidth = getAvailableWidth();
    
    // 获取设备像素比，考虑Windows的屏幕缩放
    qreal devicePixelRatio = qApp->devicePixelRatio();
    
    // 如果图片宽度超过可用宽度，则按比例缩放
    if (image.width() > availableWidth) {
        // 计算缩放后的实际像素尺寸，考虑设备像素比
        int scaledWidth = availableWidth * devicePixelRatio;
        int scaledHeight = (scaledWidth * image.height()) / image.width();
        
        // 使用高质量缩放创建高清图像
        QImage scaledImage = image.scaled(scaledWidth, scaledHeight, 
                          Qt::KeepAspectRatio, 
                          Qt::SmoothTransformation);
        
        // 设置设备像素比，这样逻辑尺寸会自动调整
        scaledImage.setDevicePixelRatio(devicePixelRatio);
        
        return scaledImage;
    }
    
    // 如果图片实际尺寸不超过可用宽度，考虑设备像素比
    // 当设备像素比不为1时，需要调整图片显示尺寸以匹配实际物理尺寸
    if (devicePixelRatio > 1.0) {
        // 创建一个新的图像，保持原始像素大小，但为QTextDocument标记正确的逻辑尺寸
        QImage adjustedImage = image.copy();
        adjustedImage.setDevicePixelRatio(devicePixelRatio);
        return adjustedImage;
    }
    
    // 否则返回原图
    return image;
}

// 将图片保存到文件
QString NoteEditWidget::saveImageToFile(const QImage &image, const QString &prefix)
{
    // 确保图片目录存在
    ensureImageDirectoryExists();
    
    // 获取便签ID
    int noteId = m_currentNote.id();
    
    // 获取存储目录
    QString dirPath = getImageDirectory(noteId);
    QDir dir;
    if (!dir.exists(dirPath)) {
        dir.mkpath(dirPath);
    }
    
    // 生成唯一文件名
    QString fileName = QString("%1_%2.png").arg(prefix).arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QString filePath = QString("%1/%2").arg(dirPath, fileName);
    
    // 保存图片
    image.save(filePath, "PNG");
    
    // 保存原始图片到内存中，以便后续显示原图
    m_originalImages[filePath] = image;
    
    return filePath;
}

// 从剪贴板插入图片
bool NoteEditWidget::insertImageFromClipboard()
{
    const QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();
    
    // 检查剪贴板是否包含图片
    if (mimeData->hasImage()) {
        QImage image = qvariant_cast<QImage>(mimeData->imageData());
        if (!image.isNull()) {
            // 保存原始图片
            QImage originalImage = image;
            
            // 获取设备像素比，处理Windows屏幕缩放问题
            qreal devicePixelRatio = qApp->devicePixelRatio();
            if (devicePixelRatio > 1.0) {
                // 确保原始图像保持正确的设备像素比
                originalImage.setDevicePixelRatio(1.0); // 重置为原始像素
            }
            
            // 调整图片大小
            image = resizeImageToFitWidth(image);
            
            // 保存图片到文件
            QString filePath = saveImageToFile(originalImage, "clipboard");
            
            // 生成资源名称
            static int imageCounter = 0;
            QString imageName = QString("clipboard_image_%1").arg(++imageCounter);
            
            // 将调整后的图片添加到文档资源
            ui->contentTextEdit->document()->addResource(
                QTextDocument::ImageResource,
                QUrl(filePath),
                QVariant(image));
            
            // 计算显示尺寸，考虑设备像素比
            int displayWidth = image.width() / image.devicePixelRatio();
            int displayHeight = image.height() / image.devicePixelRatio();
            
            // 创建图片格式
            QTextImageFormat imageFormat;
            imageFormat.setName(filePath);
            imageFormat.setWidth(displayWidth);
            imageFormat.setHeight(displayHeight);
            
            // 插入图片到光标位置
            ui->contentTextEdit->textCursor().insertImage(imageFormat);
            
            // 记录临时图片映射
            m_tempImages[imageName] = filePath;
            
            // 标记文档已修改
            m_hasChanges = true;
            m_autoSaveTimer->start();
            
            return true;
        }
    }
    
    return false;
}

// 创建右键菜单
void NoteEditWidget::createContextMenu()
{
    // 为内容编辑区域添加自定义上下文菜单
    ui->contentTextEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    
    connect(ui->contentTextEdit, &QTextEdit::customContextMenuRequested, [this](const QPoint &pos) {
        QMenu *menu = ui->contentTextEdit->createStandardContextMenu();
        
        // 在菜单开头添加粘贴图片选项
        QAction *pasteImageAction = new QAction("粘贴图片", menu);
        connect(pasteImageAction, &QAction::triggered, this, &NoteEditWidget::onPaste);
        
        menu->insertAction(menu->actions().at(0), pasteImageAction);
        menu->insertSeparator(menu->actions().at(1));
        
        menu->exec(ui->contentTextEdit->mapToGlobal(pos));
        delete menu;
    });
}

// 处理粘贴操作
void NoteEditWidget::onPaste()
{
    insertImageFromClipboard();
}

void NoteEditWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    // 只在宽度变化时调整图片大小
    if (event->oldSize().width() != event->size().width()) {
        // 使用延迟处理，避免频繁调整
        QTimer::singleShot(100, this, [this]() {
            // 获取文档中的所有图片，并调整大小
            adjustImagesInDocument();
        });
    }
}

// 调整文档中所有图片的大小
void NoteEditWidget::adjustImagesInDocument()
{
    // 保存当前的光标位置
    QTextCursor cursor = ui->contentTextEdit->textCursor();
    int position = cursor.position();
    
    QTextDocument *document = ui->contentTextEdit->document();
    bool documentModified = false;
    
    // 获取当前可用宽度
    int availableWidth = getAvailableWidth();
    
    // 获取设备像素比，处理Windows屏幕缩放问题
    qreal devicePixelRatio = qApp->devicePixelRatio();
    
    // 遍历文档寻找图片
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); it != block.end(); ++it) {
            QTextFragment fragment = it.fragment();
            
            if (fragment.isValid()) {
                QTextCharFormat format = fragment.charFormat();
                
                if (format.isImageFormat()) {
                    QTextImageFormat imageFormat = format.toImageFormat();
                    
                    // 获取图片资源名
                    QString imageName = imageFormat.name();
                    
                    // 尝试获取原始图片
                    QImage originalImage;
                    
                    if (m_originalImages.contains(imageName)) {
                        // 使用缓存的原始图片
                        originalImage = m_originalImages[imageName];
                    } else {
                        // 尝试从文档资源获取图片
                        QVariant imageData = document->resource(QTextDocument::ImageResource, QUrl(imageName));
                        
                        if (imageData.type() == QVariant::Image) {
                            originalImage = qvariant_cast<QImage>(imageData);
                            
                            // 或者尝试从文件加载
                            if (originalImage.width() <= availableWidth) {
                                QString localPath = imageName;
                                if (imageName.startsWith("file:///")) {
                                    localPath = imageName.mid(8);
                                }
                                
                                if (QFile::exists(localPath)) {
                                    QImage fileImage(localPath);
                                    if (!fileImage.isNull()) {
                                        originalImage = fileImage;
                                        // 缓存原始图片
                                        m_originalImages[imageName] = originalImage;
                                    }
                                }
                            }
                        }
                    }
                    
                    if (!originalImage.isNull()) {
                        // 调整图片大小
                        QImage resizedImage = resizeImageToFitWidth(originalImage);
                        
                        // 更新文档资源
                        document->addResource(
                            QTextDocument::ImageResource,
                            QUrl(imageName),
                            QVariant(resizedImage));
                        
                        // 计算显示尺寸，考虑设备像素比
                        int displayWidth = resizedImage.width() / resizedImage.devicePixelRatio();
                        int displayHeight = resizedImage.height() / resizedImage.devicePixelRatio();
                        
                        // 更新图片宽度和高度
                        if (imageFormat.width() != displayWidth || 
                            imageFormat.height() != displayHeight) {
                            imageFormat.setWidth(displayWidth);
                            imageFormat.setHeight(displayHeight);
                            
                            // 创建临时光标，定位到这个片段
                            QTextCursor tempCursor(document);
                            tempCursor.setPosition(fragment.position());
                            tempCursor.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
                            
                            // 应用新格式
                            tempCursor.setCharFormat(imageFormat);
                            documentModified = true;
                        }
                    }
                }
            }
        }
    }
    
    // 恢复光标位置
    cursor.setPosition(position);
    ui->contentTextEdit->setTextCursor(cursor);
    
    // 如果文档被修改，标记为已更改但不触发自动保存
    if (documentModified) {
        ui->contentTextEdit->document()->markContentsDirty(0, ui->contentTextEdit->document()->characterCount());
    }
}

// 确保图片目录存在
void NoteEditWidget::ensureImageDirectoryExists()
{
    // 获取应用数据目录
    QString dataDir = NoteDatabase::getDatabaseDir();
    
    // 创建主图片目录
    QDir dir(dataDir);
    if (!dir.exists("images")) {
        dir.mkpath("images");
    }
    
    // 设置图片目录
    m_imagesDir.setPath(dataDir + "/images");
}

// 获取特定便签的图片目录
QString NoteEditWidget::getImageDirectory(int noteId) const
{
    QString dataDir = NoteDatabase::getDatabaseDir();
    
    if (noteId <= 0) {
        return dataDir + "/images/temp";
    }
    return QString("%1/images/%2").arg(dataDir).arg(noteId);
}

// 处理内容中的图片引用，准备保存
void NoteEditWidget::processContentForSaving()
{
    QTextDocument *document = ui->contentTextEdit->document();
    QString dataDir = NoteDatabase::getDatabaseDir();
    
    // 遍历文档中的所有文本块和片段，查找图片
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); it != block.end(); ++it) {
            QTextFragment fragment = it.fragment();
            
            if (fragment.isValid()) {
                QTextCharFormat format = fragment.charFormat();
                
                if (format.isImageFormat()) {
                    QTextImageFormat imageFormat = format.toImageFormat();
                    QString imageName = imageFormat.name();
                    
                    // 跳过已经保存的图片
                    if (imageName.startsWith("file:///") || 
                        imageName.startsWith(dataDir) ||
                        imageName.contains("/images/")) {
                        continue;
                    }
                    
                    // 获取图片数据
                    QVariant imageData = document->resource(QTextDocument::ImageResource, QUrl(imageName));
                    if (imageData.type() == QVariant::Image) {
                        QImage image = qvariant_cast<QImage>(imageData);
                        
                        // 保存图片到文件
                        QString filePath = saveImageToFile(image);
                        
                        // 更新文档资源
                        document->addResource(
                            QTextDocument::ImageResource,
                            QUrl(filePath),
                            QVariant(image));
                        
                        // 更新图片格式
                        imageFormat.setName(filePath);
                        
                        // 创建临时光标，定位到这个片段
                        QTextCursor tempCursor(document);
                        tempCursor.setPosition(fragment.position());
                        tempCursor.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
                        
                        // 应用新格式
                        tempCursor.setCharFormat(imageFormat);
                        
                        // 更新映射表
                        m_tempImages[imageName] = filePath;
                    }
                }
            }
        }
    }
}

// 加载后处理内容中的图片引用
void NoteEditWidget::processContentAfterLoading()
{
    QTextDocument *document = ui->contentTextEdit->document();
    bool documentModified = false;
    QString dataDir = NoteDatabase::getDatabaseDir();
    
    // 获取设备像素比，处理Windows屏幕缩放问题
    qreal devicePixelRatio = qApp->devicePixelRatio();
    
    // 遍历文档中的图片标签，找到路径
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); it != block.end(); ++it) {
            QTextFragment fragment = it.fragment();
            
            if (fragment.isValid()) {
                QTextCharFormat format = fragment.charFormat();
                
                if (format.isImageFormat()) {
                    QTextImageFormat imageFormat = format.toImageFormat();
                    QString imagePath = imageFormat.name();
                    
                    // 获取当前可用宽度
                    int availableWidth = getAvailableWidth();
                    
                    // 检查是否是文件路径
                    if (imagePath.startsWith("file:///") || 
                        imagePath.contains("/images/") || 
                        QFile::exists(imagePath)) {
                        
                        // 加载图片
                        QImage image;
                        QString localPath = imagePath;
                        
                        if (imagePath.startsWith("file:///")) {
                            // 移除file:///前缀
                            localPath = imagePath.mid(8);
                        }
                        
                        // 尝试加载图片
                        bool loaded = image.load(localPath);
                        
                        // 如果加载失败，尝试作为相对路径加载
                        if (!loaded && !QFile::exists(localPath)) {
                            // 先尝试当前工作目录
                            QString alternatePath = QDir::currentPath() + "/" + localPath;
                            loaded = image.load(alternatePath);
                            
                            // 如果还是失败，尝试数据目录
                            if (!loaded) {
                                alternatePath = dataDir + "/" + QFileInfo(localPath).fileName();
                                loaded = image.load(alternatePath);
                            }
                            
                            if (loaded) {
                                localPath = alternatePath;
                            }
                        }
                        
                        if (!image.isNull()) {
                            // 存储原始图片，用于后续可能需要查看原图
                            m_originalImages[imagePath] = image;
                            
                            // 调整图片大小
                            QImage resizedImage = resizeImageToFitWidth(image);
                            
                            // 更新文档资源
                            document->addResource(
                                QTextDocument::ImageResource,
                                QUrl(imagePath),
                                QVariant(resizedImage));
                            
                            // 计算显示尺寸，考虑设备像素比
                            int displayWidth = resizedImage.width() / resizedImage.devicePixelRatio();
                            int displayHeight = resizedImage.height() / resizedImage.devicePixelRatio();
                            
                            // 更新图片的宽度和高度属性
                            if (imageFormat.width() != displayWidth || 
                                imageFormat.height() != displayHeight) {
                                imageFormat.setWidth(displayWidth);
                                imageFormat.setHeight(displayHeight);
                                
                                // 创建临时光标，定位到这个片段
                                QTextCursor tempCursor(document);
                                tempCursor.setPosition(fragment.position());
                                tempCursor.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
                                
                                // 应用新格式
                                tempCursor.setCharFormat(imageFormat);
                                documentModified = true;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // 如果文档被修改，但不要标记为有未保存的更改
    if (documentModified) {
        // 只更新UI，不触发保存
        ui->contentTextEdit->document()->markContentsDirty(0, ui->contentTextEdit->document()->characterCount());
    }
}

// 清理未使用的图片
void NoteEditWidget::cleanupUnusedImages()
{
    // 如果是临时便签（未保存），清理其所有临时图片
    if (m_isNewNote && m_currentNote.id() <= 0) {
        for (const QString &filePath : m_tempImages.values()) {
            QFile::remove(filePath);
        }
    }
    
    // 清空临时图片映射
    m_tempImages.clear();
    
    // 清理临时目录下的所有图片
    // 注意：这部分代码谨慎使用，只在确保不会删除重要数据时启用
    /*
    QString tempDirPath = getImageDirectory(-1); // 获取临时目录
    QDir tempDir(tempDirPath);
    if (tempDir.exists()) {
        QStringList filters;
        filters << "*.png" << "*.jpg" << "*.jpeg" << "*.gif" << "*.bmp";
        tempDir.setNameFilters(filters);
        
        for (const QString &file : tempDir.entryList(QDir::Files)) {
            tempDir.remove(file);
        }
    }
    */
}

// 设置图片交互
void NoteEditWidget::setupImageInteractions()
{
    // 安装事件过滤器到文本编辑器
    ui->contentTextEdit->viewport()->installEventFilter(this);
}

// 显示图片查看器
void NoteEditWidget::showImageViewer(const QImage &image)
{
    if (image.isNull()) {
        return;
    }
    
    // 获取设备像素比，处理Windows屏幕缩放问题
    qreal devicePixelRatio = qApp->devicePixelRatio();
    
    // 创建并显示图片查看器对话框
    ImageViewerDialog *viewer = new ImageViewerDialog(image, devicePixelRatio, this);
    viewer->setAttribute(Qt::WA_DeleteOnClose); // 关闭时自动删除
    viewer->setModal(true);
    viewer->show();
}

// ------------------- ImageViewerDialog 实现 -------------------

ImageViewerDialog::ImageViewerDialog(const QImage &image, qreal devicePixelRatio, QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowTitleHint)
    , m_originalImage(image)
    , m_devicePixelRatio(devicePixelRatio)
{
    setWindowTitle("图片查看器");
    setMinimumSize(500, 400);
    
    // 根据图像大小和屏幕大小决定初始窗口大小
    QSize initialSize;
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        int maxWidth = screenGeometry.width() * 0.8;
        int maxHeight = screenGeometry.height() * 0.8;
        
        // 计算图像实际物理尺寸（考虑DPI缩放）
        int actualWidth = image.width();
        int actualHeight = image.height();
        
        // 如果图像比屏幕小，使用图像大小加上边距
        if (actualWidth < maxWidth && actualHeight < maxHeight) {
            initialSize = QSize(actualWidth + 40, actualHeight + 100); // 加上边距和控制栏高度
        } else {
            // 否则使用屏幕的80%
            initialSize = QSize(maxWidth, maxHeight);
        }
    } else {
        initialSize = QSize(800, 600); // 默认大小
    }
    resize(initialSize);
    
    setupUI();
    setupConnections();
    
    // 居中显示
    if (screen) {
        QRect screenGeometry = screen->geometry();
        move(screenGeometry.center() - rect().center());
    }
    
    // 自动应用反向缩放，确保默认100%缩放时显示正确的实际尺寸
    QTimer::singleShot(0, this, &ImageViewerDialog::resetZoom);
}

void ImageViewerDialog::setupUI()
{
    // 设置对话框基本样式
    setStyleSheet("QDialog { background-color: white; }");
    
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);
    
    // 创建场景和视图
    m_scene = new QGraphicsScene(this);
    m_graphicsView = new QGraphicsView(m_scene, this);
    m_graphicsView->setRenderHint(QPainter::SmoothPixmapTransform);
    m_graphicsView->setRenderHint(QPainter::Antialiasing);
    m_graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
    m_graphicsView->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    m_graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    
    // 设置GraphicsView样式，包括滚动条
    m_graphicsView->setStyleSheet(
        "QGraphicsView { "
        "  border: 1px solid #CCCCCC; "
        "  background-color: white; "
        "} "
        "QScrollBar:vertical { "
        "  background: transparent; "
        "  width: 8px; "
        "  margin: 0px; "
        "  border-radius: 4px; "
        "} "
        "QScrollBar::handle:vertical { "
        "  background: #CCCCCC; "
        "  min-height: 30px; "
        "  border-radius: 4px; "
        "} "
        "QScrollBar::handle:vertical:hover { "
        "  background: #AAAAAA; "
        "} "
        "QScrollBar::handle:vertical:pressed { "
        "  background: #888888; "
        "} "
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
        "  border: none; "
        "  background: none; "
        "  height: 0px; "
        "} "
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { "
        "  background: none; "
        "} "
        "QScrollBar:horizontal { "
        "  background: transparent; "
        "  height: 8px; "
        "  margin: 0px; "
        "  border-radius: 4px; "
        "} "
        "QScrollBar::handle:horizontal { "
        "  background: #CCCCCC; "
        "  min-width: 30px; "
        "  border-radius: 4px; "
        "} "
        "QScrollBar::handle:horizontal:hover { "
        "  background: #AAAAAA; "
        "} "
        "QScrollBar::handle:horizontal:pressed { "
        "  background: #888888; "
        "} "
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { "
        "  border: none; "
        "  background: none; "
        "  width: 0px; "
        "} "
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { "
        "  background: none; "
        "} ");
    
    // 将图片添加到场景
    QPixmap pixmap = QPixmap::fromImage(m_originalImage);
    m_pixmapItem = m_scene->addPixmap(pixmap);
    m_scene->setSceneRect(pixmap.rect());
    
    // 创建底部控制栏
    QHBoxLayout *controlLayout = new QHBoxLayout();
    controlLayout->setSpacing(10);
    
    // 创建缩放滑块
    m_zoomSlider = new QSlider(Qt::Horizontal, this);
    m_zoomSlider->setRange(10, 400);
    // 初始设置为100%，但实际缩放会在resetZoom中处理
    m_zoomSlider->setValue(100);
    m_zoomSlider->setTickInterval(50);
    m_zoomSlider->setTickPosition(QSlider::TicksBelow);
    
    // 设置滑块样式
    m_zoomSlider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "  border: 1px solid #CCCCCC;"
        "  height: 4px;"
        "  background: #F0F0F0;"
        "  margin: 0px;"
        "  border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "  background: #2196F3;"
        "  border: none;"
        "  width: 16px;"
        "  margin: -6px 0px;"
        "  border-radius: 8px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "  background: #1E88E5;"
        "}"
        "QSlider::sub-page:horizontal {"
        "  background: #2196F3;"
        "  border-radius: 2px;"
        "}"
    );
    
    // 按钮样式
    QString buttonStyle = 
        "QPushButton { "
        "  border: 1px solid #CCCCCC; "
        "  border-radius: 3px; "
        "  padding: 5px 15px; "
        "  background-color: white; "
        "  color: #333333; "
        "}"
        "QPushButton:hover { "
        "  background-color: #F0F0F0; "
        "}"
        "QPushButton:pressed { "
        "  background-color: #E0E0E0; "
        "}";
    
    // 创建重置和关闭按钮
    m_resetButton = new QPushButton("重置", this);
    m_closeButton = new QPushButton("关闭", this);
    
    // 应用按钮样式
    m_resetButton->setStyleSheet(buttonStyle);
    m_closeButton->setStyleSheet(buttonStyle);
    
    // 标签样式
    QString labelStyle = "QLabel { color: #333333; font-size: 12px; }";
    
    // 添加控件到底部布局
    QLabel *zoomLabel = new QLabel("缩放比例:", this);
    zoomLabel->setStyleSheet(labelStyle);
    QLabel *zoomValueLabel = new QLabel("100%", this);
    zoomValueLabel->setStyleSheet(labelStyle);
    zoomValueLabel->setMinimumWidth(50);
    
    controlLayout->addWidget(zoomLabel);
    controlLayout->addWidget(m_zoomSlider, 1); // 滑块占据更多空间
    controlLayout->addWidget(zoomValueLabel);
    controlLayout->addWidget(m_resetButton);
    controlLayout->addWidget(m_closeButton);
    
    // 连接缩放滑块的值变化信号，更新显示的缩放比例
    connect(m_zoomSlider, &QSlider::valueChanged, [zoomValueLabel](int value) {
        zoomValueLabel->setText(QString::number(value) + "%");
    });
    
    // 设置主布局
    mainLayout->addWidget(m_graphicsView);
    mainLayout->addLayout(controlLayout);
    setLayout(mainLayout);
}

void ImageViewerDialog::setupConnections()
{
    // 滑块值变化时更新缩放
    connect(m_zoomSlider, &QSlider::valueChanged, this, &ImageViewerDialog::updateZoom);
    
    // 点击重置按钮时重置缩放
    connect(m_resetButton, &QPushButton::clicked, this, &ImageViewerDialog::resetZoom);
    
    // 点击关闭按钮时关闭对话框
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

void ImageViewerDialog::updateZoom(int value)
{
    // 基本缩放比例
    qreal scale = value / 100.0;
    
    // 如果有DPI缩放，需要适当调整缩放比例
    if (m_devicePixelRatio > 1.0) {
        // 结合DPI缩放和用户请求的缩放
        scale = scale / m_devicePixelRatio;
    }
    
    // 应用缩放变换
    QTransform transform;
    transform.scale(scale, scale);
    m_graphicsView->setTransform(transform);
}

void ImageViewerDialog::resetZoom()
{
    // 重置变换
    m_graphicsView->resetTransform();
    
    // 确保图像在视图中居中
    m_graphicsView->setSceneRect(m_pixmapItem->boundingRect());
    m_graphicsView->centerOn(m_pixmapItem);
    
    // 首先检查是否需要考虑DPI缩放
    if (m_devicePixelRatio > 1.0) {
        // 计算反向缩放比例，使图像显示为实际大小
        qreal inverseScale = 1.0 / m_devicePixelRatio;
        
        // 应用反向缩放变换
        QTransform transform;
        transform.scale(inverseScale, inverseScale);
        m_graphicsView->setTransform(transform);
        
        // 将滑块设置为100%，表示这是实际尺寸
        m_zoomSlider->setValue(100);
        return;
    }
    
    // 如果没有DPI缩放，或者DPI缩放为1，则按正常方式处理
    QRectF viewportRect = m_graphicsView->viewport()->rect();
    QRectF sceneRect = m_scene->sceneRect();
    
    if (sceneRect.width() < viewportRect.width() && sceneRect.height() < viewportRect.height()) {
        // 图像已经完全显示，不需要额外操作
        m_zoomSlider->setValue(100);
    } else {
        // 计算合适的缩放比例，使图像适合视图
        qreal xScale = viewportRect.width() / sceneRect.width();
        qreal yScale = viewportRect.height() / sceneRect.height();
        qreal scale = qMin(xScale, yScale);
        
        // 只有在需要缩小时才应用缩放
        if (scale < 1.0) {
            // 计算百分比，更新滑块
            int zoomPercent = qRound(scale * 100);
            m_zoomSlider->setValue(zoomPercent);
            
            // 应用变换
            QTransform transform;
            transform.scale(scale, scale);
            m_graphicsView->setTransform(transform);
        } else {
            m_zoomSlider->setValue(100);
        }
    }
}

// ------------------- ImageEventFilter 实现 -------------------

ImageEventFilter::ImageEventFilter(QObject *parent)
    : QObject(parent)
{
}

bool ImageEventFilter::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        
        if (mouseEvent->button() == Qt::LeftButton) {
            // 获取父对象（应该是NoteEditWidget）
            QObject *parent = this->parent();
            NoteEditWidget *noteWidget = qobject_cast<NoteEditWidget*>(parent);
            
            if (noteWidget && !m_image.isNull()) {
                // 显示图片查看器
                QMetaObject::invokeMethod(noteWidget, "showImageViewer", 
                                         Qt::QueuedConnection,
                                         Q_ARG(QImage, m_image));
                return true; // 事件已处理
            }
        }
    } else if (event->type() == QEvent::Enter) {
        // 鼠标进入时更改光标为手型
        QWidget *widget = qobject_cast<QWidget*>(watched);
        if (widget) {
            widget->setCursor(Qt::PointingHandCursor);
            return true;
        }
    } else if (event->type() == QEvent::Leave) {
        // 鼠标离开时恢复光标
        QWidget *widget = qobject_cast<QWidget*>(watched);
        if (widget) {
            widget->setCursor(Qt::ArrowCursor);
            return true;
        }
    }
    
    return QObject::eventFilter(watched, event);
}

void NoteEditWidget::updateWordCount()
{
    if (!m_wordCountLabel) return;
    // 获取正文纯文本内容
    QString text = ui->contentTextEdit->toPlainText();
    int count = 0;
    // 统计汉字数量
    QRegularExpression hanziRe("[\u4e00-\u9fa5]");
    QRegularExpressionMatchIterator it = hanziRe.globalMatch(text);
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    // 统计英文单词数量
    QRegularExpression wordRe("[A-Za-z]+(?:'[A-Za-z]+)?");
    it = wordRe.globalMatch(text);
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    // 统计数字串数量（每个连续数字串算一个字）
    QRegularExpression numRe("\\d+");
    it = numRe.globalMatch(text);
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    m_wordCountLabel->setText(QString("字数：%1").arg(count));
} 