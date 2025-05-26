#include "mainwindow.h"
#include "notedatabase.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QFontDatabase>
#include <QLocale>
#include <QTranslator>
#include <QStandardPaths>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // 设置应用程序信息
    QApplication::setApplicationName("SimpleNote");
    QApplication::setApplicationVersion("1.0.0");
    // 不设置组织名，这样应用数据目录将只使用应用程序名
    // QApplication::setOrganizationName("Biubush");
    // QApplication::setOrganizationDomain("biubush.com");
    
    // 设置应用程序图标
    QApplication::setWindowIcon(QIcon(":/icons/simplenote.png"));
    
    // 设置本地化
    QLocale locale = QLocale::system();
    QLocale::setDefault(locale);
    
    // 确保应用数据目录存在
    QString dataDir = NoteDatabase::getDatabaseDir();
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
        qDebug() << "创建应用数据目录: " << dataDir;
    }
    
    // 如果有旧的数据，尝试迁移
    // 检查当前目录下的旧数据
    QDir oldDir(QDir::currentPath() + "/database");
    if (oldDir.exists() && QFile::exists(QDir::currentPath() + "/database/notes.db")) {
        QString oldDbPath = QDir::currentPath() + "/database/notes.db";
        QString newDbPath = NoteDatabase::getDatabasePath();
        
        // 如果新位置没有数据库文件，复制旧数据库
        if (!QFile::exists(newDbPath)) {
            QFile::copy(oldDbPath, newDbPath);
            qDebug() << "从旧位置复制数据库到新位置: " << oldDbPath << " -> " << newDbPath;
            
            // 复制图片文件夹
            QString oldImagesDir = QDir::currentPath() + "/database/images";
            QString newImagesDir = dataDir + "/images";
            if (QDir(oldImagesDir).exists()) {
                QDir().mkpath(newImagesDir);
                
                // 复制所有图片文件
                QDir imagesDir(oldImagesDir);
                QStringList entries = imagesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QString &entry : entries) {
                    QDir().mkpath(newImagesDir + "/" + entry);
                    QDir subDir(oldImagesDir + "/" + entry);
                    QStringList files = subDir.entryList(QDir::Files);
                    for (const QString &file : files) {
                        QFile::copy(oldImagesDir + "/" + entry + "/" + file, 
                                   newImagesDir + "/" + entry + "/" + file);
                    }
                }
                
                qDebug() << "从旧位置复制图片到新位置: " << oldImagesDir << " -> " << newImagesDir;
            }
        }
    }
    
    // 尝试从带组织名的旧路径迁移数据
    QString oldOrgDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                             .replace("/SimpleNote", "/Biubush/SimpleNote");
    
    if (QDir(oldOrgDataDir).exists() && QFile::exists(oldOrgDataDir + "/notes.db")) {
        QString oldOrgDbPath = oldOrgDataDir + "/notes.db";
        QString newDbPath = NoteDatabase::getDatabasePath();
        
        // 如果新位置没有数据库文件，但旧位置(带组织名)有，则进行复制
        if (!QFile::exists(newDbPath)) {
            QFile::copy(oldOrgDbPath, newDbPath);
            qDebug() << "从带组织名的旧位置复制数据库到新位置: " << oldOrgDbPath << " -> " << newDbPath;
            
            // 复制图片文件夹
            QString oldImagesDir = oldOrgDataDir + "/images";
            QString newImagesDir = dataDir + "/images";
            if (QDir(oldImagesDir).exists()) {
                QDir().mkpath(newImagesDir);
                
                // 复制所有图片文件
                QDir imagesDir(oldImagesDir);
                QStringList entries = imagesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QString &entry : entries) {
                    QDir().mkpath(newImagesDir + "/" + entry);
                    QDir subDir(oldImagesDir + "/" + entry);
                    QStringList files = subDir.entryList(QDir::Files);
                    for (const QString &file : files) {
                        QFile::copy(oldImagesDir + "/" + entry + "/" + file, 
                                   newImagesDir + "/" + entry + "/" + file);
                    }
                }
                
                qDebug() << "从带组织名的旧位置复制图片到新位置: " << oldImagesDir << " -> " << newImagesDir;
            }
        }
    }
    
    MainWindow w;
    w.show();
    return a.exec();
}
