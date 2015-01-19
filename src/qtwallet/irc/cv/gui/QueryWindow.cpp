// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include <QMutex>
#include "cv/Session.h"
#include "cv/ConfigManager.h"
#include "cv/gui/WindowManager.h"
#include "cv/gui/QueryWindow.h"
#include "cv/gui/StatusWindow.h"
#include "cv/gui/OutputControl.h"

namespace cv { namespace gui {

QueryWindow::QueryWindow(Session *pSession,
                         QExplicitlySharedDataPointer<ServerConnectionPanel> pSharedServerConnPanel,
                         const QString &targetNick)
    : InputOutputWindow(targetNick)
{
    m_pSession = pSession;
    m_pSharedServerConnPanel = pSharedServerConnPanel;
    m_targetNick = targetNick;

    m_pVLayout->addWidget(m_pOutput);
    m_pVLayout->addWidget(m_pInput);
    m_pVLayout->setSpacing(5);
    m_pVLayout->setContentsMargins(2, 2, 2, 2);
    setLayout(m_pVLayout);

    m_pOpenButton = m_pSharedServerConnPanel->addOpenButton(m_pOutput, "Connect", 80, 30);
    m_pOutput->installEventFilter(this);

    g_pEvtManager->hookEvent("numericMessage", m_pSession, MakeDelegate(this, &QueryWindow::onNumericMessage));
    g_pEvtManager->hookEvent("nickMessage",    m_pSession, MakeDelegate(this, &QueryWindow::onNickMessage));
    g_pEvtManager->hookEvent("noticeMessage",  m_pSession, MakeDelegate(this, &QueryWindow::onNoticeMessage));
    g_pEvtManager->hookEvent("privmsgMessage", m_pSession, MakeDelegate(this, &QueryWindow::onPrivmsgMessage));
}

//-----------------------------------//

QueryWindow::~QueryWindow()
{
    g_pEvtManager->unhookEvent("numericMessage", m_pSession, MakeDelegate(this, &QueryWindow::onNumericMessage));
    g_pEvtManager->unhookEvent("nickMessage",    m_pSession, MakeDelegate(this, &QueryWindow::onNickMessage));
    g_pEvtManager->unhookEvent("noticeMessage",  m_pSession, MakeDelegate(this, &QueryWindow::onNoticeMessage));
    g_pEvtManager->unhookEvent("privmsgMessage", m_pSession, MakeDelegate(this, &QueryWindow::onPrivmsgMessage));

    m_pSharedServerConnPanel.reset();
}

//-----------------------------------//

// Changes the nickname of the person we're chatting with.
void QueryWindow::setTargetNick(const QString &nick)
{
    m_targetNick = nick;
    setWindowName(nick);
    setTitle(nick);
}

//-----------------------------------//

// Returns the target nick that we're chatting with
// (same as Window::getWindowName() & Window::getTitle()).
QString QueryWindow::getTargetNick()
{
    return m_targetNick;
}

//-----------------------------------//

void QueryWindow::onNumericMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();
    switch(msg.m_command)
    {
        case 401:   // ERR_NOSUCKNICK
        case 404:   // ERR_CANNOTSENDTOCHAN
        {
            // msg.m_params[0]: my nick
            // msg.m_params[1]: nick/channel
            // msg.m_params[2]: "No such nick/channel"
            if(msg.m_params[1].compare(getWindowName(), Qt::CaseInsensitive) == 0)
                printOutput(getNumericText(msg), MESSAGE_IRC_NUMERIC);
        }
    }
}

//-----------------------------------//

void QueryWindow::onNickMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();

    // Will print a nick change message to the PM window
    // if we get a NICK message, which will only be if we're in
    // a channel with the person (or if the nick being changed is ours).
    QString oldNick = parseMsgPrefix(msg.m_prefix, MsgPrefixName);
    QString textToPrint = GET_STRING("message.nick")
                          .arg(oldNick)
                          .arg(msg.m_params[0]);
    if(m_pSession->isMyNick(oldNick))
    {
        printOutput(textToPrint, MESSAGE_IRC_NICK);
    }
    else
    {
        // If the target nick has changed and there isn't another query with that name
        // already open, then we can safely change the target's nick.
        bool queryWindowExists = DCAST(StatusWindow, m_pManager->getParentWindow(this))->childIrcWindowExists(msg.m_params[0]);
        if(isTargetNick(oldNick) && !queryWindowExists)
        {
            setTargetNick(msg.m_params[0]);
            printOutput(textToPrint, MESSAGE_IRC_NICK);
        }
    }
}

