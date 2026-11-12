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

#pragma once

#include <QImage>
#include <QList>
#include <QMatrix4x4>
#include <QPoint>
#include <QSize>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <vector>

namespace opengl {
class BoxGeometry {
   public:
    BoxGeometry(QVector3D size, QVector3D position);
    BoxGeometry(QVector3D size, QVector3D position, QPoint uv, QVector3D textureDim, QSize textureSize = { 64, 64 });
    static BoxGeometry* Plane();
    virtual ~BoxGeometry();

    void rasterize(const QMatrix4x4& vpMatrix, const QImage& texture, QImage& target, std::vector<float>& zBuffer) const;

    void initGeometry(float u, float v, float width, float height, float depth, float textureWidth = 64, float textureHeight = 64);
    void rotate(float angle, const QVector3D& vector);
    void scale(const QVector3D& vector);

   private:
    struct VertexData {
        QVector4D position;
        QVector2D texCoord;
    };

    QList<VertexData> m_vertices;
    QVector3D m_size;
    QVector3D m_position;
    QMatrix4x4 m_matrix;
};
}  // namespace opengl
