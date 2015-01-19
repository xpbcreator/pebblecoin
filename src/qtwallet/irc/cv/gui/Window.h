// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// Window is the base class for all windows within Conviersa. Each one
// is managed in an MDI by the WindowManager and WindowContainer classes.
//
// Current Window hierarchy (+ indicates an abstract class):
//
// + Window
//   - ChannelListWindow
//   + OutputWindow
//     - DebugWindow
//     + InputOutputWindow
//       - StatusWindow
//       - ChannelWindow
//       - QueryWindow

#pragma once

#include <QMdiSubWindow>

namespace cv { namespace gui {

class WindowManager;
class WindowContainer;

class Window : public QWidget
{
    friend class WindowManager;
    friend class WindowContainer;

protected:
    // We need a pointer to the current container instance
    // to be able to destroy it when there are no more
    // tabs in a desktop window.
    WindowContainer *   m_pContainer;

    // This is used to maintain the default size of the window.
    QSize               m_defSize;

    // We need this pointer so every Window instance has the ability
    // to manipulate other Window instances (through the WindowManager).
    WindowManager *     m_pManager;

    // We need this so we can put our Window inside a WindowContainer
    // (it has to be inside a QMdiSubWindow so everything will work right).
    QMdiSubWindow *     m_pSubWindow;

public:
    Window(const QString &title = tr("Untitled"),
            const QSize &size = QSize(500, 300));
    virtual ~Window() { }

    QSize sizeHint() const;

    QString getTitle() const;
    void setTitle(const QString &title);

    QString getWindowName();
    void setWindowName(const QString &name);

    bool hasContainer() const { return m_pContainer != NULL; }

    // Called when the user clicks the corresponding item in the WindowManager.
    virtual void giveFocus() = 0;

    // Called when the corresponding item in the WindowManager becomes focused.
    virtual void focusedInTree() { }

protected:
    virtual void closeEvent(QCloseEvent *event);
    void resizeEvent(QResizeEvent *event);
};

} } // End namespaces
