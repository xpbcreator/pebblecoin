// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// ServerConnectionPanel is an OverlayPanel which provides a user interface
// for connecting to an IRC server. It is shared between all OutputWindows
// for a particular server (StatusWindow, DebugWindow, and any ChannelWindows
// and QueryWindows); when it is open on one Window, the others display an
// open button.

#pragma once

#include "cv/ConfigManager.h"
#include "cv/gui/OverlayPanel.h"

class QLabel;
class QLineEdit;

namespace cv { namespace gui {

class ServerConnectionPanel : public OverlayPanel, public QSharedData
{
    Q_OBJECT

    // All the form widgets used to connect to the server.
    QPushButton *       m_pConnectButton;
    QPushButton *       m_pCloseButton;

    QLabel *            m_pServerLbl;
    QLineEdit *         m_pServerInput;
    QLabel *            m_pPortLbl;
    QLineEdit *         m_pPortInput;

    QLabel *            m_pNameLbl;
    QLineEdit *         m_pNameInput;

    QLabel *            m_pNickLbl;
    QLineEdit *         m_pNickInput;
    QLabel *            m_pAltNickLbl;
    QLineEdit *         m_pAltNickInput;

public:
    ServerConnectionPanel(QWidget *parent);

    void validateAndConnect();

    static void setupServerConfig(QMap<QString, ConfigOption> &defOptions, const QString &defaultNick);

signals:
    void connect(QString server, int port, QString name, QString nick, QString altNick);

public slots:
    void onCloseClicked();
    void onEnter();

protected:
    void createForm();
};

} } // End namespaces
