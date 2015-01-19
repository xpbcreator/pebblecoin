// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include <QAction>
#include <QListWidget>
#include <QSplitter>
#include "cv/ChannelUser.h"
#include "cv/Session.h"
#include "cv/Connection.h"
#include "cv/ConfigManager.h"
#include "cv/gui/WindowManager.h"
#include "cv/gui/ChannelWindow.h"
#include "cv/gui/QueryWindow.h"
#include "cv/gui/StatusWindow.h"
#include "cv/gui/OutputControl.h"

#include <QScrollBar>
#include <QTextEdit>

#define COLOR_BACKGROUND tr("userlist.color.background")
#define COLOR_FOREGROUND tr("userlist.color.foreground")

namespace cv { namespace gui {

ChannelWindow::ChannelWindow(Session *pSession,
                             QExplicitlySharedDataPointer<ServerConnectionPanel> pSharedServerConnPanel,
                             const QString &title/* = tr("Untitled")*/,
                             const QSize &size/* = QSize(500, 300)*/)
    : InputOutputWindow(title, size),
      m_inChannel(false),
      m_matchesIdx(-1)
{
    m_pSession = pSession;
    m_pSharedServerConnPanel = pSharedServerConnPanel;

    m_pSplitter = new QSplitter(this);
    m_pUserList = new QListWidget;
    m_pUserList->setFont(m_defaultFont);
    QObject::connect(m_pUserList, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(onUserDoubleClicked(QListWidgetItem*)));

    m_pSplitter->addWidget(m_pOutput);
    m_pSplitter->addWidget(m_pUserList);

    if(size.width() > 150)
    {
        QList<int> sizes;
        sizes.append(size.width() - 150);
        sizes.append(150);
        m_pSplitter->setSizes(sizes);
    }
    m_pSplitter->setStretchFactor(0, 1);

    m_pVLayout->addWidget(m_pSplitter, 1);
    m_pVLayout->addWidget(m_pInput);
    m_pVLayout->setSpacing(5);
    m_pVLayout->setContentsMargins(2, 2, 2, 2);

    setLayout(m_pVLayout);

    m_pOpenButton = m_pSharedServerConnPanel->addOpenButton(m_pOutput, "Connect", 80, 30);
    m_pOutput->installEventFilter(this);

    setupColors();

    g_pEvtManager->hookEvent("numericMessage",  m_pSession, MakeDelegate(this, &ChannelWindow::onNumericMessage));
    g_pEvtManager->hookEvent("joinMessage",     m_pSession, MakeDelegate(this, &ChannelWindow::onJoinMessage));
    g_pEvtManager->hookEvent("kickMessage",     m_pSession, MakeDelegate(this, &ChannelWindow::onKickMessage));
    g_pEvtManager->hookEvent("modeMessage",     m_pSession, MakeDelegate(this, &ChannelWindow::onModeMessage));
    g_pEvtManager->hookEvent("noticeMessage",   m_pSession, MakeDelegate(this, &ChannelWindow::onNoticeMessage));
    g_pEvtManager->hookEvent("nickMessage",     m_pSession, MakeDelegate(this, &ChannelWindow::onNickMessage));
    g_pEvtManager->hookEvent("partMessage",     m_pSession, MakeDelegate(this, &ChannelWindow::onPartMessage));
    g_pEvtManager->hookEvent("privmsgMessage",  m_pSession, MakeDelegate(this, &ChannelWindow::onPrivmsgMessage));
    g_pEvtManager->hookEvent("topicMessage",    m_pSession, MakeDelegate(this, &ChannelWindow::onTopicMessage));

    g_pEvtManager->hookEvent("configChanged", COLOR_BACKGROUND, MakeDelegate(this, &ChannelWindow::onColorConfigChanged));
    g_pEvtManager->hookEvent("configChanged", COLOR_FOREGROUND, MakeDelegate(this, &ChannelWindow::onColorConfigChanged));
}

//-----------------------------------//

ChannelWindow::~ChannelWindow()
{
    g_pEvtManager->unhookEvent("numericMessage", m_pSession, MakeDelegate(this, &ChannelWindow::onNumericMessage));
    g_pEvtManager->unhookEvent("joinMessage",    m_pSession, MakeDelegate(this, &ChannelWindow::onJoinMessage));
    g_pEvtManager->unhookEvent("kickMessage",    m_pSession, MakeDelegate(this, &ChannelWindow::onKickMessage));
    g_pEvtManager->unhookEvent("modeMessage",    m_pSession, MakeDelegate(this, &ChannelWindow::onModeMessage));
    g_pEvtManager->unhookEvent("nickMessage",    m_pSession, MakeDelegate(this, &ChannelWindow::onNickMessage));
    g_pEvtManager->unhookEvent("noticeMessage",  m_pSession, MakeDelegate(this, &ChannelWindow::onNoticeMessage));
    g_pEvtManager->unhookEvent("partMessage",    m_pSession, MakeDelegate(this, &ChannelWindow::onPartMessage));
    g_pEvtManager->unhookEvent("privmsgMessage", m_pSession, MakeDelegate(this, &ChannelWindow::onPrivmsgMessage));
    g_pEvtManager->unhookEvent("topicMessage",   m_pSession, MakeDelegate(this, &ChannelWindow::onTopicMessage));

    g_pEvtManager->unhookEvent("configChanged", COLOR_BACKGROUND, MakeDelegate(this, &ChannelWindow::onColorConfigChanged));
    g_pEvtManager->unhookEvent("configChanged", COLOR_FOREGROUND, MakeDelegate(this, &ChannelWindow::onColorConfigChanged));

    m_pSharedServerConnPanel.reset();
}

//-----------------------------------//

void ChannelWindow::setupColorConfig(QMap<QString, ConfigOption> &defOptions)
{
    defOptions.insert(COLOR_BACKGROUND, ConfigOption("#ffffff", CONFIG_TYPE_COLOR));
    defOptions.insert(COLOR_FOREGROUND, ConfigOption("#000000", CONFIG_TYPE_COLOR));
}

//-----------------------------------//

void ChannelWindow::setupColors()
{
    QString stylesheet("QListWidget { background-color: %1 } QListWidget::item { color: %2 }");
    stylesheet = stylesheet.arg(GET_STRING(COLOR_BACKGROUND))
                           .arg(GET_STRING(COLOR_FOREGROUND));
    m_pUserList->setStyleSheet(stylesheet);
}

//-----------------------------------//

void ChannelWindow::onColorConfigChanged(Event *pEvent)
{
    setupColors();
}

//-----------------------------------//

// Adds the user to the channel's userlist in the proper place.
// [user] holds the nickname, and can include any number of prefixes,
// as well as the user and host.
//
// Returns true if the user was added, false otherwise.
bool ChannelWindow::addUser(const QString &user)
{
    ChannelUser *pNewUser = new ChannelUser(m_pSession, user);
    if(!addUser(pNewUser))
    {
        delete pNewUser;
        return false;
    }

    return true;
}

//-----------------------------------//

// Removes the user from the channel's userlist. Returns true if
// the user was removed, false otherwise.
bool ChannelWindow::removeUser(const QString &user)
{
    for(int i = 0; i < m_users.size(); ++i)
    {
        if(user.compare(m_users[i]->getNickname(), Qt::CaseInsensitive) == 0)
        {
            delete m_users.takeAt(i);
            m_pUserList->takeItem(i);
            return true;
        }
    }

    return false;
}

//-----------------------------------//

// Changes the user's nickname from [oldNick] to [newNick], unless
// the user is not in the channel.
void ChannelWindow::changeUserNick(const QString &oldNick, const QString &newNick)
{
    ChannelUser *pNewUser = findUser(oldNick);
    if(!pNewUser)
        return;

    ChannelUser *pUser = new ChannelUser(m_pSession, newNick);
    for(int i = 0; i < m_users.size(); ++i)
    {
        if(m_users[i] == pNewUser)
        {
            removeUser(pNewUser);
            pNewUser->setNickname(pUser->getNickname());
            addUser(pNewUser);
            break;
        }
    }

    delete pUser;
}

//-----------------------------------//

// Adds the specified prefix to the user, and updates the user list display
// (if necessary).
void ChannelWindow::addPrefixToUser(const QString &user, const QChar &prefixToAdd)
{
    ChannelUser *pUser = findUser(user);
    if(pUser)
    {
        removeUser(pUser);
        pUser->addPrefix(prefixToAdd);
        addUser(pUser);
    }
}

//-----------------------------------//

// Removes the specified prefix from the user, and updates the user list
// display (if necessary) - assuming the user has the given prefix.
void ChannelWindow::removePrefixFromUser(const QString &user, const QChar &prefixToRemove)
{
    ChannelUser *pUser = findUser(user);
    if(pUser)
    {
        removeUser(pUser);
        pUser->removePrefix(prefixToRemove);
        addUser(pUser);
    }
}

//-----------------------------------//

// Provided the [nick], returns the "proper" nickname (which has the
// most powerful prefix prepended to it). If there is no prefix or
// the ChannelUser cannot be found, it returns [nick].
QString ChannelWindow::fetchProperNickname(const QString &nick)
{
    ChannelUser *pUser = findUser(nick);
    if(pUser != NULL)
        return pUser->getProperNickname();
    return nick;
}

//-----------------------------------//

void ChannelWindow::onNumericMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();
    switch(msg.m_command)
    {
        // RPL_TOPIC
        case 332:
        {
            // msg.m_params[0]: my nick
            // msg.m_params[1]: channel
            // msg.m_params[2]: topic
            if(isChannelName(msg.m_params[1]))
            {
                QString titleWithTopic = QString("%1: %2")
                                         .arg(getWindowName())
                                         .arg(stripCodes(msg.m_params[2]));
                setTitle(titleWithTopic);

                QString textToPrint = GET_STRING("message.332")
                                        .arg(msg.m_params[2]);

                if(m_inChannel)
                    printOutput(textToPrint, MESSAGE_IRC_TOPIC);
                else
                    enqueueMessage(textToPrint, MESSAGE_IRC_TOPIC);
            }

            break;
        }
        case 333:
        {
            // msg.m_params[0]: my nick
            // msg.m_params[1]: channel
            // msg.m_params[2]: nick
            // msg.m_params[3]: unix time
            if(isChannelName(msg.m_params[1]))
            {
                QString textToPrint = GET_STRING("message.333.channel")
                                      .arg(msg.m_params[2])
                                      .arg(getDate(msg.m_params[3]))
                                      .arg(getTime(msg.m_params[3]));
                if(m_inChannel)
                    printOutput(textToPrint, MESSAGE_IRC_TOPIC);
                else
                    enqueueMessage(textToPrint, MESSAGE_IRC_TOPIC);
            }

            break;
        }
        case 353:
        {
            // msg.m_params[0]: my nick
            // msg.m_params[1]: "=" | "*" | "@"
            // msg.m_params[2]: channel
            // msg.m_params[3]: names, separated by spaces
            //
            // RPL_NAMREPLY was sent as a result of a JOIN command.
            if(!m_inChannel)
            {
                int numSections = msg.m_params[3].count(' ') + 1;
                for(int i = 0; i < numSections; ++i)
                    addUser(msg.m_params[3].section(' ', i, i, QString::SectionSkipEmpty));
            }
            break;
        }
        case 366:
        {
            // msg.m_params[0]: my nick
            // msg.m_params[1]: channel
            // msg.m_params[2]: "End of NAMES list"
            //
            // RPL_ENDOFNAMES was sent as a result of a JOIN command.
            if(!m_inChannel)
                joinChannel();
            break;
        }
    }
}

//-----------------------------------//

void ChannelWindow::onJoinMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();
    if(isChannelName(msg.m_params[0]))
    {
        QString textToPrint;
        QString nickJoined = parseMsgPrefix(msg.m_prefix, MsgPrefixName);
        if(m_pSession->isMyNick(nickJoined))
        {
            textToPrint = GET_STRING("message.rejoin")
                            .arg(msg.m_params[0]);
            m_pManager->setCurrentItem(m_pManager->getItemFromWindow(this));
            giveFocus();
        }
        else
        {
            textToPrint = GET_STRING("message.join")
                          .arg(parseMsgPrefix(msg.m_prefix, MsgPrefixName))
                          .arg(parseMsgPrefix(msg.m_prefix, MsgPrefixUserAndHost))
                          .arg(msg.m_params[0]);
            addUser(nickJoined);
        }

        printOutput(textToPrint, MESSAGE_IRC_JOIN);
    }
}

