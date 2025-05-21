#ifndef NOTEDATABASE_H
#define NOTEDATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QList>
#include "note.h"

class NoteDatabase : public QObject
{
    Q_OBJECT
public:
    explicit NoteDatabase(QObject *parent = nullptr);
    ~NoteDatabase();

    bool open();
    void close();
    bool isOpen() const;

    // 笔记相关操作
    bool saveNote(Note &note);
    bool deleteNote(int id);
    Note getNote(int id);
    QList<Note> getAllNotes();
    QList<Note> searchNotes(const QString &keyword);

private:
    bool createTables();
    bool initDatabase();
    
    QSqlDatabase m_db;
    QString m_dbPath;
    bool m_isOpen;
};

#endif // NOTEDATABASE_H 