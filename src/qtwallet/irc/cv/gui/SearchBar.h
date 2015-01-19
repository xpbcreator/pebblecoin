// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// SearchBar is a derivation of CLineEdit that provides the capability
// to display a progress bar within the control itself, similar to the
// address bar control in Apple's Safari.

#pragma once

#include "cv/gui/CLineEdit.h"

class QFocusEvent;
class QPaintEvent;
class QBrush;

namespace cv { namespace gui {

class SearchBar : public CLineEdit
{
    bool        m_printText;
    bool        m_selectAll;

    QBrush *    m_pBrush;

    // Value can be anywhere between 0.0 and 1.0.
    double      m_searchProgress;

public:
    SearchBar(QWidget *pParent = NULL);

    void updateProgress(double newProgress);
    void clearProgress();

protected:
    void paintEvent(QPaintEvent *event);
};

} } // End namespaces