//-----------------------------------//

void ChannelWindow::onKickMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();
    if(isChannelName(msg.m_params[0]))
    {
        QString textToPrint;
        if(m_pSession->isMyNick(msg.m_params[1]))
        {
            leaveChannel();
            textToPrint = GET_STRING("message.kick.self")
                            .arg(parseMsgPrefix(msg.m_prefix, MsgPrefixName));
        }
        else
        {
            removeUser(msg.m_params[1]);
            textToPrint = GET_STRING("message.kick")
                            .arg(msg.m_params[1])
                            .arg(parseMsgPrefix(msg.m_prefix, MsgPrefixName));
        }

        bool hasReason = (msg.m_paramsNum > 2 && !msg.m_params[2].isEmpty());
        if(hasReason)
            textToPrint += GET_STRING("message.reason")
                            .arg(msg.m_params[2])
                            .arg(QString::fromUtf8("\xF"));

        printOutput(textToPrint, MESSAGE_IRC_KICK);
    }
}

//-----------------------------------//

void ChannelWindow::onModeMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();
    if(isChannelName(msg.m_params[0]))
    {
        // Ignore the first parameter.
        QString modeParams = msg.m_params[1];
        for(int i = 2; i < msg.m_paramsNum; ++i)
            modeParams += ' ' + msg.m_params[i];

        QString textToPrint = GET_STRING("message.mode")
                                .arg(parseMsgPrefix(msg.m_prefix, MsgPrefixName))
                                .arg(modeParams);

        bool sign = true;
        QString modes = msg.m_params[1];
        for(int modesIndex = 0, paramsIndex = 2; modesIndex < modes.size(); ++modesIndex)
        {
            if(modes[modesIndex] == '+')
            {
                sign = true;
            }
            else if(modes[modesIndex] == '-')
            {
                sign = false;
            }
            else
            {
                ChanModeType type = getChanModeType(m_pSession->getChanModes(), modes[modesIndex]);
                switch(type)
                {
                    case ModeTypeA:
                    case ModeTypeB:
                    case ModeTypeC:
                    {
                        // If there's no params left, continue.
                        if(paramsIndex >= msg.m_paramsNum)
                            break;

                        QChar prefix = m_pSession->getPrefixRule(modes[modesIndex]);
                        if(prefix != '\0')
                        {
                            if(sign)
                                addPrefixToUser(msg.m_params[paramsIndex], prefix);
                            else
                                removePrefixFromUser(msg.m_params[paramsIndex], prefix);
                        }

                        ++paramsIndex;
                        break;
                    }
                    default:
                    {

                    }
                }
            }
        }

        printOutput(textToPrint, MESSAGE_IRC_MODE);
    }
}

