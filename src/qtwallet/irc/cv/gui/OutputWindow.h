// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// OutputWindow is a Window which owns an OutputControl and contains
// a Session, so it is the base class from which all IRC windows with
// output derive.

#pragma once

#include <QVBoxLayout>
#include <QPlainTextEdit>
#include "cv/gui/Window.h"
#include "cv/gui/OutputControl.h"

class QTextEdit;

namespace cv {

class Session;

namespace gui {

class OutputWindowScrollBar;
class OutputControl;

//-----------------------------------//

enum OutputMessageType {

    // IRC types
    MESSAGE_IRC_SAY,
    MESSAGE_IRC_SAY_SELF,
    MESSAGE_IRC_ACTION,
    MESSAGE_IRC_ACTION_SELF,
    MESSAGE_IRC_CTCP,

    MESSAGE_IRC_ERROR,
    MESSAGE_IRC_INVITE,
    MESSAGE_IRC_JOIN,
    MESSAGE_IRC_KICK,
    MESSAGE_IRC_MODE,
    MESSAGE_IRC_NICK,
    MESSAGE_IRC_NOTICE,
    MESSAGE_IRC_PART,
    MESSAGE_IRC_PING,
    MESSAGE_IRC_PONG,
    MESSAGE_IRC_QUIT,
    MESSAGE_IRC_TOPIC,
    MESSAGE_IRC_WALLOPS,

    MESSAGE_IRC_NUMERIC,

    // General client types
    MESSAGE_INFO,
    MESSAGE_ERROR,
    MESSAGE_DEBUG,

    // Custom type
    MESSAGE_CUSTOM

};

//-----------------------------------//

class OutputWindow : public Window
{
    Q_OBJECT

protected:
    QVBoxLayout *           m_pVLayout;

    int                     m_startOfText;
    OutputControl *         m_pOutput;
    QFont                   m_defaultFont;

    Session *               m_pSession;

    // This variable holds the most important level
    // of output alert for a window not in focus; this is
    // used to determine what color to make an OutputWindow's
    // item in the WindowManager when it receives text
    // and isn't in focus.
    int                     m_outputAlertLevel;

    QTextCodec *            m_pCodec;

    // Custom scrollbar used for searching within an OutputWindow;
    // lines on which items are found will be drawn inside
    // the slider area (proportional to the entire height of the
    // OutputControl's text.
    OutputWindowScrollBar * m_pScrollBar;

    static QString          s_invalidNickPrefix;
    static QString          s_invalidNickSuffix;

public:
    OutputWindow(const QString &title = tr("Untitled"),
                 const QSize &size = QSize(500, 300));
    ~OutputWindow();

    // Printing functions
    void printOutput(const QString &text,
                     OutputMessageType msgType,
                     OutputColor overrideMsgColor = COLOR_NONE,
                     int overrideAlertLevel = -1);
    void printError(const QString &text);
    void printDebug(const QString &text);

    bool containsNick(const QString &text);
    void focusedInTree();

    virtual void onOutput(Event *pEvent) = 0;
    virtual void onDoubleClickLink(Event *pEvent) = 0;

protected:
    // Imitates Google Chrome's search, with lines drawn in the scrollbar
    // and keywords highlighted in the document.
    //void search(const QString &textToFind);
};

//-----------------------------------//

} } // End namespaces
