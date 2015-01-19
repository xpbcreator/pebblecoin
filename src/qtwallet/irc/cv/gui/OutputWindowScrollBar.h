// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// OutputWindowScrollBar is a QScrollBar which provides the capability
// to draw lines within itself; it is intended to be used in conjunction
// with a search feature in the OutputControl, but it's still in the
// experimental phase and not yet completed.

#pragma once

#include <QScrollBar>
#include <QStyle>

namespace cv { namespace gui {

class OutputWindowScrollBar : public QScrollBar
{
    Q_OBJECT

    int             m_currMax;

    // Dictates whether or not the scrollbar has default
    // scrolling functionality.
    bool            m_defaultBehavior;

    // The list of lines to be painted.
    QList<QLineF>   m_searchLines;

public:
    OutputWindowScrollBar(QWidget *pParent = NULL);

    int getSliderHeight();
    void addLine(qreal posRatio);
    void clearLines();
    void setDefaultBehavior(bool db) { m_defaultBehavior = db; }
    bool getDefaultBehavior() { return m_defaultBehavior; }

protected:
    void mousePressEvent(QMouseEvent *event);
    void paintEvent(QPaintEvent *event);

public slots:
    void updateScrollBar(int min, int max);
};

} } // End namespaces
