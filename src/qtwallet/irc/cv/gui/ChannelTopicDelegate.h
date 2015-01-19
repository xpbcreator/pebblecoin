// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.
//
//
// ChannelTopicDelegate is a specialized class used within ChannelListWindow.

#pragma once

#include <QAbstractItemDelegate>
#include <QFont>

namespace cv { namespace gui {

class ChannelTopicDelegate : public QAbstractItemDelegate
{
    Q_OBJECT

    QFont m_font;

public:
    ChannelTopicDelegate(QObject *parent = NULL);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
                    const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

    void setFont(const QFont &font) { m_font = font; }
};

} } // End namespaces
