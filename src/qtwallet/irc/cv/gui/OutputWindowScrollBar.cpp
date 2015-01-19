// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include "cv/gui/OutputWindowScrollBar.h"
#include <QMouseEvent>
#include <QStyleOptionSlider>
#include <QPainter>

namespace cv { namespace gui {

OutputWindowScrollBar::OutputWindowScrollBar(QWidget *pParent/* = NULL*/)
    : QScrollBar(pParent),
      m_defaultBehavior(false)
{
    m_currMax = maximum();
    QObject::connect(this, SIGNAL(rangeChanged(int, int)), this, SLOT(updateScrollBar(int, int)));
}

//-----------------------------------//

int OutputWindowScrollBar::getSliderHeight()
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    QRect sr = style()->subControlRect(QStyle::CC_ScrollBar, &opt,
                                        QStyle::SC_ScrollBarSlider, this);

    return sr.height();
}

//-----------------------------------//

// Adds a line to be painted, located at sliderVal, and repaints with the new line.
void OutputWindowScrollBar::addLine(qreal posRatio)
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    QRect gr = style()->subControlRect(QStyle::CC_ScrollBar, &opt,
                                    QStyle::SC_ScrollBarGroove, this);

    int posY = (int) (posRatio * gr.height()) + gr.y();

    // Create the line and add it.
    QLineF line(0, posY, gr.width(), posY);
    m_searchLines.append(line);

    // Repaint the scrollbar.
    update();
}

//-----------------------------------//

// Clears all the lines, and repaints.
void OutputWindowScrollBar::clearLines()
{
    m_searchLines.clear();
    update();
}

//-----------------------------------//

void OutputWindowScrollBar::mousePressEvent(QMouseEvent *event)
{
    if(!m_defaultBehavior && event->button() == Qt::LeftButton)
    {
        // Copied from QScrollBar.cpp.
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        QRect gr = style()->subControlRect(QStyle::CC_ScrollBar, &opt,
                                        QStyle::SC_ScrollBarGroove, this);
          QRect sr = style()->subControlRect(QStyle::CC_ScrollBar, &opt,
                                              QStyle::SC_ScrollBarSlider, this);
        int sliderMin, sliderMax, sliderLength;
        sliderLength = sr.height();
        sliderMin = gr.y();
        sliderMax = gr.bottom() - sliderLength + 1;

        QStyle::SubControl sc = style()->hitTestComplexControl(QStyle::CC_ScrollBar, &opt, event->pos(), this);
        if(event->y() >= sliderMin && event->y() <= gr.bottom() && sc != QStyle::SC_ScrollBarSlider)
        {
            int val = QStyle::sliderValueFromPosition(minimum(), maximum(), event->y() - sliderMin - (sliderLength / 2),
                                sliderMax - sliderMin, opt.upsideDown);

            setSliderPosition(val);
        }
        else
        {
            // Default action
            QScrollBar::mousePressEvent(event);
        }
    }
    else
    {
        // Default action
        QScrollBar::mousePressEvent(event);
    }
}

//-----------------------------------//

void OutputWindowScrollBar::paintEvent(QPaintEvent *event)
{
    // Paint the default scrollbar.
    QScrollBar::paintEvent(event);

    // Paint all search lines.
    QPainter painter(this);
    QColor color("red");
    painter.setPen(color);

    QStyleOptionSlider opt;
    initStyleOption(&opt);
    QStyle::SubControl sc;
    for(int i = 0; i < m_searchLines.size(); ++i)
    {
        sc = style()->hitTestComplexControl(QStyle::CC_ScrollBar, &opt, m_searchLines[i].p1().toPoint(), this);
        if(sc != QStyle::SC_ScrollBarSlider)
            painter.drawLine(m_searchLines[i]);
    }
}

//-----------------------------------//

// Ensures that the area holding the scrollbar moves it so that
// the bottom of the viewport doesn't move.
void OutputWindowScrollBar::updateScrollBar(int min, int max)
{
    setSliderPosition(max - (m_currMax - value()));
    m_currMax = max;
}

} } // End namespaces