//-----------------------------------//

void ChannelWindow::onNickMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();

    QString oldNick = parseMsgPrefix(msg.m_prefix, MsgPrefixName);
    if(hasUser(oldNick))
    {
        changeUserNick(oldNick, msg.m_params[0]);
        QString textToPrint = GET_STRING("message.nick")
                              .arg(oldNick)
                              .arg(msg.m_params[0]);
        printOutput(textToPrint, MESSAGE_IRC_NICK);
    }
}

//-----------------------------------//

void ChannelWindow::onNoticeMessage(Event *pEvent)
{
    if(m_pManager->isWindowFocused(this))
    {
        InputOutputWindow::onNoticeMessage(pEvent);
    }
}

//-----------------------------------//

void ChannelWindow::onPartMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();
    if(isChannelName(msg.m_params[0]))
    {
        QString textToPrint;

        // If the PART message is for my nick, then call leaveChannel().
        if(m_pSession->isMyNick(parseMsgPrefix(msg.m_prefix, MsgPrefixName)))
        {
            leaveChannel();
            textToPrint = GET_STRING("message.part.self")
                            .arg(msg.m_params[0]);

        }
        else
        {
            // Get the nickname to display.
            QString nick = parseMsgPrefix(msg.m_prefix, MsgPrefixName);
            removeUser(nick);
            if(GET_BOOL("irc.channel.properNickInChat"))
                nick = fetchProperNickname(nick);

            textToPrint = GET_STRING("message.part")
                          .arg(nick)
                          .arg(parseMsgPrefix(msg.m_prefix, MsgPrefixUserAndHost))
                          .arg(msg.m_params[0]);
        }

        // If there's a reason in the PART message, append it before it gets displayed.
        bool hasReason = (msg.m_paramsNum > 1 && !msg.m_params[1].isEmpty());
        if(hasReason)
            textToPrint += GET_STRING("message.reason")
                            .arg(msg.m_params[1])
                            .arg(QString::fromUtf8("\xF"));

        printOutput(textToPrint, MESSAGE_IRC_PART);
    }
}

