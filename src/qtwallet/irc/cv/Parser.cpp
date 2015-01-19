// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include <QtGui>
#include "cv/qext.h"
#include "cv/Parser.h"

namespace cv {

// Parses the data into a structure that holds all information
// necessary to print the message.
Message parseData(const QString &data)
{
    Message msg;

    // Make an index for the sections, start at 0.
    int sectionIndex = 0;

    // Isolate the prefix, if there is one.
    if(data[0] == ':')
    {
        // Get the prefix, remove the ':' from it.
        msg.m_prefix = data.section(' ', 0, 0, QString::SectionSkipEmpty);
        if(msg.m_prefix[0] == ':')
        {
            msg.m_prefix.remove(0, 1);
        }

        // Make the index start at section 1.
        ++sectionIndex;
    }

    // Get the command.
    QString command = data.section(' ', sectionIndex, sectionIndex, QString::SectionSkipEmpty);
    ++sectionIndex;

    // Decide which one it is.
    msg.m_command = command.toInt(&msg.m_isNumeric);
    if(!msg.m_isNumeric)
    {
        // The command isn't numeric, so we still have to search for it.
        if(command.compare("ERROR", Qt::CaseInsensitive) == 0)
        {
            msg.m_command = IRC_COMMAND_ERROR;
        }
        else if(command.compare("INVITE", Qt::CaseInsensitive) == 0)
        {
            msg.m_command = IRC_COMMAND_INVITE;
        }
        else if(command.compare("JOIN", Qt::CaseInsensitive) == 0)
        {
            msg.m_command = IRC_COMMAND_JOIN;
        }
        else if(command.compare("KICK", Qt::CaseInsensitive) == 0)
        {
            msg.m_command = IRC_COMMAND_KICK;
        }
        else if(command.compare("MODE", Qt::CaseInsensitive) == 0)
        {
            msg.m_command = IRC_COMMAND_MODE;
        }
        else if(command.compare("NICK", Qt::CaseInsensitive) == 0)
        {
            msg.m_command = IRC_COMMAND_NICK;
        }
        else if(command.compare("NOTICE", Qt::CaseInsensitive) == 0)
        {
            msg.m_command = IRC_COMMAND_NOTICE;
        }
        else if(command.compare("PART", Qt::CaseInsensitive) == 0)
        {
            msg.m_command = IRC_COMMAND_PART;
        }
        else if(command.compare("PING", Qt::CaseInsensitive) == 0)
        {
            msg.m_command = IRC_COMMAND_PING;
        }
        else if(command.compare("PONG", Qt::CaseInsensitive) == 0)
        {
            msg.m_command = IRC_COMMAND_PONG;
        }
        else if(command.compare("PRIVMSG", Qt::CaseInsensitive) == 0)
        {
            msg.m_command = IRC_COMMAND_PRIVMSG;
        }
        else if(command.compare("QUIT", Qt::CaseInsensitive) == 0)
        {
            msg.m_command = IRC_COMMAND_QUIT;
        }
        else if(command.compare("TOPIC", Qt::CaseInsensitive) == 0)
        {
            msg.m_command = IRC_COMMAND_TOPIC;
        }
        else if(command.compare("WALLOPS", Qt::CaseInsensitive) == 0)
        {
            msg.m_command = IRC_COMMAND_WALLOPS;
        }
        else
        {
            msg.m_command = IRC_COMMAND_UNKNOWN;
        }
    }

    // Get the parameters.
    int paramsIndex = 0;
    while(true)
    {
        // We want to divide it into parameters until we find one that starts with ':'
        // or until we find one that ends with '\r\n' (and then make that the last parameter).
        msg.m_params += data.section(' ', sectionIndex, sectionIndex, QString::SectionSkipEmpty);
        if((msg.m_params[paramsIndex])[0] == ':')
        {
            // We have to get the rest of the string, and trim it (take out the "\r\n"),
            // so that the last parameter is like all the others.
            msg.m_params[paramsIndex] = data.section(' ', sectionIndex, -1, QString::SectionSkipEmpty).trimmed();

            // Then remove the preceding colon.
            msg.m_params[paramsIndex].remove(0, 1);

            break;
        }
        else if(msg.m_params[paramsIndex].indexOf('\n') >= 0)
        {
            // In this case, we just have to trim it.
            msg.m_params[paramsIndex] = msg.m_params[paramsIndex].trimmed();

            break;
        }

        ++sectionIndex;
        ++paramsIndex;
    }

    msg.m_paramsNum = paramsIndex+1;

    return msg;
}

//-----------------------------------//

QString getNetworkNameFrom001(const Message &msg)
{
    // msg.m_params[0]: my nick
    // msg.m_params[1]: "Welcome to the <server name> IRC Network, <nick>[!user@host]"
    QString header = "Welcome to the ";
    if(msg.m_params[1].startsWith(header, Qt::CaseInsensitive))
    {
        int idx = msg.m_params[1].indexOf(' ', header.size(), Qt::CaseInsensitive);
        if(idx >= 0)
        {
            return msg.m_params[1].mid(header.size(), idx - header.size());
        }
    }

    return "";
}

//-----------------------------------//

QString getIdleTextFrom317(const Message &msg)
{
    // msg.m_params[0]: my nick
    // msg.m_params[1]: nick
    // msg.m_params[2]: seconds
    // two options here:
    //    1) msg.m_params[3]: "seconds idle"
    //    2) msg.m_params[3]: unix time
    // msg.m_params[4]: "seconds idle, signon time"

    QString idleText = "";

    // Get the number of idle seconds first, convert to h, m, s format.
    bool conversionOk;
    uint numSecs = msg.m_params[2].toInt(&conversionOk);
    if(conversionOk)
    {
        // 24 * 60 * 60 = 86400
        uint numDays = numSecs / 86400;
        if(numDays)
        {
            idleText += QString::number(numDays);
            if(numDays == 1)
            {
                idleText += "day ";
            }
            else
            {
                idleText += "days ";
            }
            numSecs = numSecs % 86400;
        }

        // 60 * 60 = 3600
        uint numHours = numSecs / 3600;
        if(numHours)
        {
            idleText += QString::number(numHours);
            if(numHours == 1)
            {
                idleText += "hr ";
            }
            else
            {
                idleText += "hrs ";
            }
            numSecs = numSecs % 3600;
        }

        uint numMinutes = numSecs / 60;
        if(numMinutes)
        {
            idleText += QString::number(numMinutes);
            if(numMinutes == 1)
            {
                idleText += "min ";
            }
            else
            {
                idleText += "mins ";
            }
            numSecs = numSecs % 60;
        }

        if(numSecs)
        {
            idleText += QString::number(numSecs);
            if(numSecs == 1)
            {
                idleText += "sec ";
            }
            else
            {
                idleText += "secs ";
            }
        }

        // Remove the trailing space.
        idleText.remove(idleText.size()-1, 1);

        // Right now this will only support 5 parameters
        // (1 extra for the signon time), but support for more
        // can be easily added later.
        if(msg.m_paramsNum > 4)
        {
            idleText += QString(", signed on %1 %2")
                        .arg(getDate(msg.m_params[3]))
                        .arg(getTime(msg.m_params[3]));
        }
    }

    return idleText;
}

//-----------------------------------//

// Returns the completely stripped version of the text,
// so it doesn't contain any bold, underline, color, or other
// control codes.
QString stripCodes(const QString &text)
{
    QString strippedText;
    for(int i = 0; i < text.size(); ++i)
    {
        switch(text[i].toAscii())
        {
            case 1:     // CTCP stuff
            case 2:     // bold
            case 15:    // causes formatting to return to normal
            case 22:    // reverse
            case 31:    // underline
            {
                break;
            }
            case 3:	// color
            {
                // Follows mIRC's method for coloring, where the
                // foreground color comes first (up to two digits),
                // and the optional background color comes last (up
                // to two digits) and they are separated by a single comma.
                //
                // Example: '\3'05,02
                //
                // Max length of color specification is 5 (4 numbers and 1 comma).
                ++i;
                for(int j = 0; j < 2; ++j, ++i)
                {
                    if(i >= text.size())
                        goto end_color_spec;
                    if(!text[i].isDigit())
                    {
                        if(j > 0 && text[i] == ',')
                            break;
                        goto end_color_spec;
                    }
                }

                if(i >= text.size() || text[i] != ',')
                {
                    goto end_color_spec;
                }

                ++i;
                for(int j = 0; j < 2; ++j, ++i)
                {
                    if(i >= text.size())
                        goto end_color_spec;
                    if(!text[i].isDigit())
                    {
                        goto end_color_spec;
                    }
                }

            end_color_spec:
                if(i < text.size())
                    --i;
                break;
            }
            default:
            {
                strippedText += text[i];
            }
        }
    }

    return strippedText;
}

//-----------------------------------//

// Returns the color corresponding to the 1- or 2-digit
// number in the format "#XXXXXX" so it can be used in HTML.
//
// If the number passed is invalid, it returns an empty string.
QString getHtmlColor(int number)
{
    switch(number)
    {
        case 0: // white
        {
            return "#FFFFFF";
        }
        case 1: // black
        {
            return "#000000";
        }
        case 2: // navy blue
        {
            return "#000080";
        }
        case 3: // green
        {
            return "#008000";
        }
        case 4: // red
        {
            return "#FF0000";
        }
        case 5: // maroon
        {
            return "#800000";
        }
        case 6: // purple
        {
            return "#800080";
        }
        case 7: // orange
        {
            return "#FFA500";
        }
        case 8: // yellow
        {
            return "#FFFF00";
        }
        case 9: // lime green
        {
            return "#00FF00";
        }
        case 10:    // teal
        {
            return "#008080";
        }
        case 11:    // cyan/aqua
        {
            return "#00FFFF";
        }
        case 12:    // blue
        {
            return "#0000FF";
        }
        case 13:    // fuschia
        {
            return "#FF00FF";
        }
        case 14:    // gray
        {
            return "#808080";
        }
        case 15:    // silver
        {
            return "#C0C0C0";
        }
        default:
        {
            return "";
        }
    }
}

//-----------------------------------//

// Returns the type of the channel mode.
ChanModeType getChanModeType(const QString &chanModes, const QChar &letter)
{
    if(chanModes.section(',', 0, 0).contains(letter))
        return ModeTypeA;
    else if(chanModes.section(',', 1, 1).contains(letter))
        return ModeTypeB;
    else if(chanModes.section(',', 2, 2).contains(letter))
        return ModeTypeC;
    else if(chanModes.section(',', 3, 3).contains(letter))
        return ModeTypeD;

    return ModeTypeUnknown;
}

//-----------------------------------//

// Checks for the PREFIX section in a 005 numeric, and if
// it exists, it parses it and returns it.
QString getPrefixRules(const QString &param)
{
    QString prefixRules = "o@v+";

    // Examples:
    //	PREFIX=(qaohv)~&@%+
    //	PREFIX=(ov)@+
    //
    // We have two indices for param:
    //	i starts after (
    //	j starts after )
    int i = param.indexOf('(') + 1;
    if(i <= 0)
        return prefixRules;

    int j = param.indexOf(')', i) + 1;
    if(j <= 0)
        return prefixRules;

    // Iterate over the parameter.
    prefixRules.clear();
    int paramSize = param.size();
    for(; param[i] != ')' && j < paramSize; ++i, ++j)
    {
        prefixRules += param[i];
        prefixRules += param[j];
    }

    // Examples of prefixRules:
    //	q~a&o@h%v+
    //	o@v+
    return prefixRules;
}

//-----------------------------------//

// Returns the specific CtcpRequestType of the message.
CtcpRequestType getCtcpRequestType(const Message &msg)
{
    // It has to be a private message.
    if(msg.m_isNumeric || msg.m_command != IRC_COMMAND_PRIVMSG)
    {
        return RequestTypeInvalid;
    }

    QString text = msg.m_params[1];
    if(text[0] != '\1' || text[text.size()-1] != '\1')
    {
        return RequestTypeInvalid;
    }

    // Identify the specific command.
    if(text.startsWith("\1action ", Qt::CaseInsensitive))
    {
        return RequestTypeAction;
    }
    else if(text.compare("\1version\1", Qt::CaseInsensitive) == 0)
    {
        return RequestTypeVersion;
    }
    else if(text.compare("\1time\1", Qt::CaseInsensitive) == 0)
    {
        return RequestTypeTime;
    }
    else if(text.compare("\1finger\1", Qt::CaseInsensitive) == 0)
    {
        return RequestTypeFinger;
    }

    return RequestTypeUnknown;
}

//-----------------------------------//

// Forms the text that can be printed to the output for all numeric commands.
QString getNumericText(const Message &msg)
{
    QString text;

    // Ignore the first parameter, which is the user's name.
    for(int i = 1; i < msg.m_paramsNum; ++i)
    {
        text += msg.m_params[i];
        text += ' ';
    }

    return text;
}

//-----------------------------------//

// Prases the msg prefix to get a specified [part].
QString parseMsgPrefix(const QString &prefix, MsgPrefixPart part)
{
    // Prefix format: <servername> | <nick> [ '!' <user> ] [ '@' <host> ]
    if(part == MsgPrefixName)
    {
        return prefix.section('!', 0, 0);
    }
    else if(part == MsgPrefixUserAndHost)
    {
        return prefix.section('!', 1);
    }
    else
    {
        QString userAndHost = prefix.section('!', 1);

        if(part == MsgPrefixUser)
        {
            return userAndHost.section('@', 0, 0);
        }
        else    // [part] == MsgPrefixHost
        {
            return userAndHost.section('@', 1);
        }
    }
}

//-----------------------------------//

// Returns the date based on the string representation
// of unix time. If the string passed is not a valid number or
// is not in unix time, it returns an empty string.
QString getDate(QString strUnixTime)
{
    bool conversionOk;
    uint unixTime = strUnixTime.toInt(&conversionOk);
    if(conversionOk)
    {
        QDateTime dt;
        dt.setTime_t(unixTime);
        return dt.toString("ddd MMM dd");
    }

    return "";
}

//-----------------------------------//

// Returns the time based on the string representation
// of unix time. If the string passed is not a valid number or
// is not in unix time, it returns an empty string.
QString getTime(QString strUnixTime)
{
    bool conversionOk;
    uint unixTime = strUnixTime.toInt(&conversionOk);
    if(conversionOk)
    {
        QDateTime dt;
        dt.setTime_t(unixTime);
        return dt.toString("hh:mm:ss");
    }

    return "";
}

//-----------------------------------//

// Used to differentiate between a channel and a nickname.
//
// Returns true if [str] is a valid name for a channel, false otherwise.
bool isChannel(const QString &str)
{
    switch(str[0].toAscii())
    {
        case '#':
        case '&':
        case '+':
        case '!':
            return true;
        default:
            return false;
    }
}

} // End namespace
