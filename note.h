#ifndef NOTE_H
#define NOTE_H

#include <QString>
#include <QDateTime>
#include <QVariant>

class Note
{
public:
    Note();
    Note(int id, const QString &title, const QString &content, const QDateTime &createTime, const QDateTime &updateTime);
    
    int id() const;
    void setId(int id);
    
    QString title() const;
    void setTitle(const QString &title);
    
    QString content() const;
    void setContent(const QString &content);
    
    QDateTime createTime() const;
    void setCreateTime(const QDateTime &createTime);
    
    QDateTime updateTime() const;
    void setUpdateTime(const QDateTime &updateTime);
    
    // 用于将Note对象转换为QVariant，便于在QListWidget中使用
    QVariant toVariant() const;
    static Note fromVariant(const QVariant &variant);

private:
    int m_id;
    QString m_title;
    QString m_content;
    QDateTime m_createTime;
    QDateTime m_updateTime;
};

#endif // NOTE_H 