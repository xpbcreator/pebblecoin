// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// Session maintains the connection to an IRC server, passes data
// to and from a ThreadedConnection instance, and stores IRC-specific
// information about the connected user and the server. It utilizes the
// EventManager to pass data to all monitoring Windows.
//
// Events:
//
// ConnectionEvent is used for the "connecting", "connected", "disconnected",
// and "connectFailed" events.
//
// DataEvent is used for the "receivedData" event, which is fired whenever the
// Session receives any data from the server.
//
// MessageEvent is used for all the events that end in "Message", which are fired
// for successfully parsing received data into a Message object.

#pragma once

#include <QObject>
#include <QString>
#include <QSharedData>
#include <QTime>
#include "cv/Connection.h"
#include "cv/Parser.h"
#include "cv/EventManager.h"

namespace cv {

class ConnectionEvent : public Event
{
    QString *   m_pHost;
    quint16     m_port;

    // Only relevant for the "connectFailed" event.
    QAbstractSocket::SocketError m_error;

public:
    ConnectionEvent(QString &host, quint16 port)
      : m_pHost(&host),
        m_port(port)
    { }
    ConnectionEvent(QString &host, quint16 port, QAbstractSocket::SocketError error)
      : m_pHost(&host),
        m_port(port),
        m_error(error)
    { }

    QString getHost() { return *m_pHost; }
    quint16 getPort() { return m_port; }
    QAbstractSocket::SocketError error() { return m_error; }
};

//-----------------------------------//

class MessageEvent : public Event
{
    Message m_msg;

public:
    MessageEvent(const Message &msg)
      : m_msg(msg)
    { }

    Message getMessage() { return m_msg; }
};

//-----------------------------------//

class DataEvent : public Event
{
    QString m_data;

public:
    DataEvent(const QString &data)
      : m_data(data)
    { }

    QString getData() { return m_data; }
};

//-----------------------------------//

class Session : public QObject, public QSharedData
{
    Q_OBJECT

private:
    // The actual connection to the server.
    ThreadedConnection *   m_pConn;

    // User's nickname.
    QString             m_nick;

    // Name of the host that we are connected to.
    QString             m_host;

    // Port number of the server we're connected to.
    int                 m_port;

    // User's name (used for USER message).
    QString             m_name;

    // Prefix rules as dictated by the server.
    //
    // Format: <mode1><prefix1><mode2><prefix2>[<mode3><prefix3>] ...
    // Default value: o@v+
    QString             m_prefixRules;

    // This comes from the 005 numeric, CHANMODES, which specifies
    // which channel modes the server supports, and which ones take
    // a parameter and which don't.
    //
    // Format: typeA,typeB,typeC,typeD
    QString             m_chanModes;

    // This comes from the 005 numeric, MODES, which dictates
    // the maximum number of modes with parameters that may be set with
    // one message.
    int                 m_modeNum;

    // This acts as a more persistent buffer for receiving data from
    // the Connection object, so that it can be separated by '\n' and
    // then parsed.
    QString             m_prevData;

public:
    Session(const QString& nick);
    ~Session();

    void connectToServer(const QString &host, int port);
    void connectToServer(const QString &host, int port, const QString &name, const QString &nick);
    void disconnectFromServer();
    bool isConnected() { return m_pConn->isConnected(); }

    // Exposed functions for sending messages.
    void sendData(const QString &data);
    void onSendData(Event *pEvt);
    void sendPrivmsg(const QString &target, const QString &msg);
    void sendAction(const QString &target, const QString &msg);

    void setHost(const QString &host) { m_host = host; }
    QString getHost() { return m_host; }
    int getPort() { return m_port; }
    void setNick(const QString &nick) { m_nick = nick; }
    QString getNick() { return m_nick; }
    bool isMyNick(const QString &nick) { return (m_nick.compare(nick, Qt::CaseSensitive) == 0); }

    void setPrefixRules(const QString &prefixRules);
    void setChanModes(const QString &chanModes);
    QString getChanModes() { return m_chanModes; }
    void setModeNum(int modeNum) { m_modeNum = modeNum; }
    int getModeNum() { return m_modeNum; }

    int compareNickPrefixes(const QChar &prefix1, const QChar &prefix2);
    QChar getPrefixRule(const QChar &match);
    bool isNickPrefix(const QChar &prefix);

    void processMessage(const Message &msg);

signals:
    void connectToHost(QString, quint16);

public slots:
    // These are connected to the Connection object and are called when
    // Connection emits them.
    void onConnecting();
    void onConnect();
    void onFailedConnect();
    void onDisconnect();
    void onReceiveData(const QString &data);
};

} // End namespace
