#ifndef QWEBDAV_GLOBAL_H
#define QWEBDAV_GLOBAL_H

#include <QtCore/qglobal.h>

// 添加库定义，使QTWEBDAV_EXPORT展开为Q_DECL_EXPORT
#define QTWEBDAV_LIBRARY

#if defined(QTWEBDAV_LIBRARY)
#  define QTWEBDAV_EXPORT Q_DECL_EXPORT
#else
#  define QTWEBDAV_EXPORT Q_DECL_IMPORT
#endif

#endif // QWEBDAV_GLOBAL_H 