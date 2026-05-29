// Copyright (c) Warwick Allison, 1999.
// Qt4 conversion copyright (c) Ray Chason, 2012-2014.
// NetHack may be freely redistributed.  See license for details.

// qt_str.cpp -- some string functions

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <cstdarg>
#include "qt_str.h"

namespace nethack_qt_ {

// Bounded string copy
size_t str_copy(char *dest, const char *src, size_t max)
{
    size_t len = 0;
    if (max != 0) {
        len = strlen(src);
	if (len > max - 1) {
	    len = max - 1;
	}
        memcpy(dest, src, len);
        dest[len] = '\0';
    }
    return len;
}

QString str_titlecase(const QString& str)
{
    if (str == "") { return str; }

    return str.left(1).toUpper() + str.mid(1).toLower();
}

QString nh_capitalize_words(const QString& str)
{
    QStringList words = str.split(" ");
    for (size_t i = 0; i < (size_t) words.size(); ++i) {
	words[i] = str_titlecase(words[i]);
    }
    return words.join(" ");
}

QString
nh_qsprintf(const char *format, ...)
{
    QString msg;
    std::va_list args;

    va_start(args, format);
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
    msg = QString::vasprintf(format, args);
#else
    msg.vsprintf(format, args);
#endif
    va_end(args);
    return msg;
}

} // namespace nethack_qt_
