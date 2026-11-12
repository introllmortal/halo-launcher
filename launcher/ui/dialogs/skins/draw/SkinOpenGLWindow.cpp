// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2024 Trial97 <alexandru.tripon97@gmail.com>
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
 */

#include "ui/dialogs/skins/draw/SkinOpenGLWindow.h"

#include <QMouseEvent>
#include <QPainter>
#include <QVector3D>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <functional>

#include "minecraft/skins/SkinModel.h"
#include "rainbow.h"
#include "ui/dialogs/skins/draw/Scene.h"

namespace {
QColor calculateContrastingColor(const QColor& color)
{
    auto luma = Rainbow::luma(color);
    if (luma < 0.5) {
        constexpr float contrast = 0.05f;
        return Rainbow::lighten(color, contrast);
    }
    constexpr float contrast = 0.2f;
    return Rainbow::darken(color, contrast);
}
}  // namespace

SkinOpenGLWindow::SkinOpenGLWindow(SkinProvider* parent, QColor color, QWidget* widgetParent)
    : QWidget(widgetParent), m_baseColor(color), m_parent(parent)
{
    const bool isDarkBase = Rainbow::luma(color) < 0.5;
    const float contrast = isDarkBase ? 0.05f : 0.45f;
    auto contrastFunc = std::bind(isDarkBase ? Rainbow::lighten : Rainbow::darken, std::placeholders::_1, contrast, 1.0);
    m_checkerLight = contrastFunc(color);
    m_checkerDark = contrastFunc(calculateContrastingColor(color));

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(120, 160);
}

SkinOpenGLWindow::~SkinOpenGLWindow()
{
    delete m_scene;
    m_scene = nullptr;
}

void SkinOpenGLWindow::ensureSceneInitialized()
{
    if (m_scene) {
        return;
    }

    QImage skin;
    QImage cape;
    bool slim = false;
    if (m_skinDirty) {
        skin = m_pendingSkin;
        slim = m_pendingSlim;
        m_skinDirty = false;
    } else if (m_parent) {
        if (auto s = m_parent->getSelectedSkin()) {
            skin = s->getTexture();
            slim = (s->getModel() == SkinModel::SLIM);
            cape = m_parent->capes().value(s->getCapeId(), {});
        }
    }
    if (m_capeDirty) {
        cape = m_pendingCape;
        m_capeDirty = false;
    }

    m_scene = new opengl::Scene(skin, slim, cape);
    if (m_elytraDirty) {
        m_scene->setElytraVisible(m_pendingElytra);
        m_elytraDirty = false;
    }
}

void SkinOpenGLWindow::mousePressEvent(QMouseEvent* e)
{
    m_mousePosition = QVector2D(e->pos());
    m_isMousePressed = true;
}

void SkinOpenGLWindow::mouseMoveEvent(QMouseEvent* event)
{
    // Prevents mouse sticking on Wayland compositors
    if (!(event->buttons() & Qt::MouseButton::LeftButton)) {
        m_isMousePressed = false;
        return;
    }

    if (m_isMousePressed) {
        const float dx = static_cast<float>(event->position().x() - m_mousePosition.x());
        const float dy = static_cast<float>(event->position().y() - m_mousePosition.y());

        m_yaw += dx * 0.5f;
        m_pitch = std::clamp(m_pitch + dy * 0.5f, -85.0f, 85.0f);

        if (m_yaw > 360.0f) {
            m_yaw -= 360.0f;
        } else if (m_yaw < 0.0f) {
            m_yaw += 360.0f;
        }

        m_mousePosition = QVector2D(event->pos());
        update();
    }
}

void SkinOpenGLWindow::mouseReleaseEvent([[maybe_unused]] QMouseEvent* e)
{
    m_isMousePressed = false;
}