//-----------------------------------//

void ChannelWindow::onPrivmsgMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();

    if(isChannelName(msg.m_params[0]))
    {
        // Get the nickname to display.
        QString nick = parseMsgPrefix(msg.m_prefix, MsgPrefixName);
        if(GET_BOOL("irc.channel.properNickInChat"))
            nick = fetchProperNickname(nick);

        QString textToPrint;
        bool shouldHighlight = false;
        OutputMessageType msgType = MESSAGE_CUSTOM;

        CtcpRequestType requestType = getCtcpRequestType(msg);
        if(requestType != RequestTypeInvalid)
        {
            // ACTION is /me, so handle according to that.
            if(requestType == RequestTypeAction)
            {
                QString action = msg.m_params[1];

                // [action] at this point looks like this: "\1ACTION <action>\1",
                // so we want to exclude the first 8 and last 1 characters.
                msgType = MESSAGE_IRC_ACTION;
                QString msgText = action.mid(8, action.size()-9);
                shouldHighlight = containsNick(msgText);
                textToPrint = GET_STRING("message.action")
                                .arg(nick)
                                .arg(msgText);
            }
        }
        else
        {
            msgType = MESSAGE_IRC_SAY;
            shouldHighlight = containsNick(msg.m_params[1]);
            textToPrint = GET_STRING("message.say")
                            .arg(nick)
                            .arg(msg.m_params[1]);
        }
/*
        if(!hasFocus())
        {
            if(msg.m_params[1].toLower().contains(m_pSession->getNick().toLower()))
            {
                QApplication::alert(this);
            }
        }
*/
        printOutput(textToPrint, msgType, shouldHighlight ? COLOR_HIGHLIGHT : COLOR_NONE);
    }
}

