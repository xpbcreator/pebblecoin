// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// AltWindowContainer is implemented a little differently than a
// WindowContainer; it has a parent QWidget (which sits on the desktop
// at all times) and it uses an event filter to maintain its parent.

#pragma once

#include "cv/gui/WindowContainer.h"

class QMainWindow;
class QDockWidget;
class QListWidget;
class QCloseEvent;
class QMenuBar;

namespace cv { namespace gui {

class WindowManager;

class AltWindowContainer : public WindowContainer
{
    QMainWindow *   m_pParent;

    QDockWidget *   m_pDock;
    QListWidget *   m_pList;
    QMenuBar *      m_pMenuBar;

    WindowManager * m_pManager;

public:
    AltWindowContainer(WindowManager *pManager);

protected:
    void closeEvent(QCloseEvent *event);
};

} } // End namespaces
