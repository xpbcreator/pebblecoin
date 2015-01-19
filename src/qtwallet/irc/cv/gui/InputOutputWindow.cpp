// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include <QApplication>
#include <QPushButton>
#include <QFont>
#include "cv/Session.h"
#include "cv/ConfigManager.h"
#include "cv/EventManager.h"
#include "cv/gui/InputOutputWindow.h"
#include "cv/gui/WindowManager.h"
#include "cv/gui/DebugWindow.h"

#define COLOR_BACKGROUND QObject::tr("input.color.background")
#define COLOR_FOREGROUND QObject::tr("input.color.foreground")

namespace cv { namespace gui {

InputOutputWindow::InputOutputWindow(const QString &title/* = tr("Untitled")*/,
                                     const QSize &size/* = QSize(500, 300)*/)
    : OutputWindow(title, size)
{
    m_pInput = new QPlainTextEdit;
    m_pInput->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_pInput->setMaximumSize(QWIDGETSIZE_MAX, 25);
    m_pInput->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_pInput->setFont(m_defaultFont);
    m_pInput->installEventFilter(this);
    setFocusProxy(m_pInput);
    m_pOutput->setFocusProxy(m_pInput);

    g_pEvtManager->hookEvent("input", m_pInput, MakeDelegate(this, &InputOutputWindow::onInput));
    g_pEvtManager->hookEvent("configChanged", COLOR_BACKGROUND, MakeDelegate(this, &InputOutputWindow::onColorConfigChanged));
    g_pEvtManager->hookEvent("configChanged", COLOR_FOREGROUND, MakeDelegate(this, &InputOutputWindow::onColorConfigChanged));
    setupColors();
}

//-----------------------------------//

InputOutputWindow::~InputOutputWindow()
{
    g_pEvtManager->unhookEvent("input", m_pInput, MakeDelegate(this, &InputOutputWindow::onInput));
    g_pEvtManager->unhookEvent("configChanged", COLOR_BACKGROUND, MakeDelegate(this, &InputOutputWindow::onColorConfigChanged));
    g_pEvtManager->unhookEvent("configChanged", COLOR_FOREGROUND, MakeDelegate(this, &InputOutputWindow::onColorConfigChanged));
}

//-----------------------------------//

void InputOutputWindow::setupColorConfig(QMap<QString, ConfigOption> &defOptions)
{
    defOptions.insert(COLOR_BACKGROUND, ConfigOption("#ffffff", CONFIG_TYPE_COLOR));
    defOptions.insert(COLOR_FOREGROUND, ConfigOption("#000000", CONFIG_TYPE_COLOR));
}

//-----------------------------------//

void InputOutputWindow::setupColors()
{
    QString stylesheet("background-color: %1; color: %2");
    stylesheet = stylesheet.arg(GET_STRING(COLOR_BACKGROUND))
                           .arg(GET_STRING(COLOR_FOREGROUND));
    m_pInput->setStyleSheet(stylesheet);
}

//-----------------------------------//

void InputOutputWindow::onColorConfigChanged(Event *)
{
    setupColors();
}

//-----------------------------------//

void InputOutputWindow::giveFocus()
{
    if(m_pSharedServerConnPanel->isOpen(m_pOutput))
        m_pSharedServerConnPanel->setFocus();
    else
        m_pInput->setFocus();
}

//-----------------------------------//

// Handles the input for the window.
void InputOutputWindow::onInput(Event *pEvent)
{
    // TODO (seand): Change this when colors are added.
    InputEvent *inputEvt = DCAST(InputEvent, pEvent);
    QString text = inputEvt->getInput();

    // TODO (seand): Should we make the '/' configurable?
    //
    // Handle commands that do not require sending the server data.
    //
    // Current format: /server <host> [port]
    // TODO (seand): Fix channel leaving.
    if(text.startsWith("/server ", Qt::CaseInsensitive))
    {
        QString host = text.section(' ', 1, 1, QString::SectionSkipEmpty);
        bool ok;
        int port = text.section(' ', 2, 2, QString::SectionSkipEmpty).toInt(&ok);
        if(!ok)
            port = 6667;

        m_pSession->disconnectFromServer();
        m_pSession->connectToServer(host, port);

        return;
    }
    else if(text.startsWith("/set ", Qt::CaseInsensitive))
    {
        text.remove(0, 5);

        // 2 parameters: <optName> <optValue>
        QString optName = text.section(' ', 0, 0, QString::SectionSkipEmpty),
                optValue = text.section(' ', 1, -1);
        if(optName.length() > 0 && optValue.length() > 0)
        {
            if(g_pCfgManager->setOptionValue(optName, optValue, true))
            {
                printOutput(QString("Changed option \"%1\" to \"%2\"")
                              .arg(optName)
                              .arg(g_pCfgManager->getOptionValue(optName).toString()),
                            MESSAGE_INFO);
                g_pCfgManager->writeToDefaultFile();
            }
            else
            {
                printOutput(QString("Failed to change option \"%1\" to \"%2\"")
                              .arg(optName)
                              .arg(optValue),
                            MESSAGE_INFO);
            }
        }
    }
    else if(text.startsWith("/font ", Qt::CaseInsensitive))
    {
        bool ok;
        int pt = text.section(' ', 1, 1, QString::SectionSkipEmpty).toInt(&ok);
        if(!ok) pt = 12;

        m_pOutput->changeFont(QFont("Consolas", pt));
    }
    else if(text.startsWith("/timestamp ", Qt::CaseInsensitive))
    {
        text.remove(0, 11);
        if(text.compare("on", Qt::CaseInsensitive) == 0)
            g_pCfgManager->setOptionValue("timestamp", true, false);
        else if(text.compare("off", Qt::CaseInsensitive) == 0)
            g_pCfgManager->setOptionValue("timestamp", false, false);
        else
            g_pCfgManager->setOptionValue("timestamp.format", text, false);
        g_pCfgManager->writeToDefaultFile();
    }
    else if(text.startsWith("/search ", Qt::CaseInsensitive))
    {
        text.remove(0, 8);
        //search(text);
        return;
    }
    else if(text.compare("/debug", Qt::CaseInsensitive) == 0)
    {
        // Check for a DebugWindow, otherwise create a new one.
        Window *pParentWin = m_pManager->getParentWindow(this);
        if(pParentWin == NULL)
            pParentWin = this;

        DebugWindow *pDebugWin = new DebugWindow(m_pSession);
        m_pManager->addWindow(pDebugWin, m_pManager->getItemFromWindow(pParentWin), true);
    }
    else    // Commands that interact with the server.
    {
        if(!m_pSession->isConnected())
        {
            printError("Not connected to a server.");
            return;
        }

        // Regular say command.
        if(!text.startsWith('/') || text.startsWith("/say ", Qt::CaseInsensitive))
        {
            if(text.startsWith('/'))
                text.remove(0, 5);
            handleSay(text);
        }
        else if(text.startsWith("/me ", Qt::CaseInsensitive))
        {
            text.remove(0, 4);
            handleAction(text);
        }
        else if(text.startsWith("/join ", Qt::CaseInsensitive)
             || text.startsWith("/j ", Qt::CaseInsensitive))
        {
            if(text.startsWith("/j ", Qt::CaseInsensitive))
                text.remove(0, 3);
            else
                text.remove(0, 6);

            QStringList params = text.split("\\s+");
            if(params.size() > 0)
            {
                QStringList channelsToJoin = params[0].split(',', QString::SkipEmptyParts);
                if(channelsToJoin.size() == 1)
                {
                    if(text[0] != '#' && text[0] != '0')
                        text.prepend('#');
                    m_pSession->sendData("JOIN " + text);
                }
                else if(channelsToJoin.size() > 1)
                {
                    for(int i = 0; i < channelsToJoin.size(); ++i)
                        if(channelsToJoin[i][0] != '#')
                            channelsToJoin[i].prepend('#');

                    // Send the message.
                    QString textToSend = "JOIN " + channelsToJoin.join(",");
                    if(params.size() > 1)
                        textToSend += " " + params[1];
                    m_pSession->sendData(textToSend);
                }
            }
        }
        else
        {
            text.remove(0, 1);
            m_pSession->sendData(text);
        }
    }
}

//-----------------------------------//

void InputOutputWindow::onNoticeMessage(Event *pEvent)
{
    Message msg = DCAST(MessageEvent, pEvent)->getMessage();
    QString source;
    if(!msg.m_prefix.isEmpty())
        source = parseMsgPrefix(msg.m_prefix, MsgPrefixName);
    // If m_prefix is empty, it is from the host.
    else
        source = m_pSession->getHost();

    QString textToPrint = GET_STRING("message.notice")
                          .arg(source)
                          .arg(msg.m_params[1]);
    printOutput(textToPrint, MESSAGE_IRC_NOTICE);
}

//-----------------------------------//

// Moves the input cursor to the end of the line.
void InputOutputWindow::moveCursorEnd()
{
    QTextCursor cursor = m_pInput->textCursor();
    cursor.movePosition(QTextCursor::EndOfBlock);
    m_pInput->setTextCursor(cursor);
}

//-----------------------------------//

// Handles child widget events.
bool InputOutputWindow::eventFilter(QObject *obj, QEvent *event)
{
    if(obj == m_pInput)
    {
        if(event->type() == QEvent::KeyPress)
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if(keyEvent->key() == Qt::Key_Tab)
            {
                // Get the text from [m_pInput] and pass it off
                // to each specific IRC Window to handle.
                handleTab();
                return true;
            }
            else if(keyEvent->key() == Qt::Key_Return
                    || keyEvent->key() == Qt::Key_Enter)
            {
                QString text = getInputText();
                m_pastCommands.append(text);
                m_pInput->clear();

                InputEvent *inputEvt = new InputEvent(this, m_pSession, text);
                g_pEvtManager->fireEvent("input", m_pInput, inputEvt);
                delete inputEvt;

                return true;
            }
            else if(keyEvent->key() == Qt::Key_Up)
            {
                if(m_pastCommands.empty())
                {
                    QApplication::beep();
                    goto done;
                }

                m_pastCommands.prepend(getInputText());
                m_pInput->setPlainText(m_pastCommands.takeLast());

                moveCursorEnd();
            }
            else if(keyEvent->key() == Qt::Key_Down)
            {
                if(m_pastCommands.empty())
                {
                    QApplication::beep();
                    goto done;
                }

                m_pastCommands.append(getInputText());
                m_pInput->setPlainText(m_pastCommands.takeFirst());

                moveCursorEnd();
            }
        }

        return false;
    }
    else if(obj == m_pOutput && event->type() == QEvent::Resize)
    {
        m_pSharedServerConnPanel->realignPanel(m_pOpenButton);
    }

done:
    return OutputWindow::eventFilter(obj, event);
}

} } // End namespaces
