// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QStringBuilder>
#include "cv/ConfigManager.h"
#include "cv/gui/ServerConnectionPanel.h"

#include "irc_config.h"

namespace cv { namespace gui {

ServerConnectionPanel::ServerConnectionPanel(QWidget *parent)
    : OverlayPanel(parent)
{
    setFont(QFont("Arial", 10));
    setAutoFillBackground(true);

    setDuration(150);
    setAlignment(Qt::AlignTop);
    setSecondaryAlignment(Qt::AlignRight, 100, -1);
    setButtonConfig(Qt::AlignTop, 15);
    setDropShadowConfig(5, Qt::AlignRight | Qt::AlignBottom);
    setInitialState(OPEN);
    resize(360, 215);
    initialize();
    QObject::connect(this, SIGNAL(panelOpened()), this, SLOT(setFocus()));

    createForm();
    setFocusProxy(m_pServerInput);
}

//-----------------------------------//

// If all fields are valid, then this conects to the server.
// Otherwise, it sets the focus to the first invalid field.
void ServerConnectionPanel::validateAndConnect()
{
    // reset all the stylesheets
    QString invalidCss = "border: 1px solid red;";
    m_pServerInput->setStyleSheet("");
    m_pPortInput->setStyleSheet("");
    m_pNameInput->setStyleSheet("");
    m_pNickInput->setStyleSheet("");

    if(m_pServerInput->text().isEmpty())
    {
        m_pServerInput->setStyleSheet(invalidCss);
        m_pServerInput->setFocus();
        return;
    }

    bool portIsValid;
    int port = m_pPortInput->text().toInt(&portIsValid);
    if(m_pPortInput->text().isEmpty() || !portIsValid)
    {
        m_pPortInput->setStyleSheet(invalidCss);
        m_pPortInput->setFocus();
        m_pPortInput->selectAll();
        return;
    }

    if(m_pNameInput->text().isEmpty())
    {
        m_pNameInput->setStyleSheet(invalidCss);
        m_pNameInput->setFocus();
        return;
    }

    if(m_pNickInput->text().isEmpty())
    {
        m_pNickInput->setStyleSheet(invalidCss);
        m_pNickInput->setFocus();
        return;
    }

    // Emit the signal so that the window can connect to the server.
    emit connect(m_pServerInput->text(), port, m_pNameInput->text(), m_pNickInput->text(), m_pAltNickInput->text());

    // Save the name, nick, and alternate nick in the config.
    g_pCfgManager->setOptionValue("server.host", m_pServerInput->text(), false);
    g_pCfgManager->setOptionValue("server.port", m_pPortInput->text(), false);
    g_pCfgManager->setOptionValue("server.channel", m_pNameInput->text(), false);
    g_pCfgManager->setOptionValue("server.nick", m_pNickInput->text(), false);
    g_pCfgManager->setOptionValue("server.altNick", m_pAltNickInput->text(), false);
    g_pCfgManager->writeToDefaultFile();    
}

//-----------------------------------//

void ServerConnectionPanel::setupServerConfig(QMap<QString, ConfigOption> &defOptions, const QString &defaultNick)
{
    defOptions.insert("server.host", ConfigOption(IRC_SERVER_HOST));
    defOptions.insert("server.port", ConfigOption(IRC_SERVER_PORT));
    defOptions.insert("server.channel", ConfigOption(IRC_SERVER_CHANNEL));
    defOptions.insert("server.nick", ConfigOption(defaultNick));
    defOptions.insert("server.altNick", ConfigOption(""));
}

//-----------------------------------//

void ServerConnectionPanel::onCloseClicked()
{
    close();
}

//-----------------------------------//

void ServerConnectionPanel::onEnter()
{
    validateAndConnect();
}

//-----------------------------------//

void ServerConnectionPanel::createForm()
{
    int LABEL_START = 20,
        LABEL_WIDTH = 110,
        BUTTON_WIDTH = 70,
        BUTTON_HEIGHT = 30,
        CONTROL_HEIGHT = 22,
        SPACING = 5;

    int rowY = 20;
    m_pServerLbl = new QLabel(this);
    m_pServerLbl->setText("Server:");
    m_pServerLbl->move(LABEL_START, rowY);
    m_pServerLbl->resize(LABEL_WIDTH, CONTROL_HEIGHT);
    m_pServerLbl->setAlignment(Qt::AlignRight | Qt::AlignCenter);
    m_pServerLbl->setFont(font());
    m_pServerInput = new QLineEdit(this);
    m_pServerInput->move(LABEL_START + LABEL_WIDTH + SPACING, rowY);
    m_pServerInput->resize(200, CONTROL_HEIGHT);
    m_pServerInput->setText(GET_STRING("server.host"));
    m_pServerInput->setFont(font());
    QObject::connect(m_pServerInput, SIGNAL(returnPressed()), this, SLOT(onEnter()));

    rowY += CONTROL_HEIGHT + 5;
    m_pPortLbl = new QLabel(this);
    m_pPortLbl->setText("Port:");
    m_pPortLbl->move(LABEL_START, rowY);
    m_pPortLbl->resize(LABEL_WIDTH, CONTROL_HEIGHT);
    m_pPortLbl->setAlignment(Qt::AlignRight | Qt::AlignCenter);
    m_pPortLbl->setFont(font());
    m_pPortInput = new QLineEdit(this);
    m_pPortInput->move(LABEL_START + LABEL_WIDTH + SPACING, rowY);
    m_pPortInput->resize(50, CONTROL_HEIGHT);
    m_pPortInput->setText(GET_STRING("server.port"));
    m_pPortInput->setFont(font());
    QObject::connect(m_pPortInput, SIGNAL(returnPressed()), this, SLOT(onEnter()));

    rowY += CONTROL_HEIGHT + 5;
    m_pNameLbl = new QLabel(this);
    m_pNameLbl->setText("Channel:");
    m_pNameLbl->move(LABEL_START, rowY);
    m_pNameLbl->resize(LABEL_WIDTH, CONTROL_HEIGHT);
    m_pNameLbl->setAlignment(Qt::AlignRight | Qt::AlignCenter);
    m_pNameLbl->setFont(font());
    m_pNameInput = new QLineEdit(this);
    m_pNameInput->move(LABEL_START + LABEL_WIDTH + SPACING, rowY);
    m_pNameInput->resize(150, CONTROL_HEIGHT);
    m_pNameInput->setFont(font());
    m_pNameInput->setText(GET_STRING("server.channel"));
    QObject::connect(m_pNameInput, SIGNAL(returnPressed()), this, SLOT(onEnter()));

    rowY += CONTROL_HEIGHT + 5;
    m_pNickLbl = new QLabel(this);
    m_pNickLbl->setText("Nickname:");
    m_pNickLbl->move(LABEL_START, rowY);
    m_pNickLbl->resize(LABEL_WIDTH, CONTROL_HEIGHT);
    m_pNickLbl->setAlignment(Qt::AlignRight | Qt::AlignCenter);
    m_pNickLbl->setFont(font());
    m_pNickInput = new QLineEdit(this);
    m_pNickInput->move(LABEL_START + LABEL_WIDTH + SPACING, rowY);
    m_pNickInput->resize(150, CONTROL_HEIGHT);
    m_pNickInput->setFont(font());
    m_pNickInput->setText(GET_STRING("server.nick"));
    QObject::connect(m_pNickInput, SIGNAL(returnPressed()), this, SLOT(onEnter()));

    rowY += CONTROL_HEIGHT + 5;
    m_pAltNickLbl = new QLabel(this);
    m_pAltNickLbl->setText("Alternate nick:");
    m_pAltNickLbl->move(LABEL_START, rowY);
    m_pAltNickLbl->resize(LABEL_WIDTH, CONTROL_HEIGHT);
    m_pAltNickLbl->setAlignment(Qt::AlignRight | Qt::AlignCenter);
    m_pAltNickLbl->setFont(font());
    m_pAltNickInput = new QLineEdit(this);
    m_pAltNickInput->move(LABEL_START + LABEL_WIDTH + SPACING, rowY);
    m_pAltNickInput->resize(150, CONTROL_HEIGHT);
    m_pAltNickInput->setFont(font());
    m_pAltNickInput->setText(GET_STRING("server.altNick"));
    QObject::connect(m_pAltNickInput, SIGNAL(returnPressed()), this, SLOT(onEnter()));

    m_pConnectButton = new QPushButton(this);
    m_pConnectButton->setText("Connect");
    m_pConnectButton->resize(BUTTON_WIDTH, BUTTON_HEIGHT);
    m_pConnectButton->move(width() - (2 * BUTTON_WIDTH) - 25, height() - BUTTON_HEIGHT - 15);
    m_pConnectButton->setFont(font());
    QObject::connect(m_pConnectButton, SIGNAL(clicked()), this, SLOT(onEnter()));

    m_pCloseButton = new QPushButton(this);
    m_pCloseButton->setText("Close");
    m_pCloseButton->resize(BUTTON_WIDTH, BUTTON_HEIGHT);
    m_pCloseButton->move(width() - BUTTON_WIDTH - 15, height() - BUTTON_HEIGHT - 15);
    m_pCloseButton->setFont(font());
    QObject::connect(m_pCloseButton, SIGNAL(clicked()), this, SLOT(onCloseClicked()));
}

} } // End namespaces

