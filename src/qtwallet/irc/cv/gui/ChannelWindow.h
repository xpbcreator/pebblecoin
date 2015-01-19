// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// ChannelWindow is a Window that provides the user interface for
// chatting within an IRC channel. It owns input and output
// controls, as well as a list of users currently in the channel.
//
// QueuedOutputMessage represents a message that is to be displayed
// after the channel is "officially" joined; in other words, when the
// list of users is received from the server.

#pragma once

#include <QApplication>
#include <QString>
#include <QQueue>
#include "cv/ChannelUser.h"
#include "cv/gui/InputOutputWindow.h"

class QListWidget;
class QListWidgetItem;
class QSplitter;

namespace cv {

class Session;

namespace gui {

// This is subject to change.
const unsigned int MAX_NICK_PRIORITY = 1;

struct QueuedOutputMessage
{
    QString             message;
    OutputMessageType   messageType;
};

class ChannelWindow : public InputOutputWindow
{
    Q_OBJECT

protected:
    QListWidget *               m_pUserList;
    QSplitter *                 m_pSplitter;

    QList<ChannelUser *>        m_users;

    // These variables are used to keep track of
    // the set of nicks that are currently matches
    // against the autocomplete string.
    QList<ChannelUser *>        m_autocompleteMatches;
    int                         m_matchesIdx;
    QString                     m_preAutocompleteStr;
    QString                     m_postAutocompleteStr;

    // This variable dictates whether we are fully
    // in the channel or not; we are not fully in
    // a channel until the 366 numeric has been received
    // (end of /NAMES list).
    bool                        m_inChannel;
    QQueue<QueuedOutputMessage> m_messageQueue;

public:
    ChannelWindow(Session *pSession,
                  QExplicitlySharedDataPointer<ServerConnectionPanel> pSharedServerConnPanel,
                  const QString &title = tr("Untitled"),
                  const QSize &size = QSize(500, 300));
    ~ChannelWindow();

    bool isChannelName(const QString &name) { return (name.compare(getWindowName(), Qt::CaseInsensitive) == 0); }

    // Returns true if the user is in the channel, false otherwise.
    bool hasUser(const QString &user) { return (findUser(user) != NULL); }

    bool addUser(const QString &user);
    bool removeUser(const QString &user);
    void changeUserNick(const QString &oldNick, const QString &newNick);
    void addPrefixToUser(const QString &user, const QChar &prefixToAdd);
    void removePrefixFromUser(const QString &user, const QChar &prefixToRemove);
    QString fetchProperNickname(const QString &user);

    // Returns the number of users currently in the channel.
    int getUserCount() { return m_users.size(); }

    // Event callbacks
    void onNumericMessage(Event *pEvent);
    void onJoinMessage(Event *pEvent);
    void onKickMessage(Event *pEvent);
    void onModeMessage(Event *pEvent);
    void onNickMessage(Event *pEvent);
    void onNoticeMessage(Event *pEvent);
    void onPartMessage(Event *pEvent);
    void onPrivmsgMessage(Event *pEvent);
    void onTopicMessage(Event *pEvent);
    void onOutput(Event *pEvent);
    void onDoubleClickLink(Event *pEvent);
    void onColorConfigChanged(Event *pEvent);

    static void setupColorConfig(QMap<QString, ConfigOption> &defOptions);

protected:
    void setupColors();

    void enqueueMessage(const QString &msg, OutputMessageType msgType);
    void joinChannel();
    void leaveChannel();

    void handleSay(const QString &text);
    void handleAction(const QString &text);
    void handleTab();

    bool eventFilter(QObject *obj, QEvent *event);
    void closeEvent(QCloseEvent *event);

private:
    bool addUser(ChannelUser *pUser);
    void removeUser(ChannelUser *pUser);
    ChannelUser *findUser(const QString &user);

signals:
    // Signifies that the window is closing. This signal is only
    // used by StatusWindow.
    void chanWindowClosing(ChannelWindow *pWin);

public slots:
    void onUserDoubleClicked(QListWidgetItem *pItem);
};

} } // End namespaces
