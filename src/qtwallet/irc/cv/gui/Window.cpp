// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include <QTreeWidgetItem>
#include "cv/gui/Window.h"
#include "cv/gui/WindowManager.h"
#include "cv/gui/WindowContainer.h"

namespace cv { namespace gui {

Window::Window(const QString &title/* = tr("Untitled")*/,
                 const QSize &size/* = QSize(500, 300)*/)
    : QWidget(),
      m_pContainer(NULL),
      m_defSize(size),
      m_pManager(NULL)
{
    // All its children are immediately deleted when it's closed.
    setAttribute(Qt::WA_DeleteOnClose);

    setWindowTitle(title);
    resize(size);
    hide();
}

//-----------------------------------//

QSize Window::sizeHint() const
{
    return m_defSize;
}

//-----------------------------------//

// Returns the title of the window, whether it's in a container or on the desktop.
QString Window::getTitle() const
{
    return windowTitle();
}

//-----------------------------------//

// Sets the title of the window, whether it's in a container or on the desktop.
void Window::setTitle(const QString &title)
{
    if(hasContainer())
    {
        m_pSubWindow->setWindowTitle(title);
    }

    setWindowTitle(title);
}

//-----------------------------------//

// Returns the the short name displayed in the WindowManager.
QString Window::getWindowName()
{
    if(m_pManager)
    {
        QTreeWidgetItem *pItem = m_pManager->getItemFromWindow(this);
        if(pItem)
        {
            return pItem->text(0);
        }
    }

    return "";
}

//-----------------------------------//

// Sets the short name of the window (displayed in the WindowManager).
void Window::setWindowName(const QString &name)
{
    if(m_pManager)
    {
        QTreeWidgetItem *pItem = m_pManager->getItemFromWindow(this);
        if(pItem)
        {
            pItem->setText(0, name);
        }
    }
}

//-----------------------------------//

//
// Overridden event functions.
//
void Window::closeEvent(QCloseEvent *event)
{
    if(m_pManager)
    {
        // int res = FireEvent("OnCloseWindow", EVENT_PRE);
        // if(res == EVENT_CONTINUE)
        // {
        //
            // Close the child windows.
            QList<Window *> cList = m_pManager->getChildWindows(this);

            // Iterate over the children and delete their respective Windows.
            int size = cList.size();
            for(int i = 0; i < size; ++i)
            {
                // We do not use close(), because that will cause a close event to be fired.
                m_pManager->removeWindow(cList[i]);
                if(cList[i]->hasContainer())
                {
                    cList[i]->m_pSubWindow->setWidget(NULL);
                    delete cList[i]->m_pSubWindow;
                }
                delete cList[i];
            }

            // Have the WindowManager close the Window.
            m_pManager->removeWindow(this);
        // }
        //
        // FireEvent("OnCloseWindow", EVENT_POST);
        // if(res == EVENT_BREAK)
        //	event->ignore();
    }

    // Delete the window.
    event->accept();
}

//-----------------------------------//

void Window::resizeEvent(QResizeEvent *event)
{
    m_defSize = event->size();
}

} } // End namespaces

