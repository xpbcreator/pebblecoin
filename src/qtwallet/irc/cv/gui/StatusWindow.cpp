// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include <QAbstractSocket>
#include <QDateTime>
#include "cv/qext.h"
#include "cv/Session.h"
#include "cv/ConfigManager.h"
#include "cv/gui/WindowManager.h"
#include "cv/gui/StatusWindow.h"
#include "cv/gui/ChannelListWindow.h"
#include "cv/gui/ChannelWindow.h"
#include "cv/gui/QueryWindow.h"
#include "cv/gui/OutputWindowScrollBar.h"
#include "cv/gui/OutputControl.h"
#include "cv/gui/OverlayPanel.h"

#define DEBUG_MESSAGES 0

namespace cv { namespace gui {

StatusWindow::StatusWindow(const QString &title/* = tr("Server Window")*/,
                           const QSize &size/* = QSize(500, 300)*/)
  : InputOutputWindow(title, size),
    m_populatingUserList(false),
    m_pChanListWin(NULL)
{
    m_pVLayout->addWidget(m_pOutput);
    m_pVLayout->addWidget(m_pInput);
    m_pVLayout->setSpacing(5);
    m_pVLayout->setContentsMargins(2, 2, 2, 2);
    setLayout(m_pVLayout);

    m_pSharedServerConnPanel = new ServerConnectionPanel(m_pOutput);
    QObject::connect(m_pSharedServerConnPanel.data(), SIGNAL(connect(QString,int,QString,QString,QString)),
                     this, SLOT(connectToServer(QString,int,QString,QString,QString)),
                     Qt::QueuedConnection);
    m_pOpenButton = m_pSharedServerConnPanel->addOpenButton(m_pOutput, "Connect", 80, 30);
    m_pOutput->installEventFilter(this);

    m_pSession = new Session("conviersa");
    g_pEvtManager->hookEvent("connecting",     m_pSession, MakeDelegate(this, &StatusWindow::onServerConnecting));
    g_pEvtManager->hookEvent("connectFailed",  m_pSession, MakeDelegate(this, &StatusWindow::onServerConnectFailed));
    g_pEvtManager->hookEvent("connected",      m_pSession, MakeDelegate(this, &StatusWindow::onServerConnect));
    g_pEvtManager->hookEvent("disconnected",   m_pSession, MakeDelegate(this, &StatusWindow::onServerDisconnect));
    g_pEvtManager->hookEvent("errorMessage",   m_pSession, MakeDelegate(this, &StatusWindow::onErrorMessage));
    g_pEvtManager->hookEvent("inviteMessage",  m_pSession, MakeDelegate(this, &StatusWindow::onInviteMessage));
    g_pEvtManager->hookEvent("joinMessage",    m_pSession, MakeDelegate(this, &StatusWindow::onJoinMessage));
    g_pEvtManager->hookEvent("modeMessage",    m_pSession, MakeDelegate(this, &StatusWindow::onModeMessage));
    g_pEvtManager->hookEvent("nickMessage",    m_pSession, MakeDelegate(this, &StatusWindow::onNickMessage));
    g_pEvtManager->hookEvent("noticeMessage",  m_pSession, MakeDelegate(this, &InputOutputWindow::onNoticeMessage));
    g_pEvtManager->hookEvent("pongMessage",    m_pSession, MakeDelegate(this, &StatusWindow::onPongMessage));
    g_pEvtManager->hookEvent("privmsgMessage", m_pSession, MakeDelegate(this, &StatusWindow::onPrivmsgMessage));
    g_pEvtManager->hookEvent("quitMessage",    m_pSession, MakeDelegate(this, &StatusWindow::onQuitMessage));
    g_pEvtManager->hookEvent("wallopsMessage", m_pSession, MakeDelegate(this, &StatusWindow::onWallopsMessage));
    g_pEvtManager->hookEvent("numericMessage", m_pSession, MakeDelegate(this, &StatusWindow::onNumericMessage));
    g_pEvtManager->hookEvent("unknownMessage", m_pSession, MakeDelegate(this, &StatusWindow::onUnknownMessage));
}

//-----------------------------------//

StatusWindow::~StatusWindow()
{
    delete m_pSession;
    m_pSharedServerConnPanel.reset();
}

//-----------------------------------//

// Returns a pointer to the OutputWindow if it exists and is
// a child of this StatusWindow, NULL otherwise.
OutputWindow *StatusWindow::getChildIrcWindow(const QString &name)
{
    for(int i = 0; i < m_chanList.size(); ++i)
        if(name.compare(m_chanList[i]->getWindowName(), Qt::CaseInsensitive) == 0)
            return m_chanList[i];

    for(int i = 0; i < m_privList.size(); ++i)
        if(name.compare(m_privList[i]->getWindowName(), Qt::CaseInsensitive) == 0)
            return m_privList[i];

    return NULL;
}

//-----------------------------------//

// Returns a list of all ChannelWindows that are currently
// being managed in the server.
QList<ChannelWindow *> StatusWindow::getChannels()
{
    return m_chanList;
}

//-----------------------------------//

// Returns a list of all QueryWindows that are currently
// being managed in the server.
QList<QueryWindow *> StatusWindow::getPrivateMessages()
{
    return m_privList;
}

//-----------------------------------//

// Adds a channel to the list.
void StatusWindow::addChannelWindow(ChannelWindow *pChanWin)
{
    if(m_pManager)
        m_pManager->addWindow(pChanWin, m_pManager->getItemFromWindow(this));
    m_chanList.append(pChanWin);
    QObject::connect(pChanWin, SIGNAL(chanWindowClosing(ChannelWindow *)),
                this, SLOT(removeChannelWindow(ChannelWindow *)));
}

//-----------------------------------//

// Removes a channel from the list.
void StatusWindow::removeChannelWindow(ChannelWindow *pChanWin)
{
    m_chanList.removeOne(pChanWin);
}

//-----------------------------------//

// Adds a PM window to the list.
void StatusWindow::addQueryWindow(QueryWindow *pQueryWin, bool giveFocus)
{
    if(m_pManager)
        m_pManager->addWindow(pQueryWin, m_pManager->getItemFromWindow(this), giveFocus);
    m_privList.append(pQueryWin);
    QObject::connect(pQueryWin, SIGNAL(privWindowClosing(QueryWindow *)),
                this, SLOT(removeQueryWindow(QueryWindow *)));
}

//-----------------------------------//

// Removes a PM window from the list.
void StatusWindow::removeQueryWindow(QueryWindow *pPrivWin)
{
    m_privList.removeOne(pPrivWin);
}

//-----------------------------------//

void StatusWindow::connectToServer(QString server, int port, QString name, QString nick, QString altNick)
{
    m_pSession->connectToServer(server, port, name, nick);
}

//-----------------------------------//

// Handles the printing/sending of the PRIVMSG message.
void StatusWindow::handleSay(const QString &text)
{
    printError("Cannot send to status window");
}

//-----------------------------------//

// Handles the printing/sending of the PRIVMSG ACTION message.
void StatusWindow::handleAction(const QString &text)
{
    printError("Cannot send to status window");
}

//-----------------------------------//

void StatusWindow::handleTab()
{
    //QString text = getInputText();
    //int idx = m_pInput->textCursor().position();
}

//-----------------------------------//

// Handles child widget events.
bool StatusWindow::eventFilter(QObject *obj, QEvent *event)
{
    // Just for monitoring when it's closed.
    if(obj == m_pChanListWin && event->type() == QEvent::Close)
        m_pChanListWin = NULL;

    return InputOutputWindow::eventFilter(obj, event);
}

//-----------------------------------//

void StatusWindow::handle321Numeric(const Message &)
{
    if(!m_pChanListWin)
    {
        m_pChanListWin = new ChannelListWindow(m_pSession);

        m_pManager->addWindow(m_pChanListWin, m_pManager->getItemFromWindow(this));
        QString title = QString("Channel List - %1").arg(getWindowName());
        m_pChanListWin->setTitle(title);

        // TODO (seand): Fix and put inside my own event system.
        m_pChanListWin->installEventFilter(this);
    }
    else
    {
        m_pChanListWin->clearList();
        QString title = QString("Channel List - %1").arg(getWindowName());
        m_pChanListWin->setTitle(title);
    }

    m_pChanListWin->beginPopulatingList();
}

//-----------------------------------//

void StatusWindow::handle322Numeric(const Message &msg)
{
    // msg.m_params[0]: my nick
    // msg.m_params[1]: channel
    // msg.m_params[2]: number of users
    // msg.m_params[3]: topic
    if(m_pChanListWin)
    {
        m_pChanListWin->addChannel(msg.m_params[1], msg.m_params[2], msg.m_params[3]);
    }
    else
    {
        if(!m_sentListStopMsg)
        {
            m_sentListStopMsg = true;
            m_pSession->sendData("LIST STOP");
        }
    }
}

//-----------------------------------//

void StatusWindow::handle323Numeric(const Message &)
{
    if(m_pChanListWin)
        m_pChanListWin->endPopulatingList();

    m_sentListStopMsg = false;
}

//-----------------------------------//

void StatusWindow::handle353Numeric(const Message &msg)
{
    // msg.m_params[0]: my nick
    // msg.m_params[1]: "=" | "*" | "@"
    // msg.m_params[2]: channel
    // msg.m_params[3]: names, separated by spaces
    ChannelWindow *pChanWin = DCAST(ChannelWindow, getChildIrcWindow(msg.m_params[2]));

    // RPL_NAMREPLY was sent as a result of a NAMES command.
    if(pChanWin == NULL)
    {
        printOutput(getNumericText(msg), MESSAGE_IRC_NUMERIC);
    }
}

//-----------------------------------//

void StatusWindow::handle366Numeric(const Message &msg)
{
    // msg.m_params[0]: my nick
    // msg.m_params[1]: channel
    // msg.m_params[2]: "End of NAMES list"
    //
    // RPL_ENDOFNAMES was sent as a result of a NAMES command.
    if(!childIrcWindowExists(msg.m_params[1]))
        printOutput(getNumericText(msg), MESSAGE_IRC_NUMERIC);
}

//-----------------------------------//

void StatusWindow::onServerConnecting(Event *)
{
    QString textToPrint = GET_STRING("message.connecting")
                            .arg(m_pSession->getHost())
                            .arg(QString::number(m_pSession->getPort()));
    printOutput(textToPrint, MESSAGE_INFO);
}

//-----------------------------------//

void StatusWindow::onServerConnectFailed(Event *pEvt)
{
    ConnectionEvent *pConnectionEvt = DCAST(ConnectionEvent, pEvt);
    QAbstractSocket::SocketError error = pConnectionEvt->error();
    QString reason;
    switch(error)
    {
        case QAbstractSocket::ConnectionRefusedError:
            reason = "Connection refused from host";
            break;
        case QAbstractSocket::RemoteHostClosedError:
            reason = "Remote host closed connection";
            break;
        case QAbstractSocket::HostNotFoundError:
            reason = "Host could not be resolved";
            break;
        case QAbstractSocket::SocketAccessError:
            reason = "Lack of privileges";
            break;
        case QAbstractSocket::SocketResourceError:
            reason = "Ran out of resources";
            break;
        case QAbstractSocket::SocketTimeoutError:
            reason = "Connection timed out";
            break;
        default:
            reason = QString("Unknown connection error: ") + error;
    }

    QString textToPrint = GET_STRING("message.connectFailed")
                            .arg(reason);
    printOutput(textToPrint, MESSAGE_INFO);
}

//-----------------------------------//

void StatusWindow::onServerConnect(Event *pEvent)
{
    m_pSharedServerConnPanel->close();
    m_pInput->setFocus();

    ConnectionEvent *pConnEvent = DCAST(ConnectionEvent, pEvent);
    QString host = pConnEvent->getHost();

    // Get the recent servers list.
    QVariantList serverList = g_pCfgManager->getOptionValue("recent.servers").toList();
    for(int i = 0; i < serverList.size(); ++i)
    {
        // If the server is already in the list, remove it.
        if(serverList[i].toString().compare(host, Qt::CaseInsensitive) == 0)
        {
            serverList.removeAt(i);
            break;
        }
    }

    // Prepend the server.
    serverList.prepend(host);
    if(serverList.size() > 10)
        serverList.removeLast();

    // Update the recent servers list.
    g_pCfgManager->setOptionValue("recent.servers", serverList, false);
    g_pCfgManager->writeToDefaultFile();
}

//-----------------------------------//

void StatusWindow::onServerDisconnect(Event *)
{
    printOutput(GET_STRING("message.disconnected"), MESSAGE_INFO);
    setTitle("Server Window");
    setWindowName("Server Window");
    m_pSharedServerConnPanel->open();
}

//-----------------------------------//

void StatusWindow::onNumericMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();
    switch(msg.m_command)
    {
        case 1:
        {
            // Change the name of the window to the name of the network.
            QString networkName = getNetworkNameFrom001(msg);
            setTitle(networkName);
            setWindowName(networkName);

            break;
        }
        // RPL_AWAY
        case 301:
        {
            // msg.m_params[0]: my nick
            // msg.m_params[1]: nick
            // msg.m_params[2]: away message
            QString textToPrint = GET_STRING("message.301")
                                  .arg(msg.m_params[1])
                                  .arg(msg.m_params[2]);
            printOutput(textToPrint, MESSAGE_IRC_NUMERIC);

            break;
        }
        /*// RPL_USERHOST
        case 302:
        {

            break;
        }
        // RPL_ISON
        case 303:
        {

            break;
        }*/
        // RPL_WHOISIDLE
        case 317:
        {
            QString textToPrint = GET_STRING("message.317")
                                  .arg(msg.m_params[1])
                                  .arg(getIdleTextFrom317(msg));
            printOutput(textToPrint, MESSAGE_IRC_NUMERIC);
            break;
        }
        // RPL_LISTSTART
        case 321:
        {
            handle321Numeric(msg);
            break;
        }
        // RPL_LIST
        case 322:
        {
            handle322Numeric(msg);
            break;
        }
        // RPL_LISTEND
        case 323:
        {
            handle323Numeric(msg);
            break;
        }
        // RPL_WHOISACCOUNT
        case 330:
        {
            // msg.m_params[0]: my nick
            // msg.m_params[1]: nick
            // msg.m_params[2]: login/auth
            // msg.m_params[3]: "is logged in as"
            QString textToPrint = GET_STRING("message.330")
                                  .arg(msg.m_params[1])
                                  .arg(msg.m_params[3])
                                  .arg(msg.m_params[2]);
            printOutput(textToPrint, MESSAGE_IRC_NUMERIC);
            break;
        }
        // RPL_TOPIC
        case 332:
        {
            // msg.m_params[0]: my nick
            // msg.m_params[1]: channel
            // msg.m_params[2]: topic
            if(!childIrcWindowExists(msg.m_params[1]))
                printOutput(getNumericText(msg), MESSAGE_IRC_NUMERIC);
            break;
        }
        // States when topic was last set.
        case 333:
        {
            // msg.m_params[0]: my nick
            // msg.m_params[1]: channel
            // msg.m_params[2]: nick
            // msg.m_params[3]: unix time
            if(!childIrcWindowExists(msg.m_params[1]))
            {
                QString textToPrint = GET_STRING("message.333.status")
                                      .arg(msg.m_params[1])
                                      .arg(msg.m_params[2])
                                      .arg(getDate(msg.m_params[3]))
                                      .arg(getTime(msg.m_params[3]));
                printOutput(textToPrint, MESSAGE_IRC_NUMERIC);
            }

            break;
        }
        // RPL_NAMREPLY
        case 353:
        {
            handle353Numeric(msg);
            break;
        }
        // RPL_ENDOFNAMES
        case 366:
        {
            handle366Numeric(msg);
            break;
        }
        case 401:   // ERR_NOSUCKNICK
        case 404:   // ERR_CANNOTSENDTOCHAN
        {
            // msg.m_params[0]: my nick
            // msg.m_params[1]: nick/channel
            // msg.m_params[2]: "No such nick/channel"
            if(!childIrcWindowExists(msg.m_params[1]))
            {
                printOutput(getNumericText(msg), MESSAGE_IRC_NUMERIC);
            }
            break;
        }
        default:
        {
            // The following are not meant to be handled,
            // but only printed:
            // 	003
            // 	004
            // 	305
            // 	306
            printOutput(getNumericText(msg), MESSAGE_IRC_NUMERIC);
        }
    }
}

//-----------------------------------//

void StatusWindow::onErrorMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();
    printOutput(msg.m_params[0], MESSAGE_IRC_ERROR);
}

