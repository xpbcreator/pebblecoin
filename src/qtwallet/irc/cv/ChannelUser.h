// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// ChannelUser holds information about a user within an IRC channel,
// and is used by ChannelWindow.

#pragma once

#include <QString>

namespace cv {

class Session;

class ChannelUser
{
    Session *   m_pSession;

    // Message prefix: nick!user@host
    QString     m_nickname;
    QString     m_prefixes;
    QString     m_user;
    QString     m_host;

    // Priority of the nickname for tab-completion.
    int         m_priority;

public:
    ChannelUser(Session *pSession, const QString &nick);

    void addPrefix(const QChar &prefix);
    void removePrefix(const QChar &prefix);

    // These functions only manipulate the nickname, without any prefixes.
    void setNickname(const QString &nick) { m_nickname = nick; }
    QString getNickname() { return m_nickname; }

    QString getProperNickname();
    QString getFullNickname();
    QChar getPrefix();
    void setPriority(int p) { m_priority = p; }
    int getPriority() { return m_priority; }
};

} // End namespace