//-----------------------------------//

void ChannelWindow::onTopicMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();
    if(isChannelName(msg.m_params[0]))
    {
        QString textToPrint = GET_STRING("message.topic")
                                .arg(parseMsgPrefix(msg.m_prefix, MsgPrefixName))
                                .arg(msg.m_params[1]);
        printOutput(textToPrint, MESSAGE_IRC_TOPIC);

        if(m_pManager)
        {
            QTreeWidgetItem *pItem = m_pManager->getItemFromWindow(this);
            QString titleWithTopic = QString("%1: %2")
                                     .arg(pItem->text(0))
                                     .arg(msg.m_params[1]);
            setTitle(stripCodes(titleWithTopic));
        }
    }
}

//-----------------------------------//

void ChannelWindow::onOutput(Event *pEvent)
{
    OutputEvent *pOutputEvt = DCAST(OutputEvent, pEvent);
    for(int i = 0; i < m_users.size(); ++i)
    {
        QRegExp regex(OutputWindow::s_invalidNickPrefix
                    + QRegExp::escape(m_users[i]->getNickname())
                    + OutputWindow::s_invalidNickSuffix);
        regex.setCaseSensitivity(Qt::CaseInsensitive);
        int lastIdx = 0, idx;
        while((idx = regex.indexIn(pOutputEvt->getText(), lastIdx)) >= 0)
        {
            idx += regex.capturedTexts()[1].length();
            lastIdx = idx + m_users[i]->getNickname().length() - 1;
            pOutputEvt->addLinkInfo(idx, lastIdx);
        }
    }
}

//-----------------------------------//

