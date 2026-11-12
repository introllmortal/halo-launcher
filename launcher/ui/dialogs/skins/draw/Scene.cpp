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

#include "ui/dialogs/skins/draw/Scene.h"

namespace opengl {
namespace {
QImage normalizeTexture(const QImage& inputImage, int defaultWidth, int defaultHeight)
{
    if (inputImage.isNull() || inputImage.width() <= 0 || inputImage.height() <= 0) {
        QImage img(defaultWidth, defaultHeight, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        return img;
    }
    if (inputImage.format() != QImage::Format_ARGB32) {
        return inputImage.convertToFormat(QImage::Format_ARGB32);
    }
    return inputImage;
}
}  // namespace

Scene::Scene(const QImage& skin, bool slim, const QImage& cape) : m_slim(slim), m_capeVisible(!cape.isNull())
{
    m_staticComponents = {
        // head
        new opengl::BoxGeometry(QVector3D(8, 8, 8), QVector3D(0, 4, 0), QPoint(0, 0), QVector3D(8, 8, 8)),
        // body
        new opengl::BoxGeometry(QVector3D(8, 12, 4), QVector3D(0, -6, 0), QPoint(16, 16), QVector3D(8, 12, 4)),
        // right leg
        new opengl::BoxGeometry(QVector3D(4, 12, 4), QVector3D(-1.9f, -18, -0.1f), QPoint(0, 16), QVector3D(4, 12, 4)),
        // left leg
        new opengl::BoxGeometry(QVector3D(4, 12, 4), QVector3D(1.9f, -18, -0.1f), QPoint(16, 48), QVector3D(4, 12, 4)),
    };

    m_staticComponentsOverlay = {
        // head
        new opengl::BoxGeometry(QVector3D(9, 9, 9), QVector3D(0, 4, 0), QPoint(32, 0), QVector3D(8, 8, 8)),
        // body
        new opengl::BoxGeometry(QVector3D(8.5, 12.5, 4.5), QVector3D(0, -6, 0), QPoint(16, 32), QVector3D(8, 12, 4)),
        // right leg
        new opengl::BoxGeometry(QVector3D(4.5f, 12.5f, 4.5f), QVector3D(-1.9f, -18, -0.1f), QPoint(0, 32), QVector3D(4, 12, 4)),
        // left leg
        new opengl::BoxGeometry(QVector3D(4.5f, 12.5f, 4.5f), QVector3D(1.9f, -18, -0.1f), QPoint(0, 48), QVector3D(4, 12, 4)),
    };

    m_normalArms = {
        // Right Arm
        new opengl::BoxGeometry(QVector3D(4, 12, 4), QVector3D(-6, -6, 0), QPoint(40, 16), QVector3D(4, 12, 4)),
        // Left Arm
        new opengl::BoxGeometry(QVector3D(4, 12, 4), QVector3D(6, -6, 0), QPoint(32, 48), QVector3D(4, 12, 4)),
    };

    m_normalArmsOverlay = {
        // Right Arm
        new opengl::BoxGeometry(QVector3D(4.5, 12.5, 4.5), QVector3D(-6, -6, 0), QPoint(40, 32), QVector3D(4, 12, 4)),
        // Left Arm
        new opengl::BoxGeometry(QVector3D(4.5, 12.5, 4.5), QVector3D(6, -6, 0), QPoint(48, 48), QVector3D(4, 12, 4)),
    };

    m_slimArms = {
        // Right Arm
        new opengl::BoxGeometry(QVector3D(3, 12, 4), QVector3D(-5.5, -6, 0), QPoint(40, 16), QVector3D(3, 12, 4)),
        // Left Arm
        new opengl::BoxGeometry(QVector3D(3, 12, 4), QVector3D(5.5, -6, 0), QPoint(32, 48), QVector3D(3, 12, 4)),
    };

    m_slimArmsOverlay = {
        // Right Arm
        new opengl::BoxGeometry(QVector3D(3.5, 12.5, 4.5), QVector3D(-5.5, -6, 0), QPoint(40, 32), QVector3D(3, 12, 4)),
        // Left Arm
        new opengl::BoxGeometry(QVector3D(3.5, 12.5, 4.5), QVector3D(5.5, -6, 0), QPoint(48, 48), QVector3D(3, 12, 4)),
    };

    m_cape = new opengl::BoxGeometry(QVector3D(10, 16, 1), QVector3D(0, -8, 2.5), QPoint(0, 0), QVector3D(10, 16, 1), QSize(64, 32));
    m_cape->rotate(10.8f, QVector3D(1, 0, 0));
    m_cape->rotate(180, QVector3D(0, 1, 0));

    auto leftWing =
        new opengl::BoxGeometry(QVector3D(12, 22, 4), QVector3D(0, -13, -2), QPoint(22, 0), QVector3D(10, 20, 2), QSize(64, 32));
    leftWing->rotate(15, QVector3D(1, 0, 0));
    leftWing->rotate(15, QVector3D(0, 0, 1));
    leftWing->rotate(1, QVector3D(1, 0, 0));
    auto rightWing =
        new opengl::BoxGeometry(QVector3D(12, 22, 4), QVector3D(0, -13, -2), QPoint(22, 0), QVector3D(10, 20, 2), QSize(64, 32));
    rightWing->scale(QVector3D(-1, 1, 1));
    rightWing->rotate(15, QVector3D(1, 0, 0));
    rightWing->rotate(15, QVector3D(0, 0, 1));
    rightWing->rotate(1, QVector3D(1, 0, 0));
    m_elytra << leftWing << rightWing;

    m_skinImage = normalizeTexture(skin, 64, 64);
    m_capeImage = normalizeTexture(cape, 64, 32);
}

Scene::~Scene()
{
    for (auto array :
         { m_staticComponents, m_normalArms, m_slimArms, m_elytra, m_staticComponentsOverlay, m_normalArmsOverlay, m_slimArmsOverlay }) {
        for (auto g : array) {
            delete g;
        }
    }
    delete m_cape;
}

void Scene::draw(const QMatrix4x4& vpMatrix, QImage& target, std::vector<float>& zBuffer) const
{
    if (!m_skinImage.isNull()) {
        for (const auto& toDraw : { m_staticComponents, m_slim ? m_slimArms : m_normalArms, m_staticComponentsOverlay,
                                    m_slim ? m_slimArmsOverlay : m_normalArmsOverlay }) {
            for (const auto* g : toDraw) {
                g->rasterize(vpMatrix, m_skinImage, target, zBuffer);
            }
        }
    }
    if (m_capeVisible && !m_capeImage.isNull()) {
        if (!m_elytraVisible) {
            m_cape->rasterize(vpMatrix, m_capeImage, target, zBuffer);
        } else {
            for (const auto* e : m_elytra) {
                e->rasterize(vpMatrix, m_capeImage, target, zBuffer);
            }
        }
    }
}

void Scene::setSkin(const QImage& skin)
{
    m_skinImage = normalizeTexture(skin, 64, 64);
}

void Scene::setMode(bool slim)
{
    m_slim = slim;
}

void Scene::setCape(const QImage& cape)
{
    m_capeImage = normalizeTexture(cape, 64, 32);
}

void Scene::setCapeVisible(bool visible)
{
    m_capeVisible = visible;
}

void Scene::setElytraVisible(bool elytraVisible)
{
    m_elytraVisible = elytraVisible;
}
}  // namespace opengl
