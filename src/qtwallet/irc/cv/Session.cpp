// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include <QCoreApplication>
#include <QDebug>
#include "cv/Session.h"
#include "cv/Parser.h"

namespace cv {

Session::Session(const QString& nick)
  : m_nick(nick),
    m_prevData("")
{
    m_pConn = new ThreadedConnection;
    QObject::connect(m_pConn, SIGNAL(connecting()), this, SLOT(onConnecting()));
    QObject::connect(m_pConn, SIGNAL(connected()), this, SLOT(onConnect()));
    QObject::connect(m_pConn, SIGNAL(connectionFailed()), this, SLOT(onFailedConnect()));
    QObject::connect(m_pConn, SIGNAL(disconnected()), this, SLOT(onDisconnect()));
    QObject::connect(m_pConn, SIGNAL(dataReceived(QString)), this, SLOT(onReceiveData(QString)));

    g_pEvtManager->createEvent("connecting");
    g_pEvtManager->createEvent("connectFailed");
    g_pEvtManager->createEvent("connected");
    g_pEvtManager->createEvent("disconnected");
    g_pEvtManager->createEvent("sendData");
    g_pEvtManager->createEvent("receivedData");
    g_pEvtManager->createEvent("errorMessage");
    g_pEvtManager->createEvent("inviteMessage");
    g_pEvtManager->createEvent("joinMessage");
    g_pEvtManager->createEvent("kickMessage");
    g_pEvtManager->createEvent("modeMessage");
    g_pEvtManager->createEvent("nickMessage");
    g_pEvtManager->createEvent("noticeMessage");
    g_pEvtManager->createEvent("partMessage");
    g_pEvtManager->createEvent("pongMessage");
    g_pEvtManager->createEvent("privmsgMessage");
    g_pEvtManager->createEvent("quitMessage");
    g_pEvtManager->createEvent("topicMessage");
    g_pEvtManager->createEvent("wallopsMessage");
    g_pEvtManager->createEvent("numericMessage");
    g_pEvtManager->createEvent("unknownMessage");

    g_pEvtManager->hookEvent("sendData", this, MakeDelegate(this, &Session::onSendData));
}

//-----------------------------------//

Session::~Session()
{
    delete m_pConn;
    g_pEvtManager->unhookAllEvents(this);
}

//-----------------------------------//

void Session::connectToServer(const QString &host, int port)
{
    connectToServer(host, port, "conviersa", "conviersa");
}

//-----------------------------------//

void Session::connectToServer(const QString &host, int port, const QString &name, const QString &nick)
{
    m_name = name;
    m_nick = nick;
    m_host = host;
    m_port = port;

    // This defaults the server's prefix rules (for users in a channel)
    // to @ for +o (oper), and + for +v (voice); this is just in case
    // a server does not send explicit prefix rules to the client.
    //
    // For more info see the definition in Session.h.
    m_prefixRules = "o@v+";

    m_pConn->connectToHost(host, port);
}

//-----------------------------------//

void Session::disconnectFromServer()
{
    if(isConnected())
        m_pConn->disconnectFromHost();
}

//-----------------------------------//

void Session::sendData(const QString &data)
{
    DataEvent *pEvt = new DataEvent(data);
    g_pEvtManager->fireEvent("sendData", this, pEvt);
    delete pEvt;
}

//-----------------------------------//

// Default functionality for the sendData event; it just sends the data.
void Session::onSendData(Event *pEvt)
{
    m_pConn->send(DCAST(DataEvent, pEvt)->getData() + "\r\n");
}

//-----------------------------------//

void Session::sendPrivmsg(const QString &target, const QString &msg)
{
    sendData(QString("PRIVMSG %1 :%2").arg(target).arg(msg));
}

//-----------------------------------//

void Session::sendAction(const QString &target, const QString &msg)
{
    sendData(QString("PRIVMSG %1 :\1ACTION %2\1").arg(target).arg(msg));
}

//-----------------------------------//

// Sets the prefix rules supported by the server.
void Session::setPrefixRules(const QString &prefixRules)
{
    m_prefixRules = prefixRules;

    // Stick these modes in with the type B channel modes.
    if(!m_chanModes.isEmpty())
    {
        int index = m_chanModes.indexOf(',');
        if(index >= 0)
        {
            for(int i = 0; i < m_prefixRules.size(); i += 2)
                m_chanModes.insert(index+1, m_prefixRules[i]);
        }
    }
}

//-----------------------------------//

// Sets the channel modes supported by the server.
void Session::setChanModes(const QString &chanModes)
{
    m_chanModes = chanModes;

    // Stick these modes in with the type B channel modes.
    //
    // It's done in both functions to ensure the right ones are added.
    int index = m_chanModes.indexOf(',');
    if(index >= 0)
    {
        for(int i = 0; i < m_prefixRules.size(); i += 2)
            m_chanModes.insert(index+1, m_prefixRules[i]);
    }
}

//-----------------------------------//

// Compares two prefix characters in a nickname as per the server's specification.
//
// Returns:
//  -1 if prefix1 is less in value than prefix2
//  0 if prefix1 is equal to prefix2
//  1 if prefix1 is greater in value than prefix2
int Session::compareNickPrefixes(const QChar &prefix1, const QChar &prefix2)
{
    if(prefix1 == prefix2)
        return 0;

    for(int i = 1; i < m_prefixRules.size(); i += 2)
    {
        if(m_prefixRules[i] == prefix1)
            return -1;

        if(m_prefixRules[i] == prefix2)
            return 1;
    }

    return 0;
}

//-----------------------------------//

// Returns the corresponding prefix rule to the character provided by [match].
// It can either be a nick prefix or the corresponding mode.
QChar Session::getPrefixRule(const QChar &match)
{
    for(int i = 0; i < m_prefixRules.size(); ++i)
    {
        if(m_prefixRules[i] == match)
        {
            // If it's even, the corresponding letter is at i+1.
            if(i % 2 == 0)
                return m_prefixRules[i+1];
            // Otherwise, the corresponding letter is at i-1.
            else
                return m_prefixRules[i-1];
        }
    }

    return '\0';
}

//-----------------------------------//

// Returns true if the character is a set prefix for the server, false otherwise.
//
// Example prefixes: @, %, +
bool Session::isNickPrefix(const QChar &prefix)
{
    // The first prefix begins at index 1.
    for(int i = 1; i < m_prefixRules.size(); i += 2)
    {
        if(m_prefixRules[i] == prefix)
            return true;
    }

    return false;
}

//-----------------------------------//

// Handles the preliminary processing for all messages;
// this will fire events for specific message types, and store
// some information as a result of others (like numerics).
void Session::processMessage(const Message &msg)
{
    Event *pEvent = new MessageEvent(msg);
    if(msg.m_isNumeric)
    {
        switch(msg.m_command)
        {
            case 1:
            {
                // Check to make sure nickname hasn't changed; some or all servers apparently don't
                // send you a NICK message when your nickname conflicts with another user upon
                // first entering the server, and you try to change it.
                if(!isMyNick(msg.m_params[0]))
                    setNick(msg.m_params[0]);

                break;
            }
            case 2:
            {
                // msg.m_params[0]: my nick
                // msg.m_params[1]: "Your host is ..."
                QString header = "Your host is ";
                QString hostStr = msg.m_params[1].section(',', 0, 0);
                if(hostStr.startsWith(header))
                {
                    setHost(hostStr.mid(header.size()));
                }

                break;
            }
            case 5:
            {
                // We only go to the second-to-last parameter, because the
                // last parameter holds "are supported by this server".
                for(int i = 1; i < msg.m_paramsNum-1; ++i)
                {
                    if(msg.m_params[i].startsWith("PREFIX=", Qt::CaseInsensitive))
                    {
                        setPrefixRules(getPrefixRules(msg.m_params[i]));
                    }
                    else if(msg.m_params[i].compare("NAMESX", Qt::CaseInsensitive) == 0)
                    {
                        // Lets the server know we support multiple nick prefixes.
                        //
                        // TODO (seand): Implement UHNAMES?
                        sendData("PROTOCTL NAMESX");
                    }
                    else if(msg.m_params[i].startsWith("CHANMODES=", Qt::CaseInsensitive))
                    {
                        setChanModes(msg.m_params[i].section('=', 1));
                    }
                }

                break;
            }
        }

        g_pEvtManager->fireEvent("numericMessage", this, pEvent);
    }
    else
    {
        switch(msg.m_command)
        {
            case IRC_COMMAND_ERROR:
            {
                g_pEvtManager->fireEvent("errorMessage", this, pEvent);
                break;
            }
            case IRC_COMMAND_INVITE:
            {
                g_pEvtManager->fireEvent("inviteMessage", this, pEvent);
                break;
            }
            case IRC_COMMAND_JOIN:
            {
                g_pEvtManager->fireEvent("joinMessage", this, pEvent);
                break;
            }
            case IRC_COMMAND_KICK:
            {
                g_pEvtManager->fireEvent("kickMessage", this, pEvent);
                break;
            }
            case IRC_COMMAND_MODE:
            {
                g_pEvtManager->fireEvent("modeMessage", this, pEvent);
                break;
            }
            case IRC_COMMAND_NICK:
            {
                g_pEvtManager->fireEvent("nickMessage", this, pEvent);

                // update the user's nickname if he's the one changing it
                QString oldNick = parseMsgPrefix(msg.m_prefix, MsgPrefixName);
                if(isMyNick(oldNick))
                {
                    setNick(msg.m_params[0]);
                }
                break;
            }
            case IRC_COMMAND_NOTICE:
            {
                g_pEvtManager->fireEvent("noticeMessage", this, pEvent);
                break;
            }
            case IRC_COMMAND_PART:
            {
                g_pEvtManager->fireEvent("partMessage", this, pEvent);
                break;
            }
            case IRC_COMMAND_PING:
            {
                sendData("PONG :" + msg.m_params[0]);
                break;
            }
            case IRC_COMMAND_PONG:
            {
                g_pEvtManager->fireEvent("pongMessage", this, pEvent);
                break;
            }
            case IRC_COMMAND_PRIVMSG:
            {
                g_pEvtManager->fireEvent("privmsgMessage", this, pEvent);
                break;
            }
            case IRC_COMMAND_QUIT:
            {
                g_pEvtManager->fireEvent("quitMessage", this, pEvent);
                break;
            }
            case IRC_COMMAND_TOPIC:
            {
                g_pEvtManager->fireEvent("topicMessage", this, pEvent);
                break;
            }
            case IRC_COMMAND_WALLOPS:
            {
                g_pEvtManager->fireEvent("wallopsMessage", this, pEvent);
                break;
            }
            default:
            {
                g_pEvtManager->fireEvent("unknownMessage", this, pEvent);
            }
        }
    }

    delete pEvent;
}

//-----------------------------------//

void Session::onConnecting()
{
    ConnectionEvent *pEvt = new ConnectionEvent(m_host, m_port);
    g_pEvtManager->fireEvent("connecting", this, pEvt);
    delete pEvt;
}

//-----------------------------------//

void Session::onConnect()
{
    // TODO (seand): Use config options.
    sendData(QString("NICK %1").arg(m_nick));
    sendData(QString("USER %1 tolmoon \"%2\" :%3").arg(m_nick).arg(m_host).arg(m_name));
    
    QString toJoin = m_name;
    if(!toJoin.isEmpty() && toJoin[0] != '#')
        toJoin.prepend('#');

    sendData(QString("JOIN %1").arg(toJoin));

    ConnectionEvent *pEvt = new ConnectionEvent(m_host, m_port);
    g_pEvtManager->fireEvent("connected", this, pEvt);
    delete pEvt;
}

//-----------------------------------//

void Session::onFailedConnect()
{
    ConnectionEvent *pEvt = new ConnectionEvent(m_host, m_port, m_pConn->error());
    g_pEvtManager->fireEvent("connectFailed", this, pEvt);
    delete pEvt;
}

//-----------------------------------//

void Session::onDisconnect()
{
    ConnectionEvent *pEvt = new ConnectionEvent(m_host, m_port);
    g_pEvtManager->fireEvent("disconnected", this, pEvt);
    delete pEvt;
}

//-----------------------------------//

void Session::onReceiveData(const QString &data)
{
    m_prevData += data;

    // Check for a message within the data buffer, to ensure only whole messages are handled.
    int numChars;
    while((numChars = m_prevData.indexOf('\n') + 1) > 0)
    {
        // Retrieve the entire message up to and including the terminating '\n' character.
        QString msgData = m_prevData.left(numChars);
        m_prevData.remove(0, numChars);

        Event *pEvent = new DataEvent(msgData);
        g_pEvtManager->fireEvent("receivedData", this, pEvent);
        delete pEvent;

        Message msg = parseData(msgData);
        processMessage(msg);
    }
}

} // End namespace
