// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// QueryWindow is a Window which owns input and output controls, and provides
// a user interface for privately chatting with another user on an IRC server.

#pragma once

#include <QApplication>
#include "cv/gui/InputOutputWindow.h"

namespace cv {

class Session;

namespace gui {

class QueryWindow : public InputOutputWindow
{
    Q_OBJECT

private:
    QString     m_targetNick;

public:
    QueryWindow(Session *pSession,
                QExplicitlySharedDataPointer<ServerConnectionPanel> pSharedServerConnPanel,
                const QString &targetNick);
    ~QueryWindow();

    void setTargetNick(const QString &nick);
    QString getTargetNick();
    bool isTargetNick(const QString &nick) { return (m_targetNick.compare(nick, Qt::CaseSensitive) == 0); }

    // Event callbacks
    void onNumericMessage(Event *pEvent);
    void onNickMessage(Event *pEvent);
    void onNoticeMessage(Event *pEvent);
    void onPrivmsgMessage(Event *pEvent);

    void onOutput(Event *pEvent);
    void onDoubleClickLink(Event *pEvent);

protected:
    void handleSay(const QString &text);
    void handleAction(const QString &text);
    void handleTab();

    void closeEvent(QCloseEvent *event);

signals:
    // Signifies that the window is closing. This is only
    // used by StatusWindow.
    void privWindowClosing(QueryWindow *pWin);
};

} } // End namespaces
