// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "InstanceDelegate.h"
#include <QApplication>
#include <QCache>
#include <QDebug>
#include <QIcon>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QTextEdit>
#include <QTextLayout>
#include <QTextOption>
#include <QtMath>

#include "BaseInstance.h"
#include "FileSystem.h"
#include "InstanceList.h"
#include "InstanceView.h"

// Origin: Qt
static void viewItemTextLayout(QTextLayout& textLayout, int lineWidth, qreal& height, qreal& widthUsed)
{
    height = 0;
    widthUsed = 0;
    textLayout.beginLayout();
    QString str = textLayout.text();
    while (true) {
        QTextLine line = textLayout.createLine();
        if (!line.isValid())
            break;
        if (line.textLength() == 0)
            break;
        line.setLineWidth(lineWidth);
        line.setPosition(QPointF(0, height));
        height += line.height();
        widthUsed = qMax(widthUsed, line.naturalTextWidth());
    }
    textLayout.endLayout();
}

static QPixmap getCachedBanner(BaseInstance* instance)
{
    if (!instance) return QPixmap();

    static QCache<QString, QPixmap> s_bannerCache(30);

    QString id = instance->id();
    if (s_bannerCache.contains(id)) {
        return *s_bannerCache.object(id);
    }

    QStringList candidates = {
        FS::PathCombine(instance->instanceRoot(), "banner.png"),
        FS::PathCombine(instance->instanceRoot(), "banner.jpg"),
        FS::PathCombine(instance->instanceRoot(), "banner.webp"),
        FS::PathCombine(instance->gameRoot(), "banner.png"),
        FS::PathCombine(instance->gameRoot(), "banner.jpg")
    };

    for (const auto& path : candidates) {
        if (QFile::exists(path)) {
            QPixmap pix(path);
            if (!pix.isNull()) {
                s_bannerCache.insert(id, new QPixmap(pix));
                return pix;
            }
        }
    }

    // Cache null pixmap to avoid repeated filesystem checks
    s_bannerCache.insert(id, new QPixmap());
    return QPixmap();
}

ListViewDelegate::ListViewDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void drawSelectionRect(QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    QRect cardRect = rect.adjusted(2, 2, -2, -2);
    if ((option.state & QStyle::State_Selected)) {
        // Expressive, vibrant translucent accent covering the whole tile card
        QColor highlightBg = option.palette.color(QPalette::Highlight);
        highlightBg.setAlpha(100); // ~40% translucent accent fill
        painter->setBrush(highlightBg);

        QColor borderCol = option.palette.color(QPalette::Highlight).lighter(130);
        borderCol.setAlpha(230);
        painter->setPen(QPen(borderCol, 1.5));
        painter->drawRoundedRect(cardRect, 6.0, 6.0);
    } else if ((option.state & QStyle::State_MouseOver)) {
        // Subtle translucent hover card
        QColor hoverBg = option.palette.color(QPalette::Highlight);
        hoverBg.setAlpha(35);
        painter->setBrush(hoverBg);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(cardRect, 6.0, 6.0);
    }
    painter->restore();
}

void drawFocusRect(QPainter* painter, const QStyleOptionViewItem& option, const QRect& rect)
{
    if (!(option.state & QStyle::State_HasFocus) || (option.state & QStyle::State_Selected))
        return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    QRect cardRect = rect.adjusted(2, 2, -2, -2);
    QColor focusCol = option.palette.color(QPalette::Highlight);
    focusCol.setAlpha(150);
    painter->setPen(QPen(focusCol, 1.2, Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(cardRect, 6.0, 6.0);
    painter->restore();
}

// TODO this can be made a lot prettier
void drawProgressOverlay(QPainter* painter, const QStyleOptionViewItem& option, const int value, const int maximum)
{
    if (maximum == 0 || value == maximum) {
        return;
    }

    painter->save();

    qreal percent = (qreal)value / (qreal)maximum;
    QColor color = option.palette.color(QPalette::Dark);
    color.setAlphaF(0.70f);
    painter->setBrush(color);
    painter->setPen(QPen(QBrush(), 0));
    painter->drawPie(option.rect, 90 * 16, -percent * 360 * 16);

    painter->restore();
}

void drawBadges(QPainter* painter, const QStyleOptionViewItem& option, BaseInstance* instance, QIcon::Mode mode, QIcon::State state)
{
    QList<QString> pixmaps;
    if (instance->isRunning()) {
        pixmaps.append("status-running");
    } else if (instance->hasCrashed() || instance->hasVersionBroken()) {
        pixmaps.append("status-bad");
    }
    if (instance->hasUpdateAvailable()) {
        pixmaps.append("checkupdate");
    }

    static const int itemSide = 24;
    static const int spacing = 1;
    const int itemsPerRow = qMax(1, qFloor(double(option.rect.width() + spacing) / double(itemSide + spacing)));
    const int rows = qCeil((double)pixmaps.size() / (double)itemsPerRow);
    QListIterator<QString> it(pixmaps);
    painter->translate(option.rect.topLeft());
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < itemsPerRow; ++x) {
            if (!it.hasNext()) {
                return;
            }
            // FIXME: inject this.
            auto icon = QIcon::fromTheme(it.next());
            // opt.icon.paint(painter, iconbox, Qt::AlignCenter, mode, state);
            const QPixmap pixmap;
            // itemSide
            QRect badgeRect(option.rect.width() - x * itemSide + qMax(x - 1, 0) * spacing - itemSide,
                            y * itemSide + qMax(y - 1, 0) * spacing, itemSide, itemSide);
            icon.paint(painter, badgeRect, Qt::AlignCenter, mode, state);
        }
    }
    painter->translate(-option.rect.topLeft());
}

