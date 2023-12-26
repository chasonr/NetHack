// qt4pre.h

#ifndef QT4PRE_H
#define QT4PRE_H

// Undefine macros that conflict with Qt headers
#undef Invisible
#undef Warning
#undef index
#undef msleep
#undef rindex
#undef wizard
#undef yn
#undef min
#undef max
#undef C

#include <QtCore/QtGlobal>

/* QFontMetrics::width was deprecated in Qt 5.11 */
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
#define QFM_WIDTH(foo) width(foo)
#else
#define QFM_WIDTH(foo) horizontalAdvance(foo)
#endif

#endif
