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
#include "ui/widgets/UiAnimations.h"

namespace {
constexpr int kCardWidth = 138;
constexpr int kCardHeight = 128;
constexpr int kIconSize = 44;
constexpr qreal kCardRadius = 9.0;

QColor lerpColor(const QColor& from, const QColor& to, qreal t)
{
    const qreal k = qBound<qreal>(0.0, t, 1.0);
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * k,
                            from.greenF() + (to.greenF() - from.greenF()) * k,
                            from.blueF() + (to.blueF() - from.blueF()) * k,
                            from.alphaF() + (to.alphaF() - from.alphaF()) * k);
}

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

void drawStatusPills(QPainter* painter, const QRectF& cardRect, BaseInstance* instance, qreal pulseP)
{
    if (!instance) {
        return;
    }

    struct StatusDot {
        QColor color;
        bool pulse = false;
    };

    QList<StatusDot> dots;
    if (instance->isRunning()) {
        dots.append({ QColor(27, 217, 106), true });
    } else if (instance->hasCrashed() || instance->hasVersionBroken()) {
        dots.append({ QColor(239, 68, 68), false });
    }
    if (instance->hasUpdateAvailable()) {
        dots.append({ QColor(14, 165, 233), true });
    }

    if (dots.isEmpty()) {
        return;
    }

    painter->save();
    qreal x = cardRect.right() - 8.0;
    const qreal y = cardRect.top() + 8.0;
    for (const StatusDot& dot : dots) {
        const QRectF dotRect(x - 7.0, y, 7.0, 7.0);
        if (dot.pulse && pulseP > 0.01) {
            QColor halo = dot.color;
            halo.setAlpha(qRound(30.0 + 75.0 * pulseP));
            const qreal expand = 1.2 + 1.4 * pulseP;
            painter->setPen(QPen(halo, 1.2));
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(dotRect.adjusted(-expand, -expand, expand, expand));
        }
        painter->setPen(QPen(QColor(0, 0, 0, 120), 1.0));
        painter->setBrush(dot.color);
        painter->drawEllipse(dotRect);
        x -= 10.0;
    }
    painter->restore();
}

