// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// This file holds a collection of utility functions used for parsing
// messages adhering to the IRC protocol. They are mostly used by
// Session and StatusWindow.

#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QTextDocument>

namespace cv {

enum CtcpRequestType
{
    RequestTypeInvalid, // Not valid CTCP request format.

    RequestTypeAction,
    RequestTypeVersion,
    RequestTypeTime,
    RequestTypeFinger,

    RequestTypeUnknown
};

//-----------------------------------//

enum ChanModeType
{
    ModeTypeUnknown,

    ModeTypeA,
    ModeTypeB,
    ModeTypeC,
    ModeTypeD
};

//-----------------------------------//

// Used to specify a channel mode.
struct ChannelMode
{
    bool    m_sign;
    QChar   m_mode;
    QString m_param;
};

//-----------------------------------//

// Used for parsing the message prefix.
enum MsgPrefixPart
{
    MsgPrefixName,
    MsgPrefixUser,
    MsgPrefixHost,
    MsgPrefixUserAndHost
};

//-----------------------------------//

// Lists all the possible non-numeric IRC commands.
enum
{
    IRC_COMMAND_UNKNOWN,

    // Alphabetical order
    IRC_COMMAND_ERROR,
    IRC_COMMAND_INVITE,
    IRC_COMMAND_JOIN,
    IRC_COMMAND_KICK,
    IRC_COMMAND_MODE,
    IRC_COMMAND_NICK,
    IRC_COMMAND_NOTICE,
    IRC_COMMAND_PART,
    IRC_COMMAND_PING,
    IRC_COMMAND_PONG,
    IRC_COMMAND_PRIVMSG,
    IRC_COMMAND_QUIT,
    IRC_COMMAND_TOPIC,
    IRC_COMMAND_WALLOPS
};

//-----------------------------------//

struct Message
{
    QString     m_prefix;
    bool        m_isNumeric;
    int         m_command;
    int         m_paramsNum;
    QStringList m_params;
};

//-----------------------------------//

Message parseData(const QString &data);

// Message-specific parsing
QString getNetworkNameFrom001(const Message &msg);
QString getIdleTextFrom317(const Message &msg);

QString stripCodes(const QString &text);
QString getHtmlColor(int number);

ChanModeType getChanModeType(const QString &chanModes, const QChar &letter);
QString getPrefixRules(const QString &param);
CtcpRequestType getCtcpRequestType(const Message &msg);
QString getNumericText(const Message &msg);
QString parseMsgPrefix(const QString &prefix, MsgPrefixPart part);
QString getDate(QString strUnixTime);
QString getTime(QString strUnixTime);
bool isChannel(const QString &str);

} // End namespace