void ChannelWindow::onDoubleClickLink(Event *pEvent)
{
    DoubleClickLinkEvent *pDblClickLinkEvt = DCAST(DoubleClickLinkEvent, pEvent);

    // Check to see if there is a QueryWindow already open for this nick.
    Window *pParentWin = m_pManager->getWindowFromItem(m_pManager->getItemFromWindow(this)->parent());
    StatusWindow *pStatusWin = DCAST(StatusWindow, pParentWin);

    OutputWindow *pQueryWin = pStatusWin->getChildIrcWindow(pDblClickLinkEvt->getText());
    if(pQueryWin != NULL)
    {
        m_pManager->setCurrentItem(m_pManager->getItemFromWindow(pQueryWin));
        pQueryWin->giveFocus();
    }
    else
    {
        pStatusWin->addQueryWindow(new QueryWindow(m_pSession, m_pSharedServerConnPanel, pDblClickLinkEvt->getText()), true);
    }
}

//-----------------------------------//

// Adds a message to the "in-limbo" queue, where messages are
// stored before the channel has been officially "joined" (when
// the userlist has been received).
void ChannelWindow::enqueueMessage(const QString &msg, OutputMessageType msgType)
{
    QueuedOutputMessage qom;
    qom.message = msg;
    qom.messageType = msgType;
    m_messageQueue.enqueue(qom);
}

//-----------------------------------//

// "Joins" the channel by flushing all messages received since creating
// the channel (but before the list of users was received).
void ChannelWindow::joinChannel()
{
    m_inChannel = true;
    while(!m_messageQueue.isEmpty())
    {
        QueuedOutputMessage qom = m_messageQueue.dequeue();
        printOutput(qom.message, qom.messageType);
    }
}

//-----------------------------------//

// Removes all the users from memory.
void ChannelWindow::leaveChannel()
{
    m_inChannel = false;
    while(m_users.size() > 0)
    {
        delete m_users.takeAt(0);
        m_pUserList->takeItem(0);
    }
}

//-----------------------------------//

// Handles the printing/sending of the PRIVMSG message.
void ChannelWindow::handleSay(const QString &text)
{
    // Get the nickname to display.
    QString myNick = m_pSession->getNick();
    if(GET_BOOL("irc.channel.properNickInChat"))
        myNick = fetchProperNickname(myNick);

    // Print and send the message.
    QString textToPrint = GET_STRING("message.say")
                          .arg(myNick)
                          .arg(text);
    printOutput(textToPrint, MESSAGE_IRC_SAY_SELF);
    m_pSession->sendPrivmsg(getWindowName(), text);
}

//-----------------------------------//

// Handles the printing/sending of the PRIVMSG ACTION message.
void ChannelWindow::handleAction(const QString &text)
{
    // Get the nickname to display.
    QString myNick = m_pSession->getNick();
    if(GET_BOOL("irc.channel.properNickInChat"))
        myNick = fetchProperNickname(myNick);

    // Print and send the message.
    QString textToPrint = GET_STRING("message.action")
                          .arg(myNick)
                          .arg(text);
    printOutput(textToPrint, MESSAGE_IRC_ACTION_SELF);
    m_pSession->sendAction(getWindowName(), text);
}

//-----------------------------------//

void ChannelWindow::handleTab()
{
    QString text = getInputText();
    if(m_matchesIdx < 0)
    {
        int idx, lastIdx;
        idx = lastIdx = m_pInput->textCursor().position();

        // Find the beginning of the word that the user is
        // trying to tab-complete.
        while(--idx >= 0 && text[idx] != ' ');
        ++idx;

        // Now capture the parts.
        m_preAutocompleteStr = text.mid(0, idx);
        QString word = text.mid(idx, lastIdx - idx);
        m_postAutocompleteStr = text.mid(lastIdx);

        if(word.isEmpty())
            return;

        // Find all autocomplete matches.
        for(int i = 0; i < m_users.size(); ++i)
            if(m_users[i]->getNickname().startsWith(word, Qt::CaseInsensitive))
                m_autocompleteMatches.push_back(m_users[i]);

        if(m_autocompleteMatches.size() > 0)
            m_matchesIdx = 0;
    }
    else
    {
        ++m_matchesIdx;
        m_matchesIdx %= m_autocompleteMatches.size();
    }

    // Set the text and cursor.
    if(m_matchesIdx >= 0)
    {
        m_pInput->setPlainText(m_preAutocompleteStr
                             + m_autocompleteMatches[m_matchesIdx]->getNickname()
                             + m_postAutocompleteStr);
        int cursorPos = m_preAutocompleteStr.length()
                      + m_autocompleteMatches[m_matchesIdx]->getNickname().length();
        QTextCursor textCursor = m_pInput->textCursor();
        textCursor.setPosition(cursorPos);
        m_pInput->setTextCursor(textCursor);
    }
}

