#ifndef NOTELISTWIDGET_H
#define NOTELISTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QTextDocument>
#include "note.h"
#include "notedatabase.h"

// 自定义列表项代理，用于绘制两行内容（标题和时间）
class NoteItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit NoteItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
    
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        
        // 设置卡片边距
        const int margin = 8;
        QRect cardRect = opt.rect.adjusted(margin, margin/2, -margin, -margin/2);
        
        // 绘制阴影效果 - 增强阴影效果，让卡片更具立体感
        painter->setPen(Qt::NoPen);
        for (int i = 0; i < 5; i++) {
            QColor shadowColor(0, 0, 0, 30 - i * 5);
            painter->setBrush(shadowColor);
            QRect shadowRect = cardRect.adjusted(i, i, i, i);
            painter->drawRoundedRect(shadowRect, 6, 6);
        }
        
        // 绘制卡片背景（考虑选中状态）
        if (opt.state & QStyle::State_Selected) {
            // 绘制选中状态背景
            painter->setBrush(QColor("#E3F2FD"));
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(cardRect, 6, 6);
            
            // 左侧添加蓝色条 - 使用稍微粗一点的条
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor("#2196F3"));
            painter->drawRoundedRect(QRect(cardRect.left() + 2, cardRect.top() + 3, 4, cardRect.height() - 6), 2, 2);
        } else {
            // 使用白色背景并添加细边框
            painter->setBrush(QColor("#FFFFFF"));
            painter->setPen(QPen(QColor("#E0E0E0"), 1));
            painter->drawRoundedRect(cardRect, 6, 6);
        }
        
        // 绘制标题
        QFont titleFont = opt.font;
        titleFont.setBold(true);
        titleFont.setPointSize(titleFont.pointSize() + 1);
        painter->setFont(titleFont);
        painter->setPen(QColor(50, 50, 50));
        
        QRect titleRect = cardRect;
        titleRect.setHeight(cardRect.height() / 2);
        painter->drawText(titleRect.adjusted(15, 8, -10, 0), Qt::AlignLeft | Qt::AlignVCenter, index.data(Qt::DisplayRole).toString());
        
        // 绘制时间
        QFont timeFont = opt.font;
        timeFont.setPointSize(timeFont.pointSize() - 1);
        painter->setFont(timeFont);
        painter->setPen(QColor(128, 128, 128));
        
        QRect timeRect = cardRect;
        timeRect.setTop(titleRect.bottom() - 5);
        timeRect.setHeight(cardRect.height() / 2);
        painter->drawText(timeRect.adjusted(15, 0, -10, -5), Qt::AlignRight | Qt::AlignVCenter, index.data(Qt::UserRole + 1).toString());
        
        painter->restore();
    }
    
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(75); // 增加高度以适应阴影效果
        return size;
    }
};

namespace Ui {
class NoteListWidget;
}

class NoteListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NoteListWidget(QWidget *parent = nullptr);
    ~NoteListWidget();

    void refreshNoteList();
    Note getCurrentNote() const;

signals:
    void noteSelected(const Note &note);
    void createNewNote();

private slots:
    void onAddButtonClicked();
    void onNoteItemClicked(QListWidgetItem *item);
    void onSearchTextChanged(const QString &text);
    void performSearch();

private:
    Ui::NoteListWidget *ui;
    NoteDatabase *m_database;
    QTimer *m_searchTimer;
    QString m_lastSearchText;

    void addNoteToList(const Note &note);
    void updateEmptyStateVisibility();
};

#endif // NOTELISTWIDGET_H 