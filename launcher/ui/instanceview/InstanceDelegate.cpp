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
#include <QIcon>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTextEdit>
#include <QTextLayout>
#include <QTextOption>
#include <QtMath>

#include "Application.h"
#include "BaseInstance.h"
#include "InstanceList.h"
#include "InstanceView.h"
#include "MMCTime.h"
#include "translations/TranslationsModel.h"

namespace {
constexpr int kCardWidth = 168;
constexpr int kCardHeight = 152;
constexpr int kIconSize = 56;
constexpr qreal kCardRadius = 10.0;

void viewItemTextLayout(QTextLayout& textLayout, int lineWidth, int maxLines, qreal& height, qreal& widthUsed)
{
    height = 0;
    widthUsed = 0;
    textLayout.beginLayout();
    int lineIndex = 0;
    while (lineIndex < maxLines) {
        QTextLine line = textLayout.createLine();
        if (!line.isValid() || line.textLength() == 0) {
            break;
        }
        line.setLineWidth(lineWidth);
        line.setPosition(QPointF(0, height));
        height += line.height();
        widthUsed = qMax(widthUsed, line.naturalTextWidth());
        ++lineIndex;
    }
    textLayout.endLayout();
}

void drawStatusPills(QPainter* painter, const QRectF& cardRect, BaseInstance* instance)
{
    if (!instance) {
        return;
    }

    QList<QColor> dotColors;
    if (instance->isRunning()) {
        dotColors.append(QColor(27, 217, 106));
    } else if (instance->hasCrashed() || instance->hasVersionBroken()) {
        dotColors.append(QColor(239, 68, 68));
    }
    if (instance->hasUpdateAvailable()) {
        dotColors.append(QColor(14, 165, 233));
    }

    if (dotColors.isEmpty()) {
        return;
    }

    painter->save();
    qreal x = cardRect.right() - 10.0;
    const qreal y = cardRect.top() + 10.0;
    for (const QColor& col : dotColors) {
        painter->setPen(QPen(QColor(0, 0, 0, 120), 1.0));
        painter->setBrush(col);
        painter->drawEllipse(QRectF(x - 8.0, y, 8.0, 8.0));
        x -= 12.0;
    }
    painter->restore();
}

void drawCardProgressBar(QPainter* painter, const QRectF& cardRect, const QColor& accent, int value, int maximum)
{
    if (maximum <= 0 || value >= maximum) {
        return;
    }
    const qreal ratio = qBound<qreal>(0.0, static_cast<qreal>(value) / static_cast<qreal>(maximum), 1.0);
    const QRectF track(cardRect.left() + 10.0, cardRect.bottom() - 8.0, cardRect.width() - 20.0, 4.0);

    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 255, 255, 32));
    painter->drawRoundedRect(track, 2.0, 2.0);

    QRectF fill = track;
    fill.setWidth(track.width() * ratio);
    painter->setBrush(accent);
    painter->drawRoundedRect(fill, 2.0, 2.0);
    painter->restore();
}

QString cardSubtitle(BaseInstance* instance, const QModelIndex& index)
{
    if (!instance) {
        return {};
    }
    if (instance->isRunning()) {
        return TranslationsModel::trFallback(APPLICATION ? APPLICATION->translations() : nullptr,
                                             QObject::tr("Running"),
                                             "Running",
                                             QStringLiteral("Запущена"));
    }
    if (instance->hasVersionBroken()) {
        return TranslationsModel::trFallback(APPLICATION ? APPLICATION->translations() : nullptr,
                                             QObject::tr("Broken"),
                                             "Broken",
                                             QStringLiteral("Повреждена"));
    }
    if (instance->isManagedPack() && !instance->getManagedPackVersionName().isEmpty()) {
        return instance->getManagedPackVersionName();
    }
    if (instance->totalTimePlayed() > 0) {
        return Time::prettifyDuration(instance->totalTimePlayed(), APPLICATION->settings()->get("ShowGameTimeWithoutDays").toBool());
    }
    const QString group = index.data(InstanceViewRoles::GroupRole).toString();
    return group;
}
}  // namespace