//-----------------------------------//

// Handles child widget events.
bool ChannelWindow::eventFilter(QObject *obj, QEvent *event)
{
    if(obj == m_pInput)
    {
        if(event->type() == QEvent::KeyPress)
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if(keyEvent->key() != Qt::Key_Tab)
            {
                m_matchesIdx = -1;
                m_autocompleteMatches.clear();
            }
        }
    }

    return InputOutputWindow::eventFilter(obj, event);
}

//-----------------------------------//

void ChannelWindow::closeEvent(QCloseEvent *event)
{
    emit chanWindowClosing(this);

    if(m_inChannel)
    {
        leaveChannel();
        m_pSession->sendData(QString("PART %1").arg(getWindowName()));
    }

    return Window::closeEvent(event);
}

//-----------------------------------//

// Adds a ChannelUser to both the list in memory and the
// list view which is seen by the user.
bool ChannelWindow::addUser(ChannelUser *pNewUser)
{
    QListWidgetItem *pListItem = new QListWidgetItem(pNewUser->getProperNickname());
    for(int i = 0; i < m_users.size(); ++i)
    {
        int compareVal = m_pSession->compareNickPrefixes(m_users[i]->getPrefix(), pNewUser->getPrefix());
        if(compareVal > 0)
        {
            m_pUserList->insertItem(i, pListItem);
            m_users.insert(i, pNewUser);
            return true;
        }
        else if(compareVal == 0)
        {
            compareVal = QString::compare(m_users[i]->getNickname(), pNewUser->getNickname(), Qt::CaseInsensitive);
            if(compareVal > 0)
            {
                m_pUserList->insertItem(i, pListItem);
                m_users.insert(i, pNewUser);
                return true;
            }
            else if(compareVal == 0)
            {
                // The user is already in the list.
                return false;
            }
        }
    }

    m_pUserList->addItem(pListItem);
    m_users.append(pNewUser);
    return true;
}

//-----------------------------------//

// Removes a ChannelUser from both the list in memory and the
// list view which is seen by the user.
void ChannelWindow::removeUser(ChannelUser *pUser)
{
    for(int i = 0; i < m_users.size(); ++i)
    {
        if(m_users[i] == pUser)
        {
            delete m_pUserList->takeItem(i);
            m_users.removeAt(i);
            break;
        }
    }
}

//-----------------------------------//

// Finds the user within the channel, based on nickname
// (regardless if there are prefixes or a user/host in it).
//
// Returns a pointer to the Channeluser if found in the channel,
// NULL otherwise.
ChannelUser *ChannelWindow::findUser(const QString &user)
{
    ChannelUser ircChanUser(m_pSession, user);
    for(int i = 0; i < m_users.size(); ++i)
        if(QString::compare(m_users[i]->getNickname(), ircChanUser.getNickname(), Qt::CaseInsensitive) == 0)
            return m_users[i];

    return NULL;
}

//-----------------------------------//

void ChannelWindow::onUserDoubleClicked(QListWidgetItem *pItem)
{
    for(int i = 0; i < m_users.size(); ++i)
        if(m_users[i]->getProperNickname().compare(pItem->text()) == 0
        && !m_pSession->isMyNick(m_users[i]->getNickname()))
        {
            DoubleClickLinkEvent *pEvt = new DoubleClickLinkEvent(m_users[i]->getNickname());
            g_pEvtManager->fireEvent("doubleClickedLink", m_pOutput, pEvt);
            delete pEvt;
            break;
        }
}

} } // End namespaces
