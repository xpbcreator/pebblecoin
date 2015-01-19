// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// CLineEdit is a custom QLineEdit class with some additional features:
//  - Custom grayed text is displayed when the control is empty and not in focus
//  - When there is text in the control and the user clicks inside to bring focus,
//    it selects all text

#pragma once

#include <QLineEdit>

namespace cv { namespace gui {

class CLineEdit : public QLineEdit
{
    QString     m_textToPrint;
    bool        m_printText;
    bool        m_selectAll;

public:
    CLineEdit(const QString &textToPrint, QWidget *pParent = NULL);

    void setPrintedText(const QString &textToPrint) { m_textToPrint = textToPrint; }
    QString getPrintedText() { return m_textToPrint; }

protected:
    // Returns the ideal size for the line edit.
    QSize sizeHint() const;

    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

    void focusInEvent(QFocusEvent *event);
    void focusOutEvent(QFocusEvent *event);

    void paintEvent(QPaintEvent *event);
};

} } // End namespaces