ListViewDelegate::ListViewDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void ListViewDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setClipRect(opt.rect);

    const bool selected = opt.state.testFlag(QStyle::State_Selected);
    const bool hovered = opt.state.testFlag(QStyle::State_MouseOver);
    const bool dark = opt.palette.color(QPalette::Window).lightness() < 128;

    QColor accent = opt.palette.color(QPalette::Highlight);
    if (!accent.isValid()) {
        accent = QColor(14, 165, 233);
    }

    QColor bg = dark ? QColor(30, 34, 43, 224) : QColor(255, 255, 255, 228);
    QColor border = dark ? QColor(46, 52, 66, 220) : QColor(203, 213, 225, 220);
    qreal borderWidth = 1.0;

    if (hovered) {
        bg = dark ? QColor(38, 43, 55, 238) : QColor(241, 245, 249, 242);
        border = dark ? QColor(68, 79, 102, 235) : QColor(148, 163, 184, 235);
    }
    if (selected) {
        QColor tint = accent;
        tint.setAlpha(dark ? 42 : 34);
        bg = dark ? QColor(28, 36, 48, 242) : QColor(239, 246, 255, 245);
        // Layer subtle accent wash over card surface
        QPainterPath path;
        const QRectF cardRect = QRectF(opt.rect).adjusted(1.5, 1.5, -1.5, -1.5);
        path.addRoundedRect(cardRect, kCardRadius, kCardRadius);
        painter->fillPath(path, bg);
        painter->fillPath(path, tint);
        border = accent;
        borderWidth = 2.0;
    } else {
        const QRectF cardRect = QRectF(opt.rect).adjusted(1.5, 1.5, -1.5, -1.5);
        QPainterPath path;
        path.addRoundedRect(cardRect, kCardRadius, kCardRadius);
        painter->fillPath(path, bg);
    }

    const QRectF cardRect = QRectF(opt.rect).adjusted(1.5, 1.5, -1.5, -1.5);
    painter->setPen(QPen(border, borderWidth));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(cardRect, kCardRadius, kCardRadius);

    // Icon badge + icon
    const QRect iconRect(qRound(cardRect.left() + (cardRect.width() - kIconSize) / 2.0), qRound(cardRect.top() + 16.0), kIconSize,
                         kIconSize);
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(dark ? QColor(18, 20, 26, 150) : QColor(226, 232, 240, 140));
        painter->drawRoundedRect(QRectF(iconRect).adjusted(-5, -5, 5, 5), 10.0, 10.0);
        opt.icon.paint(painter, iconRect, Qt::AlignCenter, opt.state.testFlag(QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled,
                       QIcon::Off);
    }

    auto* instance = static_cast<BaseInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
    drawStatusPills(painter, cardRect, instance);

    // Title
    QFont titleFont = opt.font;
    titleFont.setWeight(QFont::DemiBold);
    const QRectF titleArea(cardRect.left() + 10.0, iconRect.bottom() + 10.0, cardRect.width() - 20.0, 38.0);
    const QColor titleColor = dark ? QColor(241, 245, 249) : QColor(15, 23, 42);
    painter->setPen(titleColor);
    painter->setFont(titleFont);

    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    textOption.setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    const QString titleText = index.data(Qt::DisplayRole).toString();
    QFontMetricsF fm(titleFont);
    QTextLayout textLayout;
    textLayout.setTextOption(textOption);
    textLayout.setFont(titleFont);
    textLayout.setText(titleText);
    qreal height = 0.0;
    qreal widthUsed = 0.0;
    viewItemTextLayout(textLayout, qRound(titleArea.width()), 2, height, widthUsed);

    const QPointF titleOrigin(titleArea.left(), titleArea.top());
    const int drawnLines = qMin(2, textLayout.lineCount());
    for (int i = 0; i < drawnLines; ++i) {
        QTextLine line = textLayout.lineAt(i);
        if (i == 1 && (line.textStart() + line.textLength()) < titleText.length()) {
            const QString tail = fm.elidedText(titleText.mid(line.textStart()), Qt::ElideRight, titleArea.width());
            painter->drawText(QRectF(titleArea.left(), titleArea.top() + line.y(), titleArea.width(), line.height()),
                              Qt::AlignTop | Qt::AlignHCenter, tail);
        } else {
            line.draw(painter, titleOrigin);
        }
    }

    // Subtitle
    const QString sub = cardSubtitle(instance, index);
    if (!sub.isEmpty()) {
        QFont subFont = opt.font;
        if (subFont.pointSizeF() > 7.5) {
            subFont.setPointSizeF(subFont.pointSizeF() - 1.0);
        }
        painter->setFont(subFont);
        const bool isRunning = instance && instance->isRunning();
        const QColor subColor = isRunning ? QColor(74, 222, 128) : (dark ? QColor(148, 163, 184) : QColor(100, 116, 139));
        painter->setPen(subColor);

        const QRectF subRect(cardRect.left() + 10.0, cardRect.bottom() - 26.0, cardRect.width() - 20.0, 16.0);
        const QString elidedSub = QFontMetricsF(subFont).elidedText(sub, Qt::ElideRight, subRect.width());
        painter->drawText(subRect, Qt::AlignCenter, elidedSub);
    }

    drawCardProgressBar(painter, cardRect, accent, index.data(InstanceViewRoles::ProgressValueRole).toInt(),
                        index.data(InstanceViewRoles::ProgressMaximumRole).toInt());

    painter->restore();
}

QSize ListViewDelegate::sizeHint([[maybe_unused]] const QStyleOptionViewItem& option,
                                 [[maybe_unused]] const QModelIndex& index) const
{
    return QSize(kCardWidth, kCardHeight);
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
    QRect textRect = option.rect.adjusted(8, kIconSize + 24, -8, -10);
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
        const QString before = model->data(index).toString();
        model->setData(index, text);
        emit textChanged(before, text);
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
