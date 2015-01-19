// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include "cv/qext.h"

namespace cv {

QString escapeEx(const QString &text)
{
    QString textToReturn = Qt::escape(text);
    textToReturn.prepend("<span style=\"white-space:pre-wrap\">");
    textToReturn.append("</span>");
    return textToReturn;
}

} // End namespace