static QSize viewItemTextSize(const QStyleOptionViewItem* option)
{
    QStyle* style = option->widget ? option->widget->style() : QApplication::style();
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    QTextLayout textLayout;
    textLayout.setTextOption(textOption);
    textLayout.setFont(option->font);
    textLayout.setText(option->text);
    const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, option, option->widget) + 1;
    QRect bounds(0, 0, 100 - 2 * textMargin, 600);
    qreal height = 0, widthUsed = 0;
    viewItemTextLayout(textLayout, bounds.width(), height, widthUsed);
    const QSize size(qCeil(widthUsed), qCeil(height));
    return QSize(size.width() + 2 * textMargin, size.height());
}

void ListViewDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    painter->save();
    painter->setClipRect(opt.rect);

    opt.features |= QStyleOptionViewItem::WrapText;
    opt.text = index.data().toString();
    opt.textElideMode = Qt::ElideRight;
    opt.displayAlignment = Qt::AlignTop | Qt::AlignHCenter;

    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

    // const int iconSize =  style->pixelMetric(QStyle::PM_IconViewIconSize);
    const int iconSize = 48;
    QRect iconbox = opt.rect;
    const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, 0, opt.widget) + 1;
    QRect textRect = opt.rect;
    QRect textHighlightRect = textRect;
    // clip the decoration on top, remove width padding
    textRect.adjust(textMargin, iconSize + textMargin + 5, -textMargin, 0);

    textHighlightRect.adjust(0, iconSize + 5, 0, 0);

    auto instance = (BaseInstance*)index.data(InstanceList::InstancePointerRole).value<void*>();
    QRect cardRect = opt.rect.adjusted(2, 2, -2, -2);
    if (instance) {
        QPixmap banner = getCachedBanner(instance);
        if (!banner.isNull()) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
            QPainterPath clipPath;
            clipPath.addRoundedRect(cardRect, 6.0, 6.0);
            painter->setClipPath(clipPath);

            QPixmap scaled = banner.scaled(cardRect.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            int xOffset = qMax(0, (scaled.width() - cardRect.width()) / 2);
            int yOffset = qMax(0, (scaled.height() - cardRect.height()) / 2);
            painter->drawPixmap(cardRect.topLeft(), scaled, QRect(xOffset, yOffset, cardRect.width(), cardRect.height()));

            // Semi-transparent cinematic dark gradient overlay
            QLinearGradient grad(cardRect.topLeft(), cardRect.bottomLeft());
            grad.setColorAt(0.0, QColor(15, 18, 24, 110));
            grad.setColorAt(0.45, QColor(15, 18, 24, 150));
            grad.setColorAt(1.0, QColor(15, 18, 24, 235));
            painter->fillRect(cardRect, grad);
            painter->restore();
        }
    }

    drawSelectionRect(painter, opt, opt.rect);
    drawFocusRect(painter, opt, opt.rect);


    // icon mode and state, also used for badges
    QIcon::Mode mode = QIcon::Normal;
    if (!(opt.state & QStyle::State_Enabled))
        mode = QIcon::Disabled;
    else if (opt.state & QStyle::State_Selected)
        mode = QIcon::Selected;
    QIcon::State state = opt.state & QStyle::State_Open ? QIcon::On : QIcon::Off;

    // draw the icon
    {
        iconbox.setHeight(iconSize);
        opt.icon.paint(painter, iconbox, Qt::AlignCenter, mode, state);
    }
    // set the text colors
    QPalette::ColorGroup cg = opt.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
    if (cg == QPalette::Normal && !(opt.state & QStyle::State_Active))
        cg = QPalette::Inactive;
    if (opt.state & QStyle::State_Selected) {
        painter->setPen(opt.palette.color(cg, QPalette::HighlightedText));
    } else {
        painter->setPen(opt.palette.color(cg, QPalette::Text));
    }

    // draw the text
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    textOption.setTextDirection(opt.direction);
    textOption.setAlignment(QStyle::visualAlignment(opt.direction, opt.displayAlignment));
    QTextLayout textLayout;
    textLayout.setTextOption(textOption);
    textLayout.setFont(opt.font);
    textLayout.setText(opt.text);

    qreal width, height;
    viewItemTextLayout(textLayout, textRect.width(), height, width);

    const int lineCount = textLayout.lineCount();

    const QRect layoutRect = QStyle::alignedRect(opt.direction, opt.displayAlignment, QSize(textRect.width(), int(height)), textRect);
    const QPointF position = layoutRect.topLeft();
    for (int i = 0; i < lineCount; ++i) {
        const QTextLine line = textLayout.lineAt(i);
        line.draw(painter, position);
    }

    // Draw status badges if instance is valid
    if (instance) {
        drawBadges(painter, opt, instance, mode, state);
    }

    drawProgressOverlay(painter, opt, index.data(InstanceViewRoles::ProgressValueRole).toInt(),
                        index.data(InstanceViewRoles::ProgressMaximumRole).toInt());

    painter->restore();
}