//-----------------------------------//

void StatusWindow::onInviteMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();
    QString textToPrint = GET_STRING("message.invite")
                          .arg(parseMsgPrefix(msg.m_prefix, MsgPrefixName))
                          .arg(msg.m_params[1]);
    printOutput(textToPrint, MESSAGE_IRC_INVITE);
}

//-----------------------------------//

void StatusWindow::onJoinMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();

    QString nickJoined = parseMsgPrefix(msg.m_prefix, MsgPrefixName);
    if(m_pSession->isMyNick(nickJoined) && !childIrcWindowExists(msg.m_params[0]))
    {
        // Create the channel and post the message to it.
        ChannelWindow *pChanWin = new ChannelWindow(m_pSession, m_pSharedServerConnPanel, msg.m_params[0]);
        addChannelWindow(pChanWin);
        QString textToPrint = GET_STRING("message.join.self").arg(msg.m_params[0]);
        pChanWin->printOutput(textToPrint, MESSAGE_IRC_JOIN);
    }
}

//-----------------------------------//

void StatusWindow::onModeMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();
    if(!childIrcWindowExists(msg.m_params[0]))  // user mode
    {
        // Ignore the first parameter.
        QString modes = msg.m_params[1];
        for(int i = 2; i < msg.m_paramsNum; ++i)
            modes += ' ' + msg.m_params[i];

        QString textToPrint = GET_STRING("message.mode")
                                .arg(parseMsgPrefix(msg.m_prefix, MsgPrefixName))
                                .arg(modes);

        printOutput(textToPrint, MESSAGE_IRC_MODE);
    }
}

