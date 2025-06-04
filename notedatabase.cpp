#include "notedatabase.h"
#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDateTime>
#include <QStandardPaths>

// 获取数据库存储目录的静态方法
QString NoteDatabase::getDatabaseDir()
{
    // 获取应用数据存储位置
    QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dataLocation;
}

// 获取数据库文件路径的静态方法
QString NoteDatabase::getDatabasePath()
{
    return getDatabaseDir() + "/notes.db";
}

NoteDatabase::NoteDatabase(QObject *parent) : QObject(parent), m_isOpen(false)
{
    // 获取并创建应用程序数据目录
    QString dataDir = getDatabaseDir();
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // 设置数据库路径
    m_dbPath = getDatabasePath();
    qDebug() << "数据库路径：" << m_dbPath;
    
    // 注册数据库连接
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(m_dbPath);
}

NoteDatabase::~NoteDatabase()
{
    if (m_isOpen) {
        close();
    }
}

bool NoteDatabase::open()
{
    if (m_isOpen) {
        return true;
    }
    
    if (!m_db.open()) {
        qDebug() << "无法打开数据库: " << m_db.lastError().text();
        return false;
    }
    
    m_isOpen = true;
    
    // 初始化数据库表
    if (!createTables()) {
        close();
        return false;
    }
    
    return true;
}

void NoteDatabase::close()
{
    if (m_isOpen) {
        m_db.close();
        m_isOpen = false;
    }
}

bool NoteDatabase::isOpen() const
{
    return m_isOpen;
}

bool NoteDatabase::createTables()
{
    QSqlQuery query;
    
    // 创建笔记表
    QString sql = "CREATE TABLE IF NOT EXISTS notes ("
                  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                  "title TEXT, "
                  "content TEXT, "
                  "create_time DATETIME, "
                  "update_time DATETIME)";
    
    if (!query.exec(sql)) {
        qDebug() << "创建表失败: " << query.lastError().text();
        return false;
    }
    
    return true;
}

bool NoteDatabase::saveNote(Note &note)
{
    if (!m_isOpen) {
        if (!open()) {
            return false;
        }
    }
    
    QSqlQuery query;
    
    if (note.id() == -1) {
        // 新建笔记
        query.prepare("INSERT INTO notes (title, content, create_time, update_time) "
                     "VALUES (?, ?, ?, ?)");
        query.addBindValue(note.title());
        query.addBindValue(note.content());
        query.addBindValue(note.createTime());
        query.addBindValue(note.updateTime());
        
        if (!query.exec()) {
            qDebug() << "保存笔记失败: " << query.lastError().text();
            return false;
        }
        
        // 获取新插入记录的ID
        note.setId(query.lastInsertId().toInt());
    } else {
        // 更新已有笔记
        query.prepare("UPDATE notes SET title = ?, content = ?, update_time = ? "
                     "WHERE id = ?");
        query.addBindValue(note.title());
        query.addBindValue(note.content());
        query.addBindValue(QDateTime::currentDateTime());
        query.addBindValue(note.id());
        
        if (!query.exec()) {
            qDebug() << "更新笔记失败: " << query.lastError().text();
            return false;
        }
    }
    
    return true;
}

bool NoteDatabase::deleteNote(int id)
{
    if (!m_isOpen) {
        if (!open()) {
            return false;
        }
    }
    
    QSqlQuery query;
    query.prepare("DELETE FROM notes WHERE id = ?");
    query.addBindValue(id);
    
    if (!query.exec()) {
        qDebug() << "删除笔记失败: " << query.lastError().text();
        return false;
    }
    
    return true;
}

Note NoteDatabase::getNote(int id)
{
    Note note;
    
    if (!m_isOpen) {
        if (!open()) {
            return note;
        }
    }
    
    QSqlQuery query;
    query.prepare("SELECT id, title, content, create_time, update_time FROM notes WHERE id = ?");
    query.addBindValue(id);
    
    if (!query.exec()) {
        qDebug() << "获取笔记失败: " << query.lastError().text();
        return note;
    }
    
    if (query.next()) {
        note.setId(query.value(0).toInt());
        note.setTitle(query.value(1).toString());
        note.setContent(query.value(2).toString());
        note.setCreateTime(query.value(3).toDateTime());
        note.setUpdateTime(query.value(4).toDateTime());
    }
    
    return note;
}

QList<Note> NoteDatabase::getAllNotes()
{
    QList<Note> notes;
    
    if (!m_isOpen) {
        if (!open()) {
            return notes;
        }
    }
    
    QSqlQuery query("SELECT id, title, content, create_time, update_time FROM notes ORDER BY update_time DESC");
    
    if (!query.exec()) {
        qDebug() << "获取所有笔记失败: " << query.lastError().text();
        return notes;
    }
    
    while (query.next()) {
        Note note;
        note.setId(query.value(0).toInt());
        note.setTitle(query.value(1).toString());
        note.setContent(query.value(2).toString());
        note.setCreateTime(query.value(3).toDateTime());
        note.setUpdateTime(query.value(4).toDateTime());
        notes.append(note);
    }
    
    return notes;
}

QList<Note> NoteDatabase::searchNotes(const QString &keyword)
{
    QList<Note> notes;
    
    if (!m_isOpen) {
        if (!open()) {
            return notes;
        }
    }
    
    QSqlQuery query;
    query.prepare("SELECT id, title, content, create_time, update_time FROM notes "
                 "WHERE title LIKE ? OR content LIKE ? "
                 "ORDER BY update_time DESC");
    query.addBindValue(QString("%%1%").arg(keyword));
    query.addBindValue(QString("%%1%").arg(keyword));
    
    if (!query.exec()) {
        qDebug() << "搜索笔记失败: " << query.lastError().text();
        return notes;
    }
    
    while (query.next()) {
        Note note;
        note.setId(query.value(0).toInt());
        note.setTitle(query.value(1).toString());
        note.setContent(query.value(2).toString());
        note.setCreateTime(query.value(3).toDateTime());
        note.setUpdateTime(query.value(4).toDateTime());
        notes.append(note);
    }
    
    return notes;
} 