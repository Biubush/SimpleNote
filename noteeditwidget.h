#ifndef NOTEEDITWIDGET_H
#define NOTEEDITWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QEvent>
#include <QPoint>
#include <QLabel>
#include <QPushButton>
#include <QClipboard>
#include <QMimeData>
#include <QImage>
#include <QResizeEvent>
#include <QDir>
#include <QUuid>
#include <QMap>
#include <QDialog>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include "note.h"
#include "notedatabase.h"

// 图片查看器对话框，允许查看原始尺寸图片并可调整大小
class ImageViewerDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit ImageViewerDialog(const QImage &image, qreal devicePixelRatio, QWidget *parent = nullptr);
    
private slots:
    void updateZoom(int value);
    void resetZoom();
    
private:
    void setupUI();
    void setupConnections();
    
    QImage m_originalImage;
    QGraphicsScene *m_scene;
    QGraphicsView *m_graphicsView;
    QGraphicsPixmapItem *m_pixmapItem;
    QSlider *m_zoomSlider;
    QPushButton *m_resetButton;
    QPushButton *m_closeButton;
    qreal m_devicePixelRatio;  // 设备像素比
};

// 图片项事件过滤器，处理图片的鼠标交互
class ImageEventFilter : public QObject
{
    Q_OBJECT
    
public:
    explicit ImageEventFilter(QObject *parent = nullptr);
    
    void setImage(const QImage &image) { m_image = image; }
    
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    
private:
    QImage m_image;
};

namespace Ui {
class NoteEditWidget;
}

class NoteEditWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NoteEditWidget(QWidget *parent = nullptr);
    ~NoteEditWidget();

    void setNote(const Note &note);
    void createNewNote();
    Note getCurrentNote() const;
    int getNoteId() const; // 获取当前便签ID
    bool hasChanges() const;
    void saveChanges();
    
    // 切换窗口置顶状态
    void toggleStayOnTop();
    
    // 检查窗口是否置顶
    bool isStayOnTop() const { return m_isStayOnTop; }

signals:
    void noteSaved(const Note &note);
    void noteDeleted(int noteId);
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onContentChanged();
    void onTitleChanged();
    void onBoldButtonClicked();
    void onItalicButtonClicked();
    void onUnderlineButtonClicked();
    void onDeleteButtonClicked();
    void onAutoSaveTimeout();
    void onStayOnTopClicked();
    void onPaste();
    void showImageViewer(const QImage &image); // 显示图片查看器
    void updateWordCount(); // 新增：更新字数统计

private:
    Ui::NoteEditWidget *ui;
    Note m_currentNote;
    NoteDatabase *m_database;
    QTimer *m_autoSaveTimer;
    bool m_isNewNote;
    bool m_hasChanges;
    bool m_isLoadingNote;
    bool m_isStayOnTop;    // 窗口是否置顶
    QPushButton* m_stayOnTopButton; // 置顶按钮
    QDir m_imagesDir;      // 图片存储目录
    QMap<QString, QString> m_tempImages; // 临时图片映射（资源名->文件路径）
    QMap<QString, QImage> m_originalImages; // 保存原始图片，用于显示原图
    ImageEventFilter *m_imageEventFilter; // 图片事件过滤器
    // 新增：字数统计标签指针
    QLabel* m_wordCountLabel;

    void updateFormattingButtons();
    void setupConnections();
    void setupUI();
    void updateTitleLabel(); // 更新窗口标题
    void updateStayOnTopButton(); // 更新置顶按钮状态
    bool insertImageFromClipboard(); // 从剪贴板插入图片
    void createContextMenu(); // 创建右键菜单
    QImage resizeImageToFitWidth(const QImage &image); // 调整图片大小以适应窗口宽度
    int getAvailableWidth(); // 获取编辑器可用宽度
    void adjustImagesInDocument(); // 调整文档中所有图片的大小
    void setupImageInteractions(); // 设置图片交互
    
    // 图片持久化相关方法
    void ensureImageDirectoryExists(); // 确保图片目录存在
    QString saveImageToFile(const QImage &image, const QString &prefix = "img"); // 将图片保存到文件
    void processContentForSaving(); // 处理内容中的图片引用，准备保存
    void processContentAfterLoading(); // 加载后处理内容中的图片引用
    QString getImageDirectory(int noteId) const; // 获取特定便签的图片目录
    void cleanupUnusedImages(); // 清理未使用的图片
};

#endif // NOTEEDITWIDGET_H 