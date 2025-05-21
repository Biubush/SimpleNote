#include "notelistwidget.h"
#include "ui_notelistwidget.h"
#include <QDateTime>
#include <QIcon>
#include <QFont>
#include <QBrush>
#include <QTextDocument>
#include <QColor>
#include <QStringList>
#include <QHBoxLayout>
#include <QLabel>
#include <QAction>

NoteListWidget::NoteListWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::NoteListWidget),
    m_database(new NoteDatabase(this)),
    m_searchTimer(new QTimer(this))
{
    ui->setupUi(this);
    
    // 设置列表项显示两行
    ui->noteListWidget->setItemDelegate(new NoteItemDelegate(this));
    
    // 设置列表控件样式 - 透明背景，更好地展示卡片效果
    ui->noteListWidget->setStyleSheet(
        "QListWidget { "
        "  background-color: #F5F5F5; "
        "  border: none; "
        "  outline: none; "
        "  padding: 5px; "
        "}"
        "QListWidget::item { "
        "  background-color: transparent; "
        "  border: none; "
        "  margin: 3px 0px; "
        "}"
        "QListWidget::item:selected { "
        "  background-color: transparent; "
        "  border: none; "
        "}"
        // 添加滚动条样式
        "QScrollBar:vertical {"
        "  border: none;"
        "  background: #F5F5F5;"
        "  width: 8px;"
        "  margin: 0px 0px 0px 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #BDBDBD;"
        "  border-radius: 4px;"
        "  min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: #9E9E9E;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
    );
    
    // 设置搜索框样式
    ui->searchLineEdit->setStyleSheet(
        "QLineEdit { "
        "  background-color: #FFFFFF; "
        "  border: 1px solid #E0E0E0; "
        "  border-radius: 15px; "
        "  padding: 5px 10px; "
        "  margin: 5px; "
        "}"
    );
    
    // 设置添加按钮样式
    ui->addButton->setStyleSheet(
        "QPushButton { "
        "  background-color: #2196F3; "
        "  color: white; "
        "  border: none; "
        "  border-radius: 15px; "
        "  font-weight: bold; "
        "  font-size: 16px; "
        "}"
        "QPushButton:hover { background-color: #1E88E5; }"
        "QPushButton:pressed { background-color: #1976D2; }"
    );
    
    // 给搜索框添加搜索图标
    QAction *searchAction = new QAction(this);
    searchAction->setIcon(QIcon::fromTheme("edit-find", QIcon(":/icons/search.png")));
    ui->searchLineEdit->addAction(searchAction, QLineEdit::LeadingPosition);
    
    // 设置搜索延迟
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(300); // 300ms延迟
    
    // 设置主窗口样式
    setStyleSheet("QWidget { background-color: #F5F5F5; }");
    
    // 连接信号和槽
    connect(ui->addButton, &QPushButton::clicked, this, &NoteListWidget::onAddButtonClicked);
    connect(ui->noteListWidget, &QListWidget::itemClicked, this, &NoteListWidget::onNoteItemClicked);
    connect(ui->searchLineEdit, &QLineEdit::textChanged, this, &NoteListWidget::onSearchTextChanged);
    connect(m_searchTimer, &QTimer::timeout, this, &NoteListWidget::performSearch);
    
    // 打开数据库
    if (!m_database->open()) {
        // 处理数据库打开失败
        ui->emptyTextLabel->setText("无法打开数据库");
    }
    
    // 初始化界面
    refreshNoteList();
}

NoteListWidget::~NoteListWidget()
{
    delete ui;
}

void NoteListWidget::refreshNoteList()
{
    ui->noteListWidget->clear();
    
    QList<Note> notes = m_database->getAllNotes();
    for (const Note &note : notes) {
        addNoteToList(note);
    }
    
    updateEmptyStateVisibility();
}

Note NoteListWidget::getCurrentNote() const
{
    QListWidgetItem *currentItem = ui->noteListWidget->currentItem();
    if (currentItem) {
        return Note::fromVariant(currentItem->data(Qt::UserRole));
    }
    
    return Note();
}

void NoteListWidget::onAddButtonClicked()
{
    emit createNewNote();
}

void NoteListWidget::onNoteItemClicked(QListWidgetItem *item)
{
    if (item) {
        Note note = Note::fromVariant(item->data(Qt::UserRole));
        emit noteSelected(note);
    }
}

void NoteListWidget::onSearchTextChanged(const QString &text)
{
    m_lastSearchText = text;
    m_searchTimer->start(); // 启动搜索延迟计时器
}

void NoteListWidget::performSearch()
{
    ui->noteListWidget->clear();
    
    if (m_lastSearchText.isEmpty()) {
        refreshNoteList();
        return;
    }
    
    QList<Note> notes = m_database->searchNotes(m_lastSearchText);
    for (const Note &note : notes) {
        addNoteToList(note);
    }
    
    updateEmptyStateVisibility();
}

void NoteListWidget::addNoteToList(const Note &note)
{
    QListWidgetItem *item = new QListWidgetItem(ui->noteListWidget);
    
    // 设置显示内容 - 使用纯文本，不使用HTML
    // 对标题进行处理：如果内容为HTML则提取纯文本，限制长度，防止过长
    QString content = note.content();
    QString title = note.title();
    
    // 如果标题为空，尝试从内容中提取第一行作为标题
    if (title.isEmpty()) {
        // 如果内容是HTML，尝试提取纯文本
        if (content.contains("<html>") || content.contains("<body>")) {
            QTextDocument doc;
            doc.setHtml(content);
            content = doc.toPlainText();
        }
        
        // 使用内容的第一行作为标题
        title = content.split("\n").first().trimmed();
        
        // 如果标题仍然为空，使用"无标题"
        if (title.isEmpty()) {
            title = "无标题";
        }
    }
    
    // 限制标题长度
    if (title.length() > 30) {
        title = title.left(30) + "...";
    }
    
    // 设置时间格式
    QString timeStr = note.updateTime().toString("MM-dd HH:mm");
    
    // 设置纯文本标题
    item->setText(title);
    
    // 设置时间为子项
    item->setData(Qt::UserRole + 1, timeStr);
    item->setData(Qt::UserRole, note.toVariant());
    
    // 设置项目高度 - 增加高度以容纳卡片和阴影
    item->setSizeHint(QSize(ui->noteListWidget->width(), 75));
    
    ui->noteListWidget->addItem(item);
}

void NoteListWidget::updateEmptyStateVisibility()
{
    bool isEmpty = ui->noteListWidget->count() == 0;
    
    if (isEmpty) {
        // 如果列表为空，根据搜索状态显示不同的提示
        if (!m_lastSearchText.isEmpty()) {
            // 搜索无结果
            ui->emptyTextLabel->setText("没有找到匹配的便签\n尝试使用其他关键词");
        } else {
            // 没有便签
            ui->emptyTextLabel->setText("点击上方的\"+\"按钮\n创建您的第一个便签");
        }
    }
    
    ui->emptyStateWidget->setVisible(isEmpty);
    ui->noteListWidget->setVisible(!isEmpty);
} 