// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include <QFocusEvent>
#include <QMouseEvent>
#include <QPainter>
#include "cv/gui/CLineEdit.h"

namespace cv { namespace gui {

CLineEdit::CLineEdit(const QString &textToPrint, QWidget *pParent/* = NULL*/)
    : QLineEdit(pParent),
      m_textToPrint(textToPrint),
      m_printText(true)
{ }

//-----------------------------------//

// Returns the ideal size for the line edit.
QSize CLineEdit::sizeHint() const
{
    return QSize(150, 20);
}

//-----------------------------------//

// Imitates firefox search bar behavior.
void CLineEdit::mousePressEvent(QMouseEvent *pEvent)
{
    if(text().size() == selectedText().size())
        deselect();

    QLineEdit::mousePressEvent(pEvent);
}

//-----------------------------------//

void CLineEdit::mouseReleaseEvent(QMouseEvent *pEvent)
{
    if(m_selectAll && !hasSelectedText())
        selectAll();
    else
        QLineEdit::mouseReleaseEvent(pEvent);

    m_selectAll = false;
}

//-----------------------------------//

// Called when the line edit gets focus.
void CLineEdit::focusInEvent(QFocusEvent *pEvent)
{
    if(!hasSelectedText())
        m_selectAll = true;

    m_printText = false;
    QLineEdit::focusInEvent(pEvent);
}

//-----------------------------------//

// Called when the line edit loses focus.
void CLineEdit::focusOutEvent(QFocusEvent *event)
{
    m_printText = true;
    m_selectAll = false;
    if(text().size() > 0)
    {
        QString strText = text();
        int i = 0;
        for(; i < strText.size(); ++i)
        {
            if(!strText[i].isSpace())
            {
                m_printText = false;
                break;
            }
        }

        if(i == strText.size())
        {
            // Every character in the search bar is
            // whitespace, so clear it out.
            clear();
        }
    }

    QLineEdit::focusOutEvent(event);
}

//-----------------------------------//

// Called when the line edit needs to be repainted.
void CLineEdit::paintEvent(QPaintEvent *event)
{
    QLineEdit::paintEvent(event);
    if(m_printText)
    {
        QPainter painter(this);
        QColor color("gray");
        painter.setPen(color);

        painter.drawText(QPointF(5, height()-6), m_textToPrint);
    }
}

} } // End namespaces
