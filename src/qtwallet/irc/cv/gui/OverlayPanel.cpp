// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include <QPushButton>
#include <QPainter>
#include <QStyleOption>
#include <QSignalMapper>
#include <QStringBuilder>
#include <QGraphicsDropShadowEffect>
#include "cv/gui/OverlayPanel.h"

namespace cv { namespace gui {

OverlayPanel::OverlayPanel(QWidget *parent)
    : QWidget(parent),

      m_isInitialized(false),
      m_state(OPEN),

      m_alignment(Qt::AlignTop),
      m_secondaryAlignment(0),
      m_firstOffset(-1),
      m_secondOffset(-1),

      m_duration(100),

      m_dropShadowWidth(0),

      m_pCurrOpenButton(NULL),
      m_buttonAlignment(Qt::AlignLeft),
      m_buttonOffset(0),
      m_pOpenBtnAnimation(NULL)
{
    m_pSlideOpenAnimation = new QPropertyAnimation(this, "geometry");
    QObject::connect(m_pSlideOpenAnimation, SIGNAL(finished()), this, SIGNAL(panelOpened()));
    m_pSlideClosedAnimation = new QPropertyAnimation(this, "geometry");
    QObject::connect(m_pSlideClosedAnimation, SIGNAL(finished()), this, SIGNAL(panelClosed()));

    m_pSignalMapper = new QSignalMapper(this);

    QObject::connect(m_pSlideClosedAnimation, SIGNAL(finished()), this, SLOT(onPanelClosed()));
}

//-----------------------------------//

// Sets the duration for the OverlayPanel.
void OverlayPanel::setDuration(int duration)
{
    if(!m_isInitialized)
        m_duration = duration;
}

//-----------------------------------//

// Sets the drop shadow width and alignment.
void OverlayPanel::setDropShadowConfig(int dropShadowWidth, Qt::Alignment dropShadowSides/* = (Qt::AlignBottom | Qt::AlignRight)*/)
{
    if(!m_isInitialized)
    {
        m_dropShadowWidth = dropShadowWidth;
        m_dropShadowSides = dropShadowSides;
    }
}

//-----------------------------------//

// Sets the button alignment and offset.
void OverlayPanel::setButtonConfig(Qt::AlignmentFlag btnAlignment, int btnOffset)
{
    if(!m_isInitialized)
    {
        m_buttonAlignment = btnAlignment;
        m_buttonOffset = btnOffset;
    }
}

//-----------------------------------//

// Adds an open button for the OverlayPanel.
QPushButton *OverlayPanel::addOpenButton(QWidget *pParent, const QString &btnText, int w, int h)
{
    if(m_isInitialized)
    {
        QPushButton *pOpenButton = new QPushButton(pParent);
        pOpenButton->setText(btnText);
        pOpenButton->resize(w, h);

        QString
            btnCss = QString("QPushButton { ") %
                     QString("color: rgba(153, 153, 153, 50%); ") %
                     QString("background-color: transparent; ") %
                     QString("border-width: 1px; ") %
                     QString("border-style: solid; ") %
                     QString("border-color: rgba(153, 153, 153, 50%); ");
        if(m_alignment == Qt::AlignTop)
        {
            btnCss += QString("border-bottom-left-radius: 5px; ") %
                      QString("border-bottom-right-radius: 5px; ") %
                      QString("border-top-width: 0px; } ");
        }
        else if(m_alignment == Qt::AlignBottom)
        {
            btnCss += QString("border-top-left-radius: 5px; ") %
                      QString("border-top-right-radius: 5px; ") %
                      QString("border-bottom-width: 0px; } ");
        }
        else if(m_alignment == Qt::AlignLeft)
        {
            btnCss += QString("border-top-right-radius: 5px; ") %
                      QString("border-bottom-right-radius: 5px; ") %
                      QString("border-left-width: 0px; } ");
        }
        else
        {
            btnCss += QString("border-top-left-radius: 5px; ") %
                      QString("border-bottom-left-radius: 5px; ") %
                      QString("border-right-width: 0px; } ");
        }
            btnCss += QString("QPushButton:hover { ") %
                      QString("color: palette(button-text); ") %
                      QString("border-color: palette(dark); ") %
                      QString("background-color: palette(button); }");
        pOpenButton->setStyleSheet(btnCss);

        QObject::connect(pOpenButton, SIGNAL(clicked()), m_pSignalMapper, SLOT(map()));
        m_pSignalMapper->setMapping(pOpenButton, pOpenButton);
        QObject::connect(m_pSignalMapper, SIGNAL(mapped(QWidget*)), this, SLOT(onOpenClicked(QWidget*)));

        // If this open button shares the same parent as the panel,
        // then create the animation for it.
        if(pParent == parentWidget())
        {
            m_pCurrOpenButton = pOpenButton;
            realignPanel(m_pCurrOpenButton);

            int btnShownX, btnShownY, btnHiddenX, btnHiddenY;
            getCurrButtonShownPosition(btnShownX, btnShownY);
            getCurrButtonHiddenPosition(btnHiddenX, btnHiddenY);

            m_pOpenBtnAnimation = new QPropertyAnimation(m_pCurrOpenButton, "geometry");
            m_pOpenBtnAnimation->setStartValue(QRect(btnHiddenX, btnHiddenY,
                                                     m_pCurrOpenButton->width(),
                                                     m_pCurrOpenButton->height()));
            m_pOpenBtnAnimation->setEndValue(QRect(btnShownX, btnShownY,
                                                   m_pCurrOpenButton->width(),
                                                   m_pCurrOpenButton->height()));
        }

        return pOpenButton;
    }

    return NULL;
}

//-----------------------------------//

// Sets the alignment for the OverlayPanel.
void OverlayPanel::setAlignment(Qt::AlignmentFlag alignment)
{
    if(!m_isInitialized)
        m_alignment = alignment;
}

//-----------------------------------//

// Sets the secondary alignment for the OverlayPanel.
void OverlayPanel::setSecondaryAlignment(Qt::Alignment alignment, int firstOffset, int secondOffset)
{
    if(!m_isInitialized)
    {
        m_secondaryAlignment = alignment;
        m_firstOffset = firstOffset;
        m_secondOffset = secondOffset;
    }
}

//-----------------------------------//

// Sets the initial state of the OverlayPanel.
void OverlayPanel::setInitialState(OverlayState state)
{
    if(!m_isInitialized)
        m_state = state;
}

//-----------------------------------//

// Initializes everything so that the OverlayPanel can be used; assumes
// that the panel's current x and y are for the OPEN state.
void OverlayPanel::initialize()
{
    m_isInitialized = true;

    if(m_state == CLOSED)
    {
        int closedX, closedY;
        if(m_alignment == Qt::AlignTop || m_alignment == Qt::AlignBottom)
        {
            closedX = x();
            closedY = (m_alignment == Qt::AlignTop) ? -height() : parentWidget()->height();
        }
        else
        {
            closedY = y();
            closedX = (m_alignment == Qt::AlignLeft) ? -width() : parentWidget()->width();
        }
        move(closedX, closedY);
    }
}

//-----------------------------------//

void OverlayPanel::open(bool animate/* = true*/)
{
    if(m_state != OPEN && m_isInitialized)
    {
        // Hide the open button, if it exists.
        if(m_pCurrOpenButton != NULL)
        {
            int btnClosedX, btnClosedY;
            getCurrButtonHiddenPosition(btnClosedX, btnClosedY);
            m_pCurrOpenButton->move(btnClosedX, btnClosedY);
        }

        int openX, openY;
        if(m_alignment == Qt::AlignTop || m_alignment == Qt::AlignBottom)
        {
            openX = x();
            openY = (m_alignment == Qt::AlignTop) ? 0 : (parentWidget()->height() - height());
        }
        else
        {
            openY = y();
            openX = (m_alignment == Qt::AlignLeft) ? 0 : (parentWidget()->width() - width());
        }

        if(animate)
        {
            QRect openRect(openX, openY, width(), height());
            QRect closedRect(x(), y(), width(), height());

            m_pSlideOpenAnimation->setDuration(m_duration);
            m_pSlideOpenAnimation->setStartValue(closedRect);
            m_pSlideOpenAnimation->setEndValue(openRect);

            m_pSlideOpenAnimation->start();
        }
        else
        {
            move(openX, openY);
            emit panelOpened();
        }

        m_state = OPEN;
    }
}

//-----------------------------------//

void OverlayPanel::close(bool animate/* = true*/)
{
    if(m_state != CLOSED && m_isInitialized)
    {
        int closedX, closedY;
        if(m_alignment == Qt::AlignTop || m_alignment == Qt::AlignBottom)
        {
            closedX = x();
            closedY = (m_alignment == Qt::AlignTop) ? -height() : parentWidget()->height();
        }
        else
        {
            closedY = y();
            closedX = (m_alignment == Qt::AlignLeft) ? -width() : parentWidget()->width();
        }

        if(animate)
        {
            QRect openRect(x(), y(), width(), height());
            QRect closedRect(closedX, closedY, width(), height());

            m_pSlideClosedAnimation->setDuration(m_duration);
            m_pSlideClosedAnimation->setStartValue(openRect);
            m_pSlideClosedAnimation->setEndValue(closedRect);

            // Reset the animation for the open button.
            if(m_pCurrOpenButton != NULL)
            {
                int btnShownX, btnShownY, btnHiddenX, btnHiddenY;
                getCurrButtonShownPosition(btnShownX, btnShownY);
                getCurrButtonHiddenPosition(btnHiddenX, btnHiddenY);
                m_pOpenBtnAnimation->setTargetObject(m_pCurrOpenButton);
                m_pOpenBtnAnimation->setDuration(m_duration);
                m_pOpenBtnAnimation->setStartValue(QRect(btnHiddenX, btnHiddenY,
                                                         m_pCurrOpenButton->width(),
                                                         m_pCurrOpenButton->height()));
                m_pOpenBtnAnimation->setEndValue(QRect(btnShownX, btnShownY,
                                                       m_pCurrOpenButton->width(),
                                                       m_pCurrOpenButton->height()));
            }

            m_pSlideClosedAnimation->start();
        }
        else
        {
            move(closedX, closedY);
            if(m_pCurrOpenButton != NULL)
            {
                int btnOpenX, btnOpenY;
                getCurrButtonShownPosition(btnOpenX, btnOpenY);
                m_pCurrOpenButton->move(btnOpenX, btnOpenY);
            }
        }
        m_state = CLOSED;
    }
}

//-----------------------------------//

void OverlayPanel::toggle(bool animate/* = true*/)
{
    if(m_state == OPEN)
        close(animate);
    else
        open(animate);
}

//-----------------------------------//

void OverlayPanel::onOpenClicked(QWidget *pButton)
{
    // If the button clicked is not inside the same widget
    // as the panel...
    if(parent() != pButton->parentWidget())
    {
        // Make sure the panel is closed (so the button will be shown).
        close(false);

        // Reparent the panel to this button's parent.
        setParent(pButton->parentWidget());
        show();

        // Set the current open button.
        m_pCurrOpenButton = dynamic_cast<QPushButton *>(pButton);
        realignPanel(m_pCurrOpenButton);
    }

    // Open the panel.
    open();
}

//-----------------------------------//

void OverlayPanel::onPanelClosed()
{
    if(m_pOpenBtnAnimation != NULL)
        m_pOpenBtnAnimation->start();
}

//-----------------------------------//

// Realigns the OverlayPanel to the parent widget.
void OverlayPanel::realignPanel(QPushButton *pOpenButton/* = NULL*/)
{
    // Update the position for the panel (and, if applicable, the open button)
    // according to the alignment rules.
    int panelX = x(), panelY = y(), panelW = width(), panelH = height();
    if(m_alignment == Qt::AlignTop || m_alignment == Qt::AlignBottom)
    {
        if(m_state == OPEN)
            panelY = (m_alignment == Qt::AlignTop) ? 0 : (pOpenButton->parentWidget()->height() - height());
        else
            panelY = (m_alignment == Qt::AlignTop) ? -height() : pOpenButton->parentWidget()->height();

        if(m_secondaryAlignment & Qt::AlignLeft)
        {
            // Set the x coord of the panel.
            panelX = m_firstOffset;

            // Also set the width of the panel.
            if(m_secondaryAlignment & Qt::AlignRight)
                panelW = pOpenButton->parentWidget()->width() - (m_firstOffset + m_secondOffset);
        }
        else if(m_secondaryAlignment & Qt::AlignRight)
        {
            // Set the x coord of the panel.
            panelX = pOpenButton->parentWidget()->width() - (width() + m_firstOffset);
        }
    }
    else // aligned to the left or right
    {
        if(m_state == OPEN)
            panelX = (m_alignment == Qt::AlignLeft) ? 0 : (pOpenButton->parentWidget()->width() - width());
        else
            panelX = (m_alignment == Qt::AlignLeft) ? -width() : pOpenButton->parentWidget()->width();

        if(m_secondaryAlignment & Qt::AlignTop)
        {
            // Set the y coord of the panel.
            panelY = m_firstOffset;

            // Also set the height of the panel.
            if(m_secondaryAlignment & Qt::AlignBottom)
                panelH = pOpenButton->parentWidget()->height() - (m_firstOffset + m_secondOffset);
        }
        else if(m_secondaryAlignment & Qt::AlignBottom)
        {
            // Set the y coord of the panel.
            panelY = pOpenButton->parentWidget()->height() - (height() + m_firstOffset);
        }
    }

    if(pOpenButton != NULL)
    {
        // Move and resize the panel if it shares the same parent as the button.
        if(m_pCurrOpenButton == pOpenButton)
        {
            move(panelX, panelY);
            resize(panelW, panelH);
        }

        realignButton(pOpenButton, panelX, panelY);
    }
}

//-----------------------------------//

// Finds the offsets for the button, and realigns it.
void OverlayPanel::realignButton(QPushButton *pOpenButton, int x, int y)
{
    if(pOpenButton != NULL)
    {
        int btnX, btnY;
        if(m_alignment == Qt::AlignTop || m_alignment == Qt::AlignBottom)
        {
            if(m_buttonAlignment == Qt::AlignLeft)
                btnX = x + m_buttonOffset;
            else
                btnX = x + width() - (pOpenButton->width() + m_buttonOffset);

            if(m_state == OPEN && m_pCurrOpenButton == pOpenButton)
                btnY = (m_alignment == Qt::AlignTop) ? -pOpenButton->height() : pOpenButton->parentWidget()->height();
            else
                btnY = (m_alignment == Qt::AlignTop) ? 0 : (pOpenButton->parentWidget()->height() - pOpenButton->height());
        }
        else
        {
            if(m_buttonAlignment == Qt::AlignTop)
                btnY = y + m_buttonOffset;
            else
                btnY = y + height() - (pOpenButton->height() + m_buttonOffset);

            if(m_state == OPEN && m_pCurrOpenButton == pOpenButton)
                btnX = (m_alignment == Qt::AlignLeft) ? -pOpenButton->width() : pOpenButton->parentWidget()->width();
            else
                btnX = (m_alignment == Qt::AlignLeft) ? 0 : (pOpenButton->parentWidget()->width() - pOpenButton->width());
        }

        pOpenButton->move(btnX, btnY);
    }
}

//-----------------------------------//

// Retrieves the button position for when it's open.
void OverlayPanel::getCurrButtonShownPosition(int &x, int &y)
{
    x = m_pCurrOpenButton->x();
    y = m_pCurrOpenButton->y();

    if(m_state == OPEN)
    {
        if(m_alignment == Qt::AlignTop)
            y += m_pCurrOpenButton->height();
        else if(m_alignment == Qt::AlignBottom)
            y -= m_pCurrOpenButton->height();
        else if(m_alignment == Qt::AlignLeft)
            x += m_pCurrOpenButton->width();
        else
            x -= m_pCurrOpenButton->width();
    }
}

//-----------------------------------//

// Retrieves button position for when it's closed.
void OverlayPanel::getCurrButtonHiddenPosition(int &x, int &y)
{
    x = m_pCurrOpenButton->x();
    y = m_pCurrOpenButton->y();

    if(m_state == CLOSED)
    {
        if(m_alignment == Qt::AlignTop)
            y = -m_pCurrOpenButton->height();
        else if(m_alignment == Qt::AlignBottom)
            y = parentWidget()->height();
        else if(m_alignment == Qt::AlignLeft)
            x = -m_pCurrOpenButton->width();
        else
            x = parentWidget()->width();
    }
}

//-----------------------------------//

void OverlayPanel::paintEvent(QPaintEvent *)
{
    const int   SHADOW_RADIUS = 20,
                HALF_SHADOW_RADIUS = SHADOW_RADIUS / 2,
                INITIAL_OPACITY = 150,
                OPACITY_DIFF = INITIAL_OPACITY / m_dropShadowWidth,
                TL_ARC_START = 180 * 16,
                TL_ARC_RANGE = -90 * 16,
                TR_ARC_START = 90 * 16,
                TR_ARC_RANGE = -90 * 16,
                BL_ARC_START = 180 * 16,
                BL_ARC_RANGE = 90 * 16,
                BR_ARC_START = 0,
                BR_ARC_RANGE = -90 * 16;

    bool topAligned = m_alignment == Qt::AlignTop,
         leftAligned = m_alignment == Qt::AlignLeft,
         rightAligned = m_alignment == Qt::AlignRight,
         bottomAligned = m_alignment == Qt::AlignBottom;

    bool shadowOnTop = m_dropShadowSides & Qt::AlignTop,
         shadowOnLeft = m_dropShadowSides & Qt::AlignLeft,
         shadowOnRight = m_dropShadowSides & Qt::AlignRight,
         shadowOnBottom = m_dropShadowSides & Qt::AlignBottom;

    // Draw the drop shadow first (if there is one).
    if(m_dropShadowWidth > 0)
    {
        QPainter painter(this);
        QPen pen(QColor(90, 90, 90, INITIAL_OPACITY));
        pen.setWidth(1);
        painter.setPen(pen);
        painter.setRenderHint(QPainter::Antialiasing);

        // Initialize the variables which will be used to draw the lines.
        int tbLeftX = -m_dropShadowWidth;
        if(!leftAligned)
        {
            tbLeftX = HALF_SHADOW_RADIUS;
            if(!shadowOnLeft)
                tbLeftX += m_dropShadowWidth;
        }

        int tbRightX = width();
        if(rightAligned)
        {
            tbRightX += m_dropShadowWidth;
        }
        else
        {
            tbRightX -= HALF_SHADOW_RADIUS;
            if(!shadowOnRight)
                tbRightX -= m_dropShadowWidth;
        }

        int lrTopY = -m_dropShadowWidth;
        if(!topAligned)
        {
            lrTopY = HALF_SHADOW_RADIUS;
            if(!shadowOnTop)
                lrTopY += m_dropShadowWidth;
        }

        int lrBottomY = height();
        if(bottomAligned)
        {
            lrBottomY += m_dropShadowWidth;
        }
        else
        {
            lrBottomY -= HALF_SHADOW_RADIUS;
            if(!shadowOnBottom)
                lrBottomY -= m_dropShadowWidth;
        }

        for(int i = m_dropShadowWidth; i > 0; --i)
        {
            QColor newColor = painter.pen().color();
            newColor.setAlpha(newColor.alpha() - OPACITY_DIFF);
            QPen newPen(newColor);
            painter.setPen(newPen);

            // These variables are dependent on the loop iteration,
            // but are built off the 4 variables defined above.
            int rightEdge = width() - i,
                bottomEdge = height() - i,
                tbLineLeftX = tbLeftX + i,
                tbLineRightX = tbRightX - i,
                tbArcLeftX = tbLineLeftX - HALF_SHADOW_RADIUS,
                tbArcRightX = tbLineRightX - HALF_SHADOW_RADIUS,
                lrLineTopY = lrTopY + i,
                lrLineBottomY = lrBottomY - i,
                lrArcTopY = lrLineTopY - HALF_SHADOW_RADIUS,
                lrArcBottomY = lrLineBottomY - HALF_SHADOW_RADIUS;

            // Draw the top line.
            if(shadowOnTop)
            {
                painter.drawLine(tbLineLeftX + 0, i, tbLineRightX - 0, i);
                painter.drawLine(tbLineLeftX + 1, i, tbLineRightX - 1, i);
                painter.drawLine(tbLineLeftX + 2, i, tbLineRightX - 2, i);
            }

            // Draw the top-left arc.
            if((shadowOnLeft || shadowOnTop) && !leftAligned && !topAligned)
            {
                painter.drawArc(tbArcLeftX,     lrArcTopY,     SHADOW_RADIUS, SHADOW_RADIUS, TL_ARC_START, TL_ARC_RANGE);
                painter.drawArc(tbArcLeftX,     lrArcTopY + 1, SHADOW_RADIUS, SHADOW_RADIUS, TL_ARC_START, TL_ARC_RANGE);
                painter.drawArc(tbArcLeftX + 1, lrArcTopY,     SHADOW_RADIUS, SHADOW_RADIUS, TL_ARC_START, TL_ARC_RANGE);
            }

            // Draw the left line.
            if(shadowOnLeft)
            {
                painter.drawLine(i, lrLineTopY + 0, i, lrLineBottomY - 0);
                painter.drawLine(i, lrLineTopY + 1, i, lrLineBottomY - 1);
                painter.drawLine(i, lrLineTopY + 2, i, lrLineBottomY - 2);
            }

            // Draw the top-right arc.
            if((shadowOnTop || shadowOnRight) && !topAligned && !rightAligned)
            {
                painter.drawArc(tbArcRightX,     lrArcTopY,     SHADOW_RADIUS, SHADOW_RADIUS, TR_ARC_START, TR_ARC_RANGE);
                painter.drawArc(tbArcRightX,     lrArcTopY + 1, SHADOW_RADIUS, SHADOW_RADIUS, TR_ARC_START, TR_ARC_RANGE);
                painter.drawArc(tbArcRightX - 1, lrArcTopY,     SHADOW_RADIUS, SHADOW_RADIUS, TR_ARC_START, TR_ARC_RANGE);
            }

            // Draw the right line.
            if(shadowOnRight)
            {
                painter.drawLine(rightEdge, lrLineTopY + 0, rightEdge, lrLineBottomY - 0);
                painter.drawLine(rightEdge, lrLineTopY + 1, rightEdge, lrLineBottomY - 1);
                painter.drawLine(rightEdge, lrLineTopY + 2, rightEdge, lrLineBottomY - 2);
            }

            // Draw the bottom-left arc.
            if((shadowOnLeft || shadowOnBottom) && !leftAligned && !bottomAligned)
            {
                painter.drawArc(tbArcLeftX,     lrArcBottomY,     SHADOW_RADIUS, SHADOW_RADIUS, BL_ARC_START, BL_ARC_RANGE);
                painter.drawArc(tbArcLeftX,     lrArcBottomY - 1, SHADOW_RADIUS, SHADOW_RADIUS, BL_ARC_START, BL_ARC_RANGE);
                painter.drawArc(tbArcLeftX + 1, lrArcBottomY,     SHADOW_RADIUS, SHADOW_RADIUS, BL_ARC_START, BL_ARC_RANGE);
            }

            // Draw the bottom line.
            if(shadowOnBottom)
            {
                painter.drawLine(tbLineLeftX + 0, bottomEdge, tbLineRightX - 0, bottomEdge);
                painter.drawLine(tbLineLeftX + 1, bottomEdge, tbLineRightX - 1, bottomEdge);
                painter.drawLine(tbLineLeftX + 2, bottomEdge, tbLineRightX - 2, bottomEdge);
            }

            // Draw the bottom-right arc.
            if((shadowOnRight || shadowOnBottom) && !rightAligned && !bottomAligned)
            {
                painter.drawArc(tbArcRightX,     lrArcBottomY,     SHADOW_RADIUS, SHADOW_RADIUS, BR_ARC_START, BR_ARC_RANGE);
                painter.drawArc(tbArcRightX,     lrArcBottomY - 1, SHADOW_RADIUS, SHADOW_RADIUS, BR_ARC_START, BR_ARC_RANGE);
                painter.drawArc(tbArcRightX - 1, lrArcBottomY,     SHADOW_RADIUS, SHADOW_RADIUS, BR_ARC_START, BR_ARC_RANGE);
            }
        }
    }

    // Draw the widget; this code is needed to correctly draw it with the
    // stylesheet rules applied.
    QStyleOption opt;
    opt.init(this);
    if(m_dropShadowWidth > 0)
    {
        opt.rect.adjust(
            shadowOnLeft ? m_dropShadowWidth : 0,
            shadowOnTop ? m_dropShadowWidth : 0,
            shadowOnRight ? -m_dropShadowWidth : 0,
            shadowOnBottom ? -m_dropShadowWidth : 0);
    }
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

} } // End namespaces
