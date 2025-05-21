#include "note.h"
#include <QMap>

Note::Note() : m_id(-1), m_createTime(QDateTime::currentDateTime()), m_updateTime(QDateTime::currentDateTime())
{
}

Note::Note(int id, const QString &title, const QString &content, const QDateTime &createTime, const QDateTime &updateTime)
    : m_id(id), m_title(title), m_content(content), m_createTime(createTime), m_updateTime(updateTime)
{
}

int Note::id() const
{
    return m_id;
}

void Note::setId(int id)
{
    m_id = id;
}

QString Note::title() const
{
    return m_title;
}

void Note::setTitle(const QString &title)
{
    m_title = title;
}

QString Note::content() const
{
    return m_content;
}

void Note::setContent(const QString &content)
{
    m_content = content;
}

QDateTime Note::createTime() const
{
    return m_createTime;
}

void Note::setCreateTime(const QDateTime &createTime)
{
    m_createTime = createTime;
}

QDateTime Note::updateTime() const
{
    return m_updateTime;
}

void Note::setUpdateTime(const QDateTime &updateTime)
{
    m_updateTime = updateTime;
}

QVariant Note::toVariant() const
{
    QMap<QString, QVariant> map;
    map["id"] = m_id;
    map["title"] = m_title;
    map["content"] = m_content;
    map["createTime"] = m_createTime;
    map["updateTime"] = m_updateTime;
    return map;
}

Note Note::fromVariant(const QVariant &variant)
{
    QMap<QString, QVariant> map = variant.toMap();
    return Note(
        map["id"].toInt(),
        map["title"].toString(),
        map["content"].toString(),
        map["createTime"].toDateTime(),
        map["updateTime"].toDateTime()
    );
} 