//-----------------------------------//

void QueryWindow::onNoticeMessage(Event *pEvent)
{
    if(m_pManager->isWindowFocused(this))
        InputOutputWindow::onNoticeMessage(pEvent);
}

//-----------------------------------//

void QueryWindow::onPrivmsgMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();
    if(m_pSession->isMyNick(msg.m_params[0]))
    {
        QString fromNick = parseMsgPrefix(msg.m_prefix, MsgPrefixName);
        if(isTargetNick(fromNick))
        {
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

                    // Action is in the format of "\1ACTION <action>\1", so
                    // the first 8 and last 1 characters will be excluded.
                    msgType = MESSAGE_IRC_ACTION;
                    QString msgText = action.mid(8, action.size()-9);
                    shouldHighlight = containsNick(msgText);
                    textToPrint = GET_STRING("message.action")
                                  .arg(fromNick)
                                  .arg(msgText);
                }
            }
            else
            {
                msgType = MESSAGE_IRC_SAY;
                shouldHighlight = containsNick(msg.m_params[1]);
                textToPrint = GET_STRING("message.say")
                              .arg(fromNick)
                              .arg(msg.m_params[1]);
            }

            if(!hasFocus())
            {
                QApplication::alert(this);
            }

            printOutput(textToPrint, msgType, shouldHighlight ? COLOR_HIGHLIGHT : COLOR_NONE);
        }
    }
}

//-----------------------------------//

void QueryWindow::onOutput(Event *pEvent)
{
    OutputEvent *pOutputEvt = DCAST(OutputEvent, pEvent);
    QRegExp regex(OutputWindow::s_invalidNickPrefix
                + QRegExp::escape(m_targetNick)
                + OutputWindow::s_invalidNickSuffix);
    regex.setCaseSensitivity(Qt::CaseInsensitive);
    int lastIdx = 0, idx;
    while((idx = regex.indexIn(pOutputEvt->getText(), lastIdx)) >= 0)
    {
        idx += regex.capturedTexts()[1].length();
        lastIdx = idx + m_targetNick.length() - 1;
        pOutputEvt->addLinkInfo(idx, lastIdx);
    }
}

//-----------------------------------//

void QueryWindow::onDoubleClickLink(Event *pEvent)
{
    DoubleClickLinkEvent *pDblClickLinkEvt = DCAST(DoubleClickLinkEvent, pEvent);
    m_pSession->sendData(QString().arg(pDblClickLinkEvt->getText()));
}

//-----------------------------------//

// Handles the printing/sending of the PRIVMSG message.
void QueryWindow::handleSay(const QString &text)
{
    QString textToPrint = GET_STRING("message.say")
                            .arg(m_pSession->getNick())
                            .arg(text);
    printOutput(textToPrint, MESSAGE_IRC_SAY_SELF);
    m_pSession->sendPrivmsg(getWindowName(), text);
}

//-----------------------------------//

// Handles the printing/sending of the PRIVMSG ACTION message.
void QueryWindow::handleAction(const QString &text)
{
    QString textToPrint = GET_STRING("message.action")
                            .arg(m_pSession->getNick())
                            .arg(text);
    printOutput(textToPrint, MESSAGE_IRC_ACTION_SELF);
    m_pSession->sendAction(getWindowName(), text);
}

//-----------------------------------//

void QueryWindow::handleTab()
{
    QString text = getInputText();
    int idx = m_pInput->textCursor().position();
}

//-----------------------------------//

void QueryWindow::closeEvent(QCloseEvent *event)
{
    emit privWindowClosing(this);
    return Window::closeEvent(event);
}

} } // End namespaces
