// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include <QTextCodec>
#include <QTcpSocket>
#include <QSslSocket>
#include <QMutexLocker>
#include <QTimer>

#include "cv/Connection.h"

namespace cv {

const int CONFIG_CONNECTION_TIMEOUT_MSEC = 5000;

//-----------------------------------//
//-----------------------------------//
//----------              -----------//
//----------  Connection  -----------//
//----------              -----------//
//-----------------------------------//
//-----------------------------------//

Connection::Connection()
{
    m_pSocket = new QTcpSocket;
    m_pConnectionTimer = new QTimer;
    m_pConnectionTimer->setSingleShot(true);

    // Connects the necessary signals to each corresponding slot.
    QObject::connect(m_pSocket, SIGNAL(connected()), this, SLOT(onConnect()));
    QObject::connect(m_pSocket, SIGNAL(disconnected()), this, SIGNAL(disconnected()));
    QObject::connect(m_pSocket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    QObject::connect(m_pConnectionTimer, SIGNAL(timeout()), this, SLOT(onConnectionTimeout()));
}

//-----------------------------------//

Connection::~Connection()
{
    delete m_pSocket;
    delete m_pConnectionTimer;
}

//-----------------------------------//

bool Connection::isConnected()
{
    return (m_pSocket->state() == QAbstractSocket::ConnectedState);
}

//-----------------------------------//

void Connection::connectToHost(const QString &host, quint16 port)
{
    m_pConnectionTimer->stop();
    m_pSocket->abort();

    m_pSocket->connectToHost(host, port);
    emit connecting();
    m_pConnectionTimer->start(CONFIG_CONNECTION_TIMEOUT_MSEC);
}

//-----------------------------------//

void Connection::disconnectFromHost()
{
    m_pSocket->disconnectFromHost();
}

//-----------------------------------//

void Connection::send(const QString &data)
{
    if(!isConnected())
        return;

    m_pSocket->write(data.toUtf8());
}

//-----------------------------------//

QAbstractSocket::SocketError Connection::error()
{
    return m_pSocket->error();
}

//-----------------------------------//

void Connection::onConnect()
{
    m_pConnectionTimer->stop();
    emit connected();
}

//-----------------------------------//

void Connection::onConnectionTimeout()
{
    if(m_pSocket->waitForConnected(0))
        emit connected();
    else
        emit connectionFailed();
}

//-----------------------------------//

const int SOCKET_BUFFER_SIZE = 1024;

void Connection::onReadyRead()
{
    while(true)
    {
        char buffer[SOCKET_BUFFER_SIZE];
        qint64 size = m_pSocket->read(buffer, SOCKET_BUFFER_SIZE-1);

        if(size > 0)
        {
            buffer[size] = '\0';
            QString data = QString::fromUtf8(buffer);
            emit dataReceived(data);
        }
        else
        {
            if(size < 0)
            {
                // TODO (seand): Log this error.
            }
            break;
        }
    }
}


//-----------------------------------//
//-----------------------------------//
//------                      -------//
//------  ThreadedConnection  -------//
//------                      -------//
//-----------------------------------//
//-----------------------------------//

ThreadedConnection::ThreadedConnection(QObject *pParent/* = NULL*/)
  : QThread(pParent),
    m_pConnection(NULL)
{
    start();
}

//-----------------------------------//

ThreadedConnection::~ThreadedConnection()
{
    quit();

    // Have to give it some time to delete the connection
    // and finish executing the thread; 500 ms is the MAX
    // amount of time we are willing to wait for it.
    wait(500);
}

//-----------------------------------//

void ThreadedConnection::connectToHost(const QString &host, quint16 port)
{
    emit connectToHostSignal(host, port);
}

//-----------------------------------//

void ThreadedConnection::run()
{
    m_mutex.lock();
      m_pConnection = new Connection;

      // These signals & slots are used to call into the Connection object.
      QObject::connect(this, SIGNAL(connectToHostSignal(QString,quint16)),
                       m_pConnection, SLOT(connectToHost(QString,quint16)));
      QObject::connect(this, SIGNAL(disconnectFromHostSignal()),
                       m_pConnection, SLOT(disconnectFromHost()));
      QObject::connect(this, SIGNAL(sendSignal(QString)),
                       m_pConnection, SLOT(send(QString)));

      // These signals are used to pass on information from the Connection object.
      QObject::connect(m_pConnection, SIGNAL(connecting()), this, SIGNAL(connecting()));
      QObject::connect(m_pConnection, SIGNAL(connected()), this, SIGNAL(connected()));
      QObject::connect(m_pConnection, SIGNAL(connectionFailed()), this, SIGNAL(connectionFailed()));
      QObject::connect(m_pConnection, SIGNAL(disconnected()), this, SIGNAL(disconnected()));
      QObject::connect(m_pConnection, SIGNAL(dataReceived(QString)), this, SIGNAL(dataReceived(QString)));
    m_mutex.unlock();

    exec();

    m_mutex.lock();
      delete m_pConnection;
      m_pConnection = NULL;
    m_mutex.unlock();
}

//-----------------------------------//

void ThreadedConnection::disconnectFromHost()
{
    emit disconnectFromHostSignal();
}

//-----------------------------------//

bool ThreadedConnection::isConnected()
{
    QMutexLocker lock(&m_mutex);
    return (m_pConnection != NULL && m_pConnection->isConnected());
}

//-----------------------------------//

QAbstractSocket::SocketError ThreadedConnection::error()
{
    QMutexLocker lock(&m_mutex);
    if(m_pConnection != NULL)
        return m_pConnection->error();

    return QAbstractSocket::UnknownSocketError;
}

//-----------------------------------//

void ThreadedConnection::send(const QString &data)
{
    emit sendSignal(data);
}

} // End namespace
