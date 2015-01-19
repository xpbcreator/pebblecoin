// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// Connection is a class that uses a QTcpSocket to connect to a
// server, and then uses the signals and slots system to broadcast
// events and data that has been received. The connectToHost() function
// is a blocking call.
//
// ThreadedConnection wraps a Connection instance in its own thread
// so that it can provide the same functionality via non-blocking functions.

#pragma once

#include <QThread>
#include <QMutex>
#include <QAbstractSocket>

class QTcpSocket;
class QTimer;

namespace cv {

class Connection : public QObject
{
    Q_OBJECT

    QTcpSocket *m_pSocket;
    QTimer *    m_pConnectionTimer;

public:
    Connection();
    ~Connection();

    bool isConnected();
    QAbstractSocket::SocketError error();

signals:
    void connecting();
    void connected();
    void disconnected();
    void connectionFailed();
    void dataReceived(const QString &data);

public slots:
    void connectToHost(const QString &host, quint16 port);
    void disconnectFromHost();
    void send(const QString &data);

    // These are connected to the socket and are called whenever
    // the socket emits the corresponding signal.
    void onConnect();
    void onConnectionTimeout();
    void onReadyRead();
};

//-----------------------------------//

class ThreadedConnection : public QThread
{
    Q_OBJECT

    Connection *    m_pConnection;
    QMutex          m_mutex;

public:
    ThreadedConnection(QObject *pParent = NULL);
    ~ThreadedConnection();

    void connectToHost(const QString &host, quint16 port);
    bool connectSSL() { return true; }
    void disconnectFromHost();
    bool isConnected();

    void send(const QString &data);

    QAbstractSocket::SocketError error();

protected:
    void run();

signals:
    // These are emitted from the ThreadedConnection class.
    void connecting();
    void connected();
    void disconnected();
    void connectionFailed();
    void dataReceived(const QString &data);

    // Signals to call into the alternate thread.
    void connectToHostSignal(const QString &host, quint16 port);
    void disconnectFromHostSignal();
    void sendSignal(const QString &data);
};

} // End namespace
