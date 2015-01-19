// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#pragma once

class QTreeWidgetItem;

namespace cv { namespace gui {

class Window;

// Window types.
enum
{
    TYPE_WINDOW,
    TYPE_IRCWINDOW,
    TYPE_IRCCHANWINDOW,
    TYPE_IRCPRIVWINDOW
};


// For management by the WindowManager class.
struct win_info_t
{
    QTreeWidgetItem *   m_pTreeItem;
    Window *            m_pWindow;
};

} } // End namespaces