//-----------------------------------//

void StatusWindow::onNickMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();

    QString oldNick = parseMsgPrefix(msg.m_prefix, MsgPrefixName);
    if(m_pSession->isMyNick(oldNick))
    {
        QString textToPrint = GET_STRING("message.nick")
                              .arg(oldNick)
                              .arg(msg.m_params[0]);
        printOutput(textToPrint, MESSAGE_IRC_NICK);
    }
}

//-----------------------------------//

void StatusWindow::onPongMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();

    // The prefix is used to determine the server that
    // sends the PONG (instead of using the first parameter),
    // because it will always have the server in it.
    //
    // Example:
    //	PING hi :there
    //	:irc.server.net PONG there :hi
    QString textToPrint = GET_STRING("message.pong")
                          .arg(msg.m_prefix)
                          .arg(msg.m_params[1]);
    printOutput(textToPrint, MESSAGE_IRC_PONG);
}

//-----------------------------------//

void StatusWindow::onPrivmsgMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();

    QString fromNick = parseMsgPrefix(msg.m_prefix, MsgPrefixName);
    CtcpRequestType requestType = getCtcpRequestType(msg);
    if(requestType != RequestTypeInvalid &&
       requestType != RequestTypeAction)
    {
        QString replyStr;
        QString requestTypeStr;

        switch(requestType)
        {
            case RequestTypeVersion:
            {
                replyStr = "conviersa v0.00000000001";
                requestTypeStr = "VERSION";
                break;
            }
            case RequestTypeTime:
            {
                replyStr = "bitch, please";
                requestTypeStr = "TIME";
                break;
            }
            case RequestTypeFinger:
            {
                replyStr = "good hustle.";
                requestTypeStr = "FINGER";
                break;
            }
            default:
            {
                requestTypeStr = "UNKNOWN";
            }
        }

        if(!replyStr.isEmpty())
        {
            QString textToSend = QString("NOTICE %1 :\1%2 %3\1")
                                 .arg(fromNick)
                                 .arg(requestTypeStr)
                                 .arg(replyStr);
            m_pSession->sendData(textToSend);
        }

        QString textToPrint = GET_STRING("message.ctcp")
                              .arg(requestTypeStr)
                              .arg(fromNick);
        printOutput(textToPrint, MESSAGE_IRC_CTCP);
        return;
    }

    // If the target is me, it's a PM from someone.
    if(m_pSession->isMyNick(msg.m_params[0]) && !childIrcWindowExists(fromNick))
    {
        QueryWindow *pQueryWin = new QueryWindow(m_pSession, m_pSharedServerConnPanel, fromNick);
        addQueryWindow(pQueryWin, false);

        // Delegate to the newly created QueryWindow.
        pQueryWin->onPrivmsgMessage(pEvent);
    }
}

