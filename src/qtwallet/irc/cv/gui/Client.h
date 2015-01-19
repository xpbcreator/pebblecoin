// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// Client is the main class of Conviersa. It is the main window of the client,
// and constructs the EventManager and ConfigManager, and owns the WindowManager.

#pragma once

#include <QMainWindow>
#include <QString>
#include <QMenu>
#include <QMenuBar>
#include <QDockWidget>

class QTreeWidgetItem;
class QTimer;

namespace cv {

struct ConfigOption;

namespace gui {

class WindowManager;
class WindowContainer;

class Client : public QMainWindow
{
    Q_OBJECT

private:
    WindowContainer *   m_pMainContainer;
    WindowManager *     m_pManager;

    QMenu *             m_pFileMenu;
    QDockWidget *       m_pDock;

    // This timer is used to find the last resize event
    // in a set of resize events that occur in quick succession -
    // like when dragging the frame to resize the window - so
    // that we can find the updated client size.
    QTimer *            m_pResizeTimer;

public:
    Client(const QString &title, const QString &configFileName, const QString &defaultNick);
    ~Client();
    
protected:
    void closeEvent(QCloseEvent *);
    void resizeEvent(QResizeEvent *pEvent);

private:
    void setupEvents();
    void setupMenu();
    void setupConfig(const QString &configFileName, const QString &defaultNick);

    void setupColorConfig(QMap<QString, ConfigOption> &defOptions);
    void setupServerConfig(QMap<QString, ConfigOption> &defOptions, const QString &defaultNick);
    void setupGeneralConfig(QMap<QString, ConfigOption> &defOptions);
    void setupMessagesConfig(QMap<QString, ConfigOption> &defOptions);

    void setClientSize();
  
public slots:
    void onNewIrcServerWindow();
    void updateSizeConfig();
    void loadQSS();
};

} } // End namespaces