void drawCardProgressBar(QPainter* painter, const QRectF& cardRect, const QColor& accent, int value, int maximum)
{
    if (maximum <= 0 || value >= maximum) {
        return;
    }
    const qreal ratio = qBound<qreal>(0.0, static_cast<qreal>(value) / static_cast<qreal>(maximum), 1.0);
    const QRectF track(cardRect.left() + 8.0, cardRect.bottom() - 5.5, cardRect.width() - 16.0, 3.0);

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

    const auto* view = qobject_cast<const InstanceView*>(opt.widget);
    const qreal hoverP = view ? view->hoverProgress(index, hovered) : (hovered ? 1.0 : 0.0);
    const qreal selectP = view ? view->selectProgress(index, selected) : (selected ? 1.0 : 0.0);
    const qreal pressP = view ? view->pressProgress(index) : 0.0;
    const qreal pulseP = view ? view->statusPulseValue() : 0.0;

    QColor accent = opt.palette.color(QPalette::Highlight);
    if (!accent.isValid()) {
        accent = QColor(56, 189, 248);
    }

    const QColor baseBg = dark ? QColor(24, 29, 43, 238) : QColor(255, 255, 255, 228);
    const QColor hoverBg = dark ? QColor(33, 41, 61, 245) : QColor(241, 245, 249, 242);
    const QColor selectBg = dark ? QColor(25, 32, 49, 248) : QColor(239, 246, 255, 245);

    const QColor baseBorder = dark ? QColor(45, 56, 82, 235) : QColor(203, 213, 225, 220);
    const QColor hoverBorder = dark ? QColor(88, 110, 158, 245) : QColor(148, 163, 184, 235);
    const QColor selectBorder = dark ? QColor(56, 189, 248, 220) : accent;

    QColor bg = lerpColor(lerpColor(baseBg, hoverBg, hoverP), selectBg, selectP);
    if (pressP > 0.01) {
        const QColor pressTarget = dark ? QColor(18, 22, 34, 250) : QColor(226, 232, 240, 248);
        bg = lerpColor(bg, pressTarget, pressP * 0.45);
    }

    const QColor border = lerpColor(lerpColor(baseBorder, hoverBorder, hoverP), selectBorder, selectP);
    const qreal borderWidth = 1.0 + 0.5 * selectP;
    const qreal pressInset = 0.85 * pressP;
    const QRectF cardRect = QRectF(opt.rect).adjusted(3.0 + pressInset, 3.0 + pressInset, -3.0 - pressInset, -3.0 - pressInset);

    QPainterPath path;
    path.addRoundedRect(cardRect, kCardRadius, kCardRadius);

    painter->fillPath(path, bg);
    if (selectP > 0.01) {
        QColor tint = accent;
        tint.setAlpha(qRound((dark ? 16.0 : 22.0) * selectP));
        painter->fillPath(path, tint);
    }

    painter->setPen(QPen(border, borderWidth));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(cardRect, kCardRadius, kCardRadius);

    // Icon badge + icon
    const QRect iconRect(qRound(cardRect.left() + (cardRect.width() - kIconSize) / 2.0), qRound(cardRect.top() + 11.0), kIconSize,
                         kIconSize);
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(dark ? QColor(16, 19, 26, 165) : QColor(226, 232, 240, 140));
        painter->drawRoundedRect(QRectF(iconRect).adjusted(-4, -4, 4, 4), 8.0, 8.0);
        opt.icon.paint(painter, iconRect, Qt::AlignCenter, opt.state.testFlag(QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled,
                       QIcon::Off);
    }

    auto* instance = static_cast<BaseInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
    drawStatusPills(painter, cardRect, instance, pulseP);

    // Title
    QFont titleFont = opt.font;
    titleFont.setWeight(QFont::Bold);
    const QRectF titleArea(cardRect.left() + 8.0, iconRect.bottom() + 7.0, cardRect.width() - 16.0, 34.0);
    const QColor titleColor = dark ? QColor(248, 250, 252) : QColor(15, 23, 42);
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

    // Subtitle / modpack version pill
    const QString sub = cardSubtitle(instance, index);
    if (!sub.isEmpty()) {
        const bool isRunning = instance && instance->isRunning();
        const bool isBroken = instance && instance->hasVersionBroken();
        const bool isPackVersion = instance && !isRunning && !isBroken && instance->isManagedPack() &&
                                   !instance->getManagedPackVersionName().isEmpty();

        QFont subFont = opt.font;
        if (subFont.pointSizeF() > 7.5) {
            subFont.setPointSizeF(subFont.pointSizeF() - 1.0);
        }
        if (isRunning || isBroken || isPackVersion) {
            subFont.setWeight(QFont::DemiBold);
        }
        painter->setFont(subFont);

        const QFontMetricsF subFm(subFont);
        const qreal maxTextWidth = cardRect.width() - 28.0;
        const QString elidedSub = subFm.elidedText(sub, Qt::ElideRight, maxTextWidth);
        const qreal pillWidth = qMin(cardRect.width() - 16.0, subFm.horizontalAdvance(elidedSub) + 14.0);
        const qreal pillHeight = 18.0;
        const QRectF pillRect(cardRect.left() + (cardRect.width() - pillWidth) / 2.0, cardRect.bottom() - 23.0, pillWidth, pillHeight);

        QColor pillBg;
        QColor pillBorder;
        QColor subColor;
        if (isRunning) {
            pillBg = QColor(34, 197, 94, dark ? 40 : 32);
            pillBorder = QColor(74, 222, 128, dark ? 140 : 170);
            subColor = dark ? QColor(134, 239, 172) : QColor(21, 128, 61);
        } else if (isBroken) {
            pillBg = QColor(239, 68, 68, dark ? 40 : 30);
            pillBorder = QColor(248, 113, 113, dark ? 145 : 175);
            subColor = dark ? QColor(252, 165, 165) : QColor(185, 28, 28);
        } else if (isPackVersion) {
            pillBg = dark ? QColor(14, 116, 144, 72) : QColor(224, 242, 254, 235);
            pillBorder = dark ? QColor(56, 189, 248, 150) : QColor(2, 132, 199, 145);
            subColor = dark ? QColor(224, 242, 254) : QColor(3, 105, 161);
        } else {
            pillBg = dark ? QColor(30, 38, 56, 160) : QColor(241, 245, 249, 210);
            pillBorder = dark ? QColor(62, 76, 108, 140) : QColor(203, 213, 225, 200);
            subColor = dark ? QColor(148, 163, 184) : QColor(100, 116, 139);
        }

        painter->setPen(QPen(pillBorder, 1.0));
        painter->setBrush(pillBg);
        painter->drawRoundedRect(pillRect, pillHeight / 2.0, pillHeight / 2.0);

        painter->setPen(subColor);
        painter->drawText(pillRect, Qt::AlignCenter, elidedSub);
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
    QRect textRect = option.rect.adjusted(6, kIconSize + 18, -6, -8);
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
