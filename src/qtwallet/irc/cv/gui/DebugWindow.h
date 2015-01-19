// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
// DebugWindow is a Window that provides a display for all underlying
// messages sent to and from the IRC server. It displays the raw message
// to aid in debugging.

#pragma once

#include "cv/gui/OutputWindow.h"

namespace cv { namespace gui {

class DebugWindow : public OutputWindow
{
public:
    DebugWindow(Session *pSession,
                const QString &title = tr("Debug Window"),
                const QSize &size = QSize(500, 300));
    ~DebugWindow();

    void giveFocus() { }

    void onSendData(Event *pEvt);
    void onReceivedData(Event *pEvt);

    void onOutput(Event *pEvt);
    void onDoubleClickLink(Event *pEvt);
};

} } // End namespaces