QSize ListViewDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.features |= QStyleOptionViewItem::WrapText;
    opt.text = index.data().toString();
    opt.textElideMode = Qt::ElideRight;
    opt.displayAlignment = Qt::AlignTop | Qt::AlignHCenter;

    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &option, opt.widget) + 1;
    int height = 48 + textMargin * 2 + 5;  // TODO: turn constants into variables
    QSize szz = viewItemTextSize(&opt);
    height += szz.height();
    // FIXME: maybe the icon items could scale and keep proportions?
    QSize sz(100, height);
    return sz;
}

class NoReturnTextEdit : public QTextEdit {
    Q_OBJECT
   public:
    explicit NoReturnTextEdit(QWidget* parent) : QTextEdit(parent)
    {
        setTextInteractionFlags(Qt::TextEditorInteraction);
        setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    }
    bool event(QEvent* event) override
    {
        auto eventType = event->type();
        if (eventType == QEvent::KeyPress || eventType == QEvent::KeyRelease) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
            auto key = keyEvent->key();
            if ((key == Qt::Key_Return || key == Qt::Key_Enter) && eventType == QEvent::KeyPress) {
                emit editingDone();
                return true;
            }
            if (key == Qt::Key_Tab) {
                return true;
            }
        }
        return QTextEdit::event(event);
    }
   signals:
    void editingDone();
};

void ListViewDelegate::updateEditorGeometry(QWidget* editor,
                                            const QStyleOptionViewItem& option,
                                            [[maybe_unused]] const QModelIndex& index) const
{
    const int iconSize = 48;
    QRect textRect = option.rect;
    // QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    textRect.adjust(0, iconSize + 5, 0, 0);
    editor->setGeometry(textRect);
}

void ListViewDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    auto text = index.data(Qt::EditRole).toString();
    QTextEdit* realEditor = qobject_cast<NoReturnTextEdit*>(editor);
    realEditor->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    realEditor->append(text);
    realEditor->selectAll();
    realEditor->document()->clearUndoRedoStacks();
}

void ListViewDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    QTextEdit* realEditor = qobject_cast<NoReturnTextEdit*>(editor);
    QString text = realEditor->toPlainText();
    text.replace(QChar('\n'), QChar(' '));
    text = text.trimmed();
    // Prevent instance names longer than 128 chars
    text.truncate(128);
    if (text.size() != 0) {
        emit textChanged(model->data(index).toString(), text);
        model->setData(index, text);
    }
}

QWidget* ListViewDelegate::createEditor(QWidget* parent,
                                        [[maybe_unused]] const QStyleOptionViewItem& option,
                                        [[maybe_unused]] const QModelIndex& index) const
{
    auto editor = new NoReturnTextEdit(parent);
    connect(editor, &NoReturnTextEdit::editingDone, this, &ListViewDelegate::editingDone);
    return editor;
}

void ListViewDelegate::editingDone()
{
    NoReturnTextEdit* editor = qobject_cast<NoReturnTextEdit*>(sender());
    emit commitData(editor);
    emit closeEditor(editor);
}

#include "InstanceDelegate.moc"