void SkinOpenGLWindow::renderBackground(QImage& target) const
{
    const int width = target.width();
    const int height = target.height();
    const QRgb lightRgb = m_checkerLight.rgba();
    const QRgb darkRgb = m_checkerDark.rgba();

    constexpr int gridSize = 32;
    for (int y = 0; y < height; ++y) {
        const int ty = (y * gridSize) / height;
        auto* row = reinterpret_cast<QRgb*>(target.scanLine(y));
        for (int x = 0; x < width; ++x) {
            const int tx = (x * gridSize) / width;
            row[x] = ((tx + ty) & 1) == 0 ? lightRgb : darkRgb;
        }
    }
}

void SkinOpenGLWindow::paintEvent([[maybe_unused]] QPaintEvent* event)
{
    const qreal dpr = devicePixelRatioF();
    const int pixelW = std::max(1, static_cast<int>(std::lround(static_cast<qreal>(width()) * dpr)));
    const int pixelH = std::max(1, static_cast<int>(std::lround(static_cast<qreal>(height()) * dpr)));

    ensureSceneInitialized();

    if (m_skinDirty) {
        m_scene->setMode(m_pendingSlim);
        m_scene->setSkin(m_pendingSkin);
        m_skinDirty = false;
    }
    if (m_capeDirty) {
        m_scene->setCapeVisible(!m_pendingCape.isNull());
        m_scene->setCape(m_pendingCape);
        m_capeDirty = false;
    }
    if (m_elytraDirty) {
        m_scene->setElytraVisible(m_pendingElytra);
        m_elytraDirty = false;
    }

    QImage frame(pixelW, pixelH, QImage::Format_ARGB32);
    renderBackground(frame);

    m_zBuffer.assign(static_cast<size_t>(pixelW) * static_cast<size_t>(pixelH), 2.0f);

    const float aspect = static_cast<float>(pixelW) / static_cast<float>(pixelH);
    QMatrix4x4 projection;
    projection.perspective(45.0f, aspect, 1.0f, 500.0f);

    QMatrix4x4 view;
    const float yawRad = qDegreesToRadians(m_yaw);
    const float pitchRad = qDegreesToRadians(m_pitch);
    view.lookAt(QVector3D(
                    m_distance * qCos(pitchRad) * qCos(yawRad),
                    m_distance * qSin(pitchRad) - 8.0f,
                    m_distance * qCos(pitchRad) * qSin(yawRad)),
                QVector3D(0.0f, -8.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));

    m_scene->draw(projection * view, frame, m_zBuffer);

    frame.setDevicePixelRatio(dpr);
    QPainter painter(this);
    painter.drawImage(0, 0, frame);
}

void SkinOpenGLWindow::updateScene(SkinModel* skin)
{
    if (!skin) {
        return;
    }
    m_pendingSkin = skin->getTexture();
    m_pendingSlim = (skin->getModel() == SkinModel::SLIM);
    m_skinDirty = true;
    if (m_scene) {
        m_scene->setMode(m_pendingSlim);
        m_scene->setSkin(m_pendingSkin);
        m_skinDirty = false;
    }
    update();
}

void SkinOpenGLWindow::updateCape(const QImage& cape)
{
    m_pendingCape = cape;
    m_capeDirty = true;
    if (m_scene) {
        m_scene->setCapeVisible(!m_pendingCape.isNull());
        m_scene->setCape(m_pendingCape);
        m_capeDirty = false;
    }
    update();
}

void SkinOpenGLWindow::wheelEvent(QWheelEvent* event)
{
    const int delta = event->angleDelta().y();
    m_distance = std::clamp(m_distance - static_cast<float>(delta) * 0.01f, 18.0f, 120.0f);
    update();
}

void SkinOpenGLWindow::setElytraVisible(bool visible)
{
    m_pendingElytra = visible;
    m_elytraDirty = true;
    if (m_scene) {
        m_scene->setElytraVisible(visible);
        m_elytraDirty = false;
    }
    update();
}

bool SkinOpenGLWindow::hasOpenGL()
{
    return true;
}
