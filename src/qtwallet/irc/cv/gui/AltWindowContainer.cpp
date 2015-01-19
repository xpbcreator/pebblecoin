// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include <QMainWindow>
#include <QDockWidget>
#include <QListWidget>
#include <QCloseEvent>
#include <QMenuBar>
#include <QMenu>
#include "cv/gui/AltWindowContainer.h"
#include "cv/gui/WindowManager.h"

namespace cv { namespace gui {

AltWindowContainer::AltWindowContainer(WindowManager *pManager)
    : WindowContainer(NULL),
      m_pManager(pManager)
{
    m_pParent = new QMainWindow(NULL);
    setParent(m_pParent);

    m_pList = new QListWidget;
    m_pDock = new QDockWidget("Windows");
    m_pDock->setWidget(m_pList);
    m_pDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_pParent->setCentralWidget(this);
    m_pParent->addDockWidget(Qt::LeftDockWidgetArea, m_pDock);

    QMenu *pFileMenu = m_pParent->menuBar()->addMenu("File");
    pFileMenu->addAction("hello");
    pFileMenu->addSeparator();
    pFileMenu->addAction("oh hi there");

    m_pParent->show();
}

//-----------------------------------//

void AltWindowContainer::closeEvent(QCloseEvent *event)
{
    if(m_pManager)
    {
        m_pManager->removeContainer(this);
    }

    m_pParent->setCentralWidget(NULL);
    delete m_pParent;

    event->accept();
}

} } // End namespaces
