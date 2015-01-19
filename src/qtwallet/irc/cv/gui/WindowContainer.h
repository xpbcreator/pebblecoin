// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// WindowContainer is simply a container which can hold multiple Windows
// using MDI.

#pragma once

#include <QMdiArea>
#include <QMdiSubWindow>

namespace cv { namespace gui {

class Window;

class WindowContainer : public QMdiArea
{
protected:
    QList<Window *> m_windows;

public:
    WindowContainer(QWidget* pParent);
    virtual ~WindowContainer() { }

    int size() const { return m_windows.size(); }
};

} } // End namespaces
