// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include <QFile>
#include <QTimer>
#include "cv/ConfigManager.h"
#include "cv/gui/Client.h"
#include "cv/gui/WindowManager.h"
#include "cv/gui/AltWindowContainer.h"
#include "cv/gui/InputOutputWindow.h"
#include "cv/gui/StatusWindow.h"
#include "cv/gui/ChannelWindow.h"
#include "cv/gui/QueryWindow.h"

namespace cv {

ConfigManager * g_pCfgManager;
EventManager *  g_pEvtManager;

namespace gui {

Client::Client(const QString &title, const QString &configFileName, const QString &defaultNick)
{
    if (g_pCfgManager || g_pEvtManager)
    {
        throw std::runtime_error("Should only run one cv::gui::Client at a time");
    }
    
    setupEvents();
    setupConfig(configFileName, defaultNick);

    // Initialize the resize timer.
    m_pResizeTimer = new QTimer(this);
    m_pResizeTimer->setSingleShot(true);
    connect(m_pResizeTimer, SIGNAL(timeout()), this, SLOT(updateSizeConfig()));

    setClientSize();
    setWindowTitle(title);

    setupMenu();

    m_pMainContainer = new WindowContainer(this);
    setCentralWidget(m_pMainContainer);

    m_pDock = new QDockWidget(tr("Window Manager"));
    m_pDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_pDock->setFeatures(QDockWidget::DockWidgetMovable);
    loadQSS();

    m_pManager = new WindowManager(m_pDock, m_pMainContainer);
    m_pDock->setWidget(m_pManager);
    m_pDock->setTitleBarWidget(NULL);
    addDockWidget(Qt::LeftDockWidgetArea, m_pDock);

    // Create a StatusWindow on client start.
    onNewIrcServerWindow();
}

//-----------------------------------//

Client::~Client()
{
    delete g_pCfgManager;
    delete g_pEvtManager;
}

//-----------------------------------//

void Client::closeEvent(QCloseEvent *)
{
    disconnect(this, SLOT(updateSizeConfig()));

    // Write all the config to the file.
    hide();
    g_pCfgManager->writeToDefaultFile();

    delete m_pManager;
    delete m_pDock;
    deleteLater();
}

//-----------------------------------//

void Client::resizeEvent(QResizeEvent *)
{
    m_pResizeTimer->start(150);
}

//-----------------------------------//

void Client::setupEvents()
{
    g_pEvtManager = new EventManager;
    g_pEvtManager->createEvent("input");
    g_pEvtManager->createEvent("output");
    g_pEvtManager->createEvent("doubleClickedLink");
}

//-----------------------------------//

void Client::setupMenu()
{
    m_pFileMenu = menuBar()->addMenu(tr("&File"));

    QAction *pNewIrcWinAct = m_pFileMenu->addAction(tr("&New IRC Server"));
    QList<QKeySequence> newServerShortcuts;
    newServerShortcuts.append(QKeySequence("Ctrl+T"));
    pNewIrcWinAct->setShortcuts(newServerShortcuts);

    m_pFileMenu->addSeparator();

    QAction *pRefreshQSS = m_pFileMenu->addAction(tr("Refresh QSS"));
    QList<QKeySequence> refreshShortcuts;
    refreshShortcuts.append(QKeySequence("Ctrl+R"));
    pRefreshQSS->setShortcuts(refreshShortcuts);

    QObject::connect(pNewIrcWinAct, SIGNAL(triggered()), this, SLOT(onNewIrcServerWindow()));
    QObject::connect(pRefreshQSS, SIGNAL(triggered()), this, SLOT(loadQSS()));

    m_pFileMenu->addSeparator();
}

//-----------------------------------//

void Client::setupConfig(const QString &configFileName, const QString &defaultNick)
{
    g_pCfgManager = new ConfigManager(configFileName);

    QMap<QString, ConfigOption> defOptions;

    setupColorConfig(defOptions);
    setupServerConfig(defOptions, defaultNick);
    setupGeneralConfig(defOptions);
    setupMessagesConfig(defOptions);
    StatusWindow::setupIRCConfig(defOptions);
    
    g_pCfgManager->setupDefaultConfigFile(defOptions);
}

//-----------------------------------//

void Client::setupColorConfig(QMap<QString, ConfigOption> &defOptions)
{
    WindowManager::setupColorConfig(defOptions);
    InputOutputWindow::setupColorConfig(defOptions);
    OutputControl::setupColorConfig(defOptions);
    ChannelWindow::setupColorConfig(defOptions);
}

//-----------------------------------//

void Client::setupServerConfig(QMap<QString, ConfigOption> &defOptions, const QString &defaultNick)
{
    ServerConnectionPanel::setupServerConfig(defOptions, defaultNick);
    StatusWindow::setupServerConfig(defOptions);
}

//-----------------------------------//

void Client::setupGeneralConfig(QMap<QString, ConfigOption> &defOptions)
{
    defOptions.insert("client.width", ConfigOption(1000, CONFIG_TYPE_INTEGER));
    defOptions.insert("client.height", ConfigOption(500, CONFIG_TYPE_INTEGER));
    defOptions.insert("client.maximized", ConfigOption(false, CONFIG_TYPE_BOOLEAN));

    defOptions.insert("timestamp",         ConfigOption(false, CONFIG_TYPE_BOOLEAN));
    defOptions.insert("timestamp.format",  ConfigOption("[hh:mm:ss]"));
}

//-----------------------------------//

void Client::setupMessagesConfig(QMap<QString, ConfigOption> &defOptions)
{
    // Regular messages
    defOptions.insert("message.action",        ConfigOption("* %1 %2"));
    defOptions.insert("message.ctcp",          ConfigOption("[CTCP %1 (from %2)]"));
    defOptions.insert("message.connecting",    ConfigOption("* Connecting to %1 (%2)"));
    defOptions.insert("message.connectFailed", ConfigOption("* Failed to connect to server (%1)"));
    defOptions.insert("message.disconnected",  ConfigOption("* Disconnected"));
    defOptions.insert("message.invite",        ConfigOption("* %1 has invited you to %2"));
    defOptions.insert("message.join",          ConfigOption("* %1 (%2) has joined %3"));
    defOptions.insert("message.join.self",     ConfigOption("* You have joined %1"));
    defOptions.insert("message.kick",          ConfigOption("* %1 was kicked by %2"));
    defOptions.insert("message.kick.self",     ConfigOption("* You were kicked by %1"));
    defOptions.insert("message.rejoin",        ConfigOption("* You have rejoined %1"));
    defOptions.insert("message.mode",          ConfigOption("* %1 has set mode: %2"));
    defOptions.insert("message.nick",          ConfigOption("* %1 is now known as %2"));
    defOptions.insert("message.notice",        ConfigOption("-%1- %2"));
    defOptions.insert("message.part",          ConfigOption("* %1 (%2) has left %3"));
    defOptions.insert("message.part.self",     ConfigOption("* You have left %1"));
    defOptions.insert("message.pong",          ConfigOption("* PONG from %1: %2"));
    defOptions.insert("message.quit",          ConfigOption("* %1 (%2) has quit"));
    defOptions.insert("message.reason",        ConfigOption(" (%1%2)"));
    defOptions.insert("message.say",           ConfigOption("<%1> %2"));
    defOptions.insert("message.topic",         ConfigOption("* %1 changes topic to: %2"));
    defOptions.insert("message.wallops",       ConfigOption("* WALLOPS from %1: %2"));

    // Numeric messages
    defOptions.insert("message.301",           ConfigOption("%1 is away: %2"));
    defOptions.insert("message.317",           ConfigOption("%1 has been idle %2"));
    defOptions.insert("message.330",           ConfigOption("%1 %2: %3"));
    defOptions.insert("message.332",           ConfigOption("* Topic is: %1"));
    defOptions.insert("message.333.status",    ConfigOption("%1 topic set by %2 on %3 %4"));
    defOptions.insert("message.333.channel",   ConfigOption("* Topic set by %1 on %2 %3"));
}

//-----------------------------------//

void Client::setClientSize()
{
    int width = GET_INT("client.width");
    int height = GET_INT("client.height");
    resize(width, height);

    if(GET_STRING("client.maximized").compare("1") == 0)
        showMaximized();
}

//-----------------------------------//

// Creates a new StatusWindow.
void Client::onNewIrcServerWindow()
{
    StatusWindow *pWin = new StatusWindow();
    m_pManager->addWindow(pWin);
    pWin->showMaximized();
}

//-----------------------------------//

// This function is called when the resize timer times out
// (when it's time to update the config options holding client size).
void Client::updateSizeConfig()
{
    // We only want to overwrite the client size if it's
    // in the normal window state (not maximized).
    bool maximized;
    if(!isMaximized())
    {
        g_pCfgManager->setOptionValue("client.width", width(), false);
        g_pCfgManager->setOptionValue("client.height", height(), false);
        maximized = false;
    }
    else
        maximized = true;
    g_pCfgManager->setOptionValue("client.maximized", maximized, false);
    g_pCfgManager->writeToDefaultFile();
}

//-----------------------------------//

void Client::loadQSS()
{
    QFile qss("conviersa.qss");
    qss.open(QIODevice::ReadOnly);
    if(qss.isOpen())
    {
        qApp->setStyleSheet(qss.readAll());
        qss.close();
    }
}

} } // End namespaces
