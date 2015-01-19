// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// StatusWindow is the main Window that interfaces with an IRC server;
// it owns the Session instance and so is necessary to stay connected
// to the server.

#pragma once

#include "cv/Parser.h"
#include "cv/gui/InputOutputWindow.h"
#include "cv/gui/ServerConnectionPanel.h"

namespace cv { namespace gui {

class ChannelListWindow;
class ChannelWindow;
class QueryWindow;

class StatusWindow : public InputOutputWindow
{
    Q_OBJECT

protected:
    bool                    m_populatingUserList;
    bool                    m_sentListStopMsg;

    ChannelListWindow *     m_pChanListWin;
    QList<ChannelWindow *>  m_chanList;
    QList<QueryWindow *>    m_privList;

public:
    StatusWindow(const QString &title = tr("Server Window"),
                 const QSize &size = QSize(500, 300));
    ~StatusWindow();

    OutputWindow *getChildIrcWindow(const QString &name);

    // Returns true if the child window with the provided name
    // exists, false otherwise.
    bool childIrcWindowExists(const QString &name) { return (getChildIrcWindow(name) != NULL); }

    QList<ChannelWindow *> getChannels();
    QList<QueryWindow *> getPrivateMessages();
    void addChannelWindow(ChannelWindow *pChan);
    void addQueryWindow(QueryWindow *pPriv, bool giveFocus);

    // Event callbacks
    void onServerConnecting(Event *pEvent);
    void onServerConnectFailed(Event *pEvent);
    void onServerConnect(Event *pEvent);
    void onServerDisconnect(Event *pEvent);
    void onErrorMessage(Event *pEvent);
    void onInviteMessage(Event *pEvent);
    void onJoinMessage(Event *pEvent);
    void onModeMessage(Event *pEvent);
    void onNickMessage(Event *pEvent);
    void onPongMessage(Event *pEvent);
    void onPrivmsgMessage(Event *pEvent);
    void onQuitMessage(Event *pEvent);
    void onWallopsMessage(Event *pEvent);
    void onNumericMessage(Event *pEvent);
    void onUnknownMessage(Event *pEvent);

    void onOutput(Event *pEvent);
    void onDoubleClickLink(Event *pEvent);

    static void setupServerConfig(QMap<QString, ConfigOption> &defOptions);
    static void setupIRCConfig(QMap<QString, ConfigOption> &defOptions);

protected:
    void handleSay(const QString &text);
    void handleAction(const QString &text);
    void handleTab();

    bool eventFilter(QObject *obj, QEvent *event);

private:
    // Numeric messages
    void handle321Numeric(const Message &msg);
    void handle322Numeric(const Message &msg);
    void handle323Numeric(const Message &msg);
    void handle353Numeric(const Message &msg);
    void handle366Numeric(const Message &msg);

public slots:
    void removeChannelWindow(ChannelWindow *pChanWin);
    void removeQueryWindow(QueryWindow *pChanWin);
    void connectToServer(QString server, int port, QString name, QString nick, QString altNick);
};

} } // End namespaces
