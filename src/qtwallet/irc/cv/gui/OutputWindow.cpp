// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include <QApplication>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextLine>
#include <QAbstractTextDocumentLayout>
#include "cv/qext.h"
#include "cv/Parser.h"
#include "cv/Session.h"
#include "cv/ConfigManager.h"
#include "cv/EventManager.h"
#include "cv/gui/OutputWindow.h"
#include "cv/gui/OutputWindowScrollBar.h"
#include "cv/gui/WindowManager.h"

namespace cv { namespace gui {

/*
QString OutputWindow::s_invalidNickPrefix = "(^|[^A-z\\{\\|\\}])(";
QString OutputWindow::s_invalidNickSuffix = ")($|[^0-9A-z\\{\\|\\}\\-])";
*/
QString OutputWindow::s_invalidNickPrefix = "(^|[^A-Za-z0-9])(";
QString OutputWindow::s_invalidNickSuffix = ")($|[^A-Za-z0-9])";

//-----------------------------------//

OutputWindow::OutputWindow(const QString &title/* = tr("Untitled")*/,
                           const QSize &size/* = QSize(500, 300)*/)
    : Window(title, size),
      // TODO (seand): Remove hardcoded font
      m_defaultFont("Consolas", 10),
      m_outputAlertLevel(0)
{
    // TODO (seand): Choose an appropriate system default font...
    setFont(m_defaultFont);

    m_pCodec = QTextCodec::codecForLocale();

    m_pVLayout = new QVBoxLayout;

    /*
    m_pScrollBar = new OutputWindowScrollBar(this);
    m_pOutput->setVerticalScrollBar(m_pScrollBar);
    */
    m_pOutput = new OutputControl;
    g_pEvtManager->hookEvent("output", m_pOutput, MakeDelegate(this, &OutputWindow::onOutput));
    g_pEvtManager->hookEvent("doubleClickedLink", m_pOutput, MakeDelegate(this, &OutputWindow::onDoubleClickLink));
    //m_pOutput->setParentWindow(this);
}

//-----------------------------------//

OutputWindow::~OutputWindow()
{
    g_pEvtManager->unhookEvent("output", m_pOutput, MakeDelegate(this, &OutputWindow::onOutput));
    g_pEvtManager->unhookEvent("doubleClickedLink", m_pOutput, MakeDelegate(this, &OutputWindow::onDoubleClickLink));
}

//-----------------------------------//

void OutputWindow::printOutput(const QString &text,
                               OutputMessageType msgType,
                               OutputColor overrideMsgColor/* = COLOR_NONE*/,
                               int overrideAlertLevel/* = -1*/)
{
    OutputColor defaultMsgColor;
    int outputAlertLevel = 0;
    switch(msgType)
    {
        case MESSAGE_IRC_SAY:
            defaultMsgColor = COLOR_CHAT_FOREGROUND;
            outputAlertLevel = 2;
            break;
        case MESSAGE_IRC_SAY_SELF:
            defaultMsgColor = COLOR_CHAT_SELF;
            break;
        case MESSAGE_IRC_ACTION:
            defaultMsgColor = COLOR_ACTION;
            outputAlertLevel = 2;
            break;
        case MESSAGE_IRC_ACTION_SELF:
            defaultMsgColor = COLOR_ACTION;
            break;
        case MESSAGE_IRC_CTCP:
            defaultMsgColor = COLOR_CTCP;
            outputAlertLevel = 2;
            break;
        case MESSAGE_IRC_ERROR:
            defaultMsgColor = COLOR_ERROR;
            outputAlertLevel = 1;
            break;
        case MESSAGE_IRC_INVITE:
            defaultMsgColor = COLOR_INVITE;
            outputAlertLevel = 1;
            break;
        case MESSAGE_IRC_JOIN:
            defaultMsgColor = COLOR_JOIN;
            outputAlertLevel = 1;
            break;
        case MESSAGE_IRC_KICK:
            defaultMsgColor = COLOR_KICK;
            outputAlertLevel = 1;
            break;
        case MESSAGE_IRC_MODE:
            defaultMsgColor = COLOR_MODE;
            outputAlertLevel = 1;
            break;
        case MESSAGE_IRC_NICK:
            defaultMsgColor = COLOR_NICK;
            outputAlertLevel = 1;
            break;
        case MESSAGE_IRC_NOTICE:
            defaultMsgColor = COLOR_NOTICE;
            outputAlertLevel = 1;
            break;
        case MESSAGE_IRC_PART:
            defaultMsgColor = COLOR_PART;
            outputAlertLevel = 1;
            break;
        case MESSAGE_IRC_PING:
        case MESSAGE_IRC_PONG:
        case MESSAGE_IRC_NUMERIC:
            defaultMsgColor = COLOR_INFO;
            outputAlertLevel = 1;
            break;
        case MESSAGE_IRC_QUIT:
            defaultMsgColor = COLOR_QUIT;
            outputAlertLevel = 1;
            break;
        case MESSAGE_IRC_TOPIC:
            defaultMsgColor = COLOR_TOPIC;
            outputAlertLevel = 1;
            break;
        case MESSAGE_IRC_WALLOPS:
            defaultMsgColor = COLOR_WALLOPS;
            outputAlertLevel = 2;
            break;
        default:
            defaultMsgColor = COLOR_CHAT_FOREGROUND;
    }

    m_pOutput->appendMessage(text, (overrideMsgColor != COLOR_NONE) ? overrideMsgColor : defaultMsgColor);

    if(overrideMsgColor == COLOR_HIGHLIGHT)
        outputAlertLevel = 3;

    if(overrideAlertLevel >= 0)
        outputAlertLevel = overrideAlertLevel;

    QTreeWidgetItem *pItem = m_pManager->getItemFromWindow(this);
    if(pItem != NULL && !pItem->isSelected())
    {
        if(m_outputAlertLevel < outputAlertLevel)
        {
            m_outputAlertLevel = outputAlertLevel;
            QString color;
            switch(m_outputAlertLevel)
            {
                case 1:
                    color = "gray";
                    break;
                case 2:
                    color = "red";
                    break;
                case 3:
                    color = "green";
                    break;
            }

            pItem->setForeground(0, QBrush(QColor(color), Qt::SolidPattern));
        }
    }
}

//-----------------------------------//

void OutputWindow::printError(const QString &text)
{
    QString error = "[ERROR] ";
    error += text;
    printOutput(error, MESSAGE_ERROR);
}

//-----------------------------------//

void OutputWindow::printDebug(const QString &text)
{
    QString debug = "[DEBUG] ";
    debug += text;
    printOutput(debug, MESSAGE_DEBUG);
}

//-----------------------------------//

// Returns true if the user's nick is within the provided text, false otherwise.
bool OutputWindow::containsNick(const QString &text)
{
    QRegExp regex(OutputWindow::s_invalidNickPrefix
                + QRegExp::escape(m_pSession->getNick())
                + OutputWindow::s_invalidNickSuffix);
    regex.setCaseSensitivity(Qt::CaseInsensitive);
    return text.contains(regex);
}

//-----------------------------------//
/*
// Imitates Google Chrome's search, with lines drawn in the scrollbar
// and keywords highlighted in the document.
void OutputWindow::search(const QString &textToFind)
{
    // Reset the search lines.
    m_pScrollBar->clearLines();

    // Create the list of ExtraSelection items to set.
    QList<QTextEdit::ExtraSelection> list;

    QTextCursor cursor(m_pOutput->document());
    QTextBlock currBlock = m_pOutput->document()->begin();
    qreal currHeight = 0.0;
    qreal totalHeight = m_pOutput->document()->size().height();
    while(true)
    {
        cursor = m_pOutput->document()->find(textToFind, cursor);
        if(cursor.isNull())
            break;

        QTextEdit::ExtraSelection extraSel;
        extraSel.cursor = cursor;
        QTextCharFormat format;
        QColor color("red");
        QBrush brush(color);
        format.setBackground(brush);
        extraSel.format = format;
        list.append(extraSel);

        while(currBlock.isValid() && currBlock.blockNumber() != cursor.blockNumber())
        {
            // Keep track of the current block's height in the document.
            currHeight += m_pOutput->document()->documentLayout()->blockBoundingRect(currBlock).size().height();

            currBlock = currBlock.next();
        }

        // Add in the height of the QTextLine (where the cursor is located) within the QTextBlock.
        int relativePos = cursor.position() - cursor.block().position();
        QTextLine line = cursor.block().layout()->lineForTextPosition(relativePos);
        if(line.isValid())
        {
            // Add a search line to the scrollbar.
            //
            // TODO (seand): Add line.height()?
            m_pScrollBar->addLine((currHeight + line.y()) / totalHeight);
        }
    }

    m_pOutput->setExtraSelections(list);
}
*/
//-----------------------------------//

void OutputWindow::focusedInTree()
{
    m_outputAlertLevel = 0;
    QTreeWidgetItem *pItem = m_pManager->getItemFromWindow(this);
    pItem->setForeground(0, QBrush(QColor("black"), Qt::SolidPattern));
}

} } // End namespaces

