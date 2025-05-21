#include "mainwindow.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QFontDatabase>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // 设置应用程序信息
    QApplication::setApplicationName("SimpleNote");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("Biubush");
    QApplication::setOrganizationDomain("biubush.com");
    
    // 设置应用程序图标
    QApplication::setWindowIcon(QIcon(":/icons/simplenote.png"));
    
    // 设置本地化
    QLocale locale = QLocale::system();
    QLocale::setDefault(locale);
    
    // 确保数据库目录存在
    QDir dir;
    if (!dir.exists("database")) {
        dir.mkdir("database");
    }
    
    MainWindow w;
    w.show();
    return a.exec();
}
