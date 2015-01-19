// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include "cv/ChannelUser.h"
#include "cv/Parser.h"
#include "cv/Session.h"

namespace cv {

// Parses the input nick into nickname, and prefixes and user/host (if applicable).
ChannelUser::ChannelUser(Session *pSession, const QString &nick)
    : m_pSession(pSession)
{
    m_nickname = nick.section('!', 0, 0);

    // Get the prefixes from the nickname (if any).
    int numPrefixes = 0;
    for(int i = 0; i < m_nickname.size() && m_pSession->isNickPrefix(m_nickname[i]); ++i)
    {
        m_prefixes += m_nickname[i];
        ++numPrefixes;
    }

    // Remove the prefixes from [m_nickname].
    if(numPrefixes > 0)
    {
        m_nickname.remove(0, numPrefixes);
    }

    // Get the user and host, if applicable.
    if(nick.indexOf('!') >= 0)
    {
        QString userAndHost = nick.section('!', 1);
        m_user = userAndHost.section('@', 0, 0);
        m_host = userAndHost.section('@', 1);
    }
}

//-----------------------------------//

// Adds the given prefix to the user, unless it's
// already there or it isn't a valid prefix.
void ChannelUser::addPrefix(const QChar &prefixToAdd)
{
    if(!m_pSession->isNickPrefix(prefixToAdd))
        return;

    // Insert the prefix in the right spot.
    for(int i = 0; i < m_prefixes.size(); ++i)
    {
        int compareVal = m_pSession->compareNickPrefixes(m_prefixes[i], prefixToAdd);
        if(compareVal > 0)
        {
            m_prefixes.insert(i, prefixToAdd);
            return;
        }
        else if(compareVal == 0)
        {
            return;
        }
    }

    m_prefixes.append(prefixToAdd);
}

//-----------------------------------//

// Removes the given prefix from the user, if it exists.
void ChannelUser::removePrefix(const QChar &prefix)
{
    m_prefixes.remove(prefix);
}

//-----------------------------------//

// Returns the nickname with the most powerful prefix (if any) prepended to it.
QString ChannelUser::getProperNickname()
{
    QString name;

    if(!m_prefixes.isEmpty())
    {
        name += m_prefixes[0];
    }
    name += m_nickname;

    return name;
}

//-----------------------------------//

// Returns the nickname with all prefixes (if any) prepended to it.
QString ChannelUser::getFullNickname()
{
    QString name;

    if(!m_prefixes.isEmpty())
    {
        name += m_prefixes;
    }
    name += m_nickname;

    return name;
}

//-----------------------------------//

// Returns the most powerful prefix of the nickname, or '\0' if there is none.
QChar ChannelUser::getPrefix()
{
    if(!m_prefixes.isEmpty())
    {
        return m_prefixes[0];
    }

    return '\0';
}

} // End namespace
