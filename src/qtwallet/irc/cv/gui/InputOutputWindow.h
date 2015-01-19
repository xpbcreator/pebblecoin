// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// InputOutputWindow is a Window which provides input and output controls,
// and is the base class for StatusWindow, ChannelWindow, and QueryWindow.

#pragma once

#include <QPlainTextEdit>
#include "cv/gui/OutputWindow.h"
#include "cv/gui/ServerConnectionPanel.h"

class QPushButton;

namespace cv {

class Session;

namespace gui {

class InputOutputWindow;

class InputEvent : public Event
{
    InputOutputWindow * m_pWindow;
    Session *           m_pSession;
    QString             m_input;

public:
    InputEvent(InputOutputWindow *pWindow,
               Session *pSession,
               const QString &input)
        : m_pWindow(pWindow),
          m_pSession(pSession),
          m_input(input)
    { }

    InputOutputWindow *getWindow() { return m_pWindow; }
    Session *getSession() { return m_pSession; }
    QString getInput() { return m_input; }
};

class InputOutputWindow : public OutputWindow
{
    Q_OBJECT

protected:
    QPlainTextEdit *    m_pInput;
    QList<QString>      m_pastCommands;

    QExplicitlySharedDataPointer<ServerConnectionPanel>
                        m_pSharedServerConnPanel;
    QPushButton *       m_pOpenButton;

public:
    InputOutputWindow(const QString &title = tr("Untitled"),
                      const QSize &size = QSize(500, 300));
    ~InputOutputWindow();

    void giveFocus();

    // Event callbacks
    void onInput(Event *pEvent);
    void onNoticeMessage(Event *pEvent);

    void onColorConfigChanged(Event *pEvent);

    static void setupColorConfig(QMap<QString, ConfigOption> &defOptions);

protected:
    void setupColors();
    void moveCursorEnd();
    bool eventFilter(QObject *obj, QEvent *event);
    QString getInputText() { return m_pInput->toPlainText(); }

    // Intended to handle the printing/sending of the PRIVMSG message.
    virtual void handleSay(const QString &msg) = 0;

    // Intended to handle the printing/sending of the PRIVMSG ACTION message.
    virtual void handleAction(const QString &msg) = 0;

    virtual void handleTab() = 0;
};

} } // End namespaces