//-----------------------------------//

void StatusWindow::onQuitMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();
    QString nick = parseMsgPrefix(msg.m_prefix, MsgPrefixName);
    QString userAndHost = parseMsgPrefix(msg.m_prefix, MsgPrefixUserAndHost);
    bool hasReason = (msg.m_paramsNum > 0 && !msg.m_params[0].isEmpty());

    for(int i = 0; i < m_chanList.size(); ++i)
    {
        ChannelWindow *pChannelWin = m_chanList[i];
        if(pChannelWin->hasUser(nick))
        {
            QString nickToDisplay = nick;
            if(GET_BOOL("irc.channel.properNickInChat"))
                nickToDisplay = pChannelWin->fetchProperNickname(nick);

            // Construct the message to display in this ChannelWindow.
            QString textToPrint = GET_STRING("message.quit")
                                    .arg(nickToDisplay)
                                    .arg(userAndHost);
            if(hasReason)
                textToPrint += GET_STRING("message.reason")
                                .arg(msg.m_params[0])
                                .arg(QString::fromUtf8("\xF"));

            pChannelWin->removeUser(nick);
            pChannelWin->printOutput(textToPrint, MESSAGE_IRC_QUIT);
        }
    }

    // Will print a quit message to the PM window if we get a QUIT message,
    // which will only be if we're in a channel with the person.
    for(int i = 0; i < m_privList.size(); ++i)
    {
        if(nick.compare(m_privList[i]->getTargetNick(), Qt::CaseInsensitive) == 0)
        {
            // Construct the message to display in the QueryWindow.
            QString textToPrint = GET_STRING("message.quit")
                                    .arg(nick)
                                    .arg(userAndHost);
            if(hasReason)
                textToPrint += GET_STRING("message.reason")
                                .arg(msg.m_params[0])
                                .arg(QString::fromUtf8("\xF"));

            m_privList[i]->printOutput(textToPrint, MESSAGE_IRC_QUIT);
            break;
        }
    }
}

//-----------------------------------//

void StatusWindow::onWallopsMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();
    QString textToPrint = GET_STRING("message.wallops")
                            .arg(parseMsgPrefix(msg.m_prefix, MsgPrefixName))
                            .arg(msg.m_params[0]);
    printOutput(textToPrint, MESSAGE_IRC_WALLOPS);
}

//-----------------------------------//

void StatusWindow::onUnknownMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();
    // Print the whole raw line.
    //printOutput(data);
    // TODO (seand): Decide what to do here.
}

//-----------------------------------//

void StatusWindow::onOutput(Event *) { }
void StatusWindow::onDoubleClickLink(Event *) { }

//-----------------------------------//

void StatusWindow::setupServerConfig(QMap<QString, ConfigOption> &defOptions)
{
    QVariantList serverList;
    defOptions.insert("recent.servers", ConfigOption(serverList, CONFIG_TYPE_LIST));
}

//-----------------------------------//

void StatusWindow::setupIRCConfig(QMap<QString, ConfigOption> &defOptions)
{
    defOptions.insert("irc.channel.properNickInChat", ConfigOption(false, CONFIG_TYPE_BOOLEAN));
}

} } // End namespaces
