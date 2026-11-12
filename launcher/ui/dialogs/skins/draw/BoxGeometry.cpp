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

#include "BoxGeometry.h"

#include <QColor>
#include <QList>
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cmath>

// For cube we would need only 8 vertices but we have to
// duplicate vertex for each face because texture coordinate
// is different.
static const QList<QVector4D> vertices = {
    // Vertex data for face 0
    QVector4D(-0.5f, -0.5f, 0.5f, 1.0f),  // v0
    QVector4D(0.5f, -0.5f, 0.5f, 1.0f),   // v1
    QVector4D(-0.5f, 0.5f, 0.5f, 1.0f),   // v2
    QVector4D(0.5f, 0.5f, 0.5f, 1.0f),    // v3
    // Vertex data for face 1
    QVector4D(0.5f, -0.5f, 0.5f, 1.0f),   // v4
    QVector4D(0.5f, -0.5f, -0.5f, 1.0f),  // v5
    QVector4D(0.5f, 0.5f, 0.5f, 1.0f),    // v6
    QVector4D(0.5f, 0.5f, -0.5f, 1.0f),   // v7

    // Vertex data for face 2
    QVector4D(0.5f, -0.5f, -0.5f, 1.0f),   // v8
    QVector4D(-0.5f, -0.5f, -0.5f, 1.0f),  // v9
    QVector4D(0.5f, 0.5f, -0.5f, 1.0f),    // v10
    QVector4D(-0.5f, 0.5f, -0.5f, 1.0f),   // v11

    // Vertex data for face 3
    QVector4D(-0.5f, -0.5f, -0.5f, 1.0f),  // v12
    QVector4D(-0.5f, -0.5f, 0.5f, 1.0f),   // v13
    QVector4D(-0.5f, 0.5f, -0.5f, 1.0f),   // v14
    QVector4D(-0.5f, 0.5f, 0.5f, 1.0f),    // v15

    // Vertex data for face 4
    QVector4D(-0.5f, -0.5f, -0.5f, 1.0f),  // v16
    QVector4D(0.5f, -0.5f, -0.5f, 1.0f),   // v17
    QVector4D(-0.5f, -0.5f, 0.5f, 1.0f),   // v18
    QVector4D(0.5f, -0.5f, 0.5f, 1.0f),    // v19

    // Vertex data for face 5
    QVector4D(-0.5f, 0.5f, 0.5f, 1.0f),   // v20
    QVector4D(0.5f, 0.5f, 0.5f, 1.0f),    // v21
    QVector4D(-0.5f, 0.5f, -0.5f, 1.0f),  // v22
    QVector4D(0.5f, 0.5f, -0.5f, 1.0f),   // v23
};

QList<QVector4D> transformVectors(const QMatrix4x4& matrix, const QList<QVector4D>& vectors)
{
    QList<QVector4D> transformedVectors;
    transformedVectors.reserve(vectors.size());

    for (const QVector4D& vec : vectors) {
        if (!matrix.isIdentity()) {
            transformedVectors.append(matrix * vec);
        } else {
            transformedVectors.append(vec);
        }
    }

    return transformedVectors;
}

// Function to calculate UV coordinates
// this is pure magic (if something is wrong with textures this is at fault)
QList<QVector2D> getCubeUVs(float u, float v, float width, float height, float depth, float textureWidth, float textureHeight)
{
    auto toFaceVertices = [textureHeight, textureWidth](float x1, float y1, float x2, float y2) -> QList<QVector2D> {
        return {
            QVector2D(x1 / textureWidth, 1.0f - y2 / textureHeight),
            QVector2D(x2 / textureWidth, 1.0f - y2 / textureHeight),
            QVector2D(x2 / textureWidth, 1.0f - y1 / textureHeight),
            QVector2D(x1 / textureWidth, 1.0f - y1 / textureHeight),
        };
    };

    auto top = toFaceVertices(u + depth, v, u + width + depth, v + depth);
    auto bottom = toFaceVertices(u + width + depth, v, u + width * 2 + depth, v + depth);
    auto left = toFaceVertices(u, v + depth, u + depth, v + depth + height);
    auto front = toFaceVertices(u + depth, v + depth, u + width + depth, v + depth + height);
    auto right = toFaceVertices(u + width + depth, v + depth, u + width + depth * 2, v + height + depth);
    auto back = toFaceVertices(u + width + depth * 2, v + depth, u + width * 2 + depth * 2, v + height + depth);

    auto uvRight = {
        right[0],
        right[1],
        right[3],
        right[2],
    };
    auto uvLeft = {
        left[0],
        left[1],
        left[3],
        left[2],
    };
    auto uvTop = {
        top[0],
        top[1],
        top[3],
        top[2],
    };
    auto uvBottom = {
        bottom[3],
        bottom[2],
        bottom[0],
        bottom[1],
    };
    auto uvFront = {
        front[0],
        front[1],
        front[3],
        front[2],
    };
    auto uvBack = {
        back[0],
        back[1],
        back[3],
        back[2],
    };

    QList<QVector2D> uvData;
    uvData.reserve(24);

    for (const auto& uvArray : { uvFront, uvRight, uvBack, uvLeft, uvBottom, uvTop }) {
        uvData.append(uvArray);
    }

    return uvData;
}

namespace {
struct ClipVertex {
    QVector4D clip;
    QVector2D uv;
};

void rasterizeTriangle(const std::array<ClipVertex, 3>& tri, const QImage& texture, QImage& target, std::vector<float>& zBuffer)
{
    const int width = target.width();
    const int height = target.height();
    const int texW = texture.width();
    const int texH = texture.height();

    std::array<float, 3> sx{};
    std::array<float, 3> sy{};
    std::array<float, 3> ndcZ{};
    std::array<float, 3> invW{};
    std::array<float, 3> uOverW{};
    std::array<float, 3> vOverW{};

    for (int i = 0; i < 3; ++i) {
        const float w = tri[i].clip.w();
        invW[i] = 1.0f / w;
        const float nx = tri[i].clip.x() * invW[i];
        const float ny = tri[i].clip.y() * invW[i];
        ndcZ[i] = tri[i].clip.z() * invW[i];
        sx[i] = (nx * 0.5f + 0.5f) * static_cast<float>(width);
        sy[i] = (0.5f - ny * 0.5f) * static_cast<float>(height);
        uOverW[i] = tri[i].uv.x() * invW[i];
        vOverW[i] = tri[i].uv.y() * invW[i];
    }

    const float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sy[1] - sy[0]) * (sx[2] - sx[0]);
    if (std::abs(area) < 1e-5f) {
        return;
    }
    const float invArea = 1.0f / area;

    const int minX = std::max(0, static_cast<int>(std::floor(std::min({ sx[0], sx[1], sx[2] }))));
    const int maxX = std::min(width - 1, static_cast<int>(std::ceil(std::max({ sx[0], sx[1], sx[2] }))));
    const int minY = std::max(0, static_cast<int>(std::floor(std::min({ sy[0], sy[1], sy[2] }))));
    const int maxY = std::min(height - 1, static_cast<int>(std::ceil(std::max({ sy[0], sy[1], sy[2] }))));

    for (int y = minY; y <= maxY; ++y) {
        const float py = static_cast<float>(y) + 0.5f;
        auto* dstRow = reinterpret_cast<QRgb*>(target.scanLine(y));
        const int rowOffset = y * width;

        for (int x = minX; x <= maxX; ++x) {
            const float px = static_cast<float>(x) + 0.5f;

            const float e0 = ((sx[2] - sx[1]) * (py - sy[1]) - (sy[2] - sy[1]) * (px - sx[1])) * invArea;
            const float e1 = ((sx[0] - sx[2]) * (py - sy[2]) - (sy[0] - sy[2]) * (px - sx[2])) * invArea;
            const float e2 = 1.0f - e0 - e1;

            if (e0 < -1e-5f || e1 < -1e-5f || e2 < -1e-5f) {
                continue;
            }

            const float z = e0 * ndcZ[0] + e1 * ndcZ[1] + e2 * ndcZ[2];
            const int idx = rowOffset + x;
            if (z < -1.0f || z > 1.0f || z >= zBuffer[idx]) {
                continue;
            }

            const float wRecip = e0 * invW[0] + e1 * invW[1] + e2 * invW[2];
            if (wRecip <= 1e-6f) {
                continue;
            }
            const float invWRecip = 1.0f / wRecip;
            const float u = (e0 * uOverW[0] + e1 * uOverW[1] + e2 * uOverW[2]) * invWRecip;
            const float v = (e0 * vOverW[0] + e1 * vOverW[1] + e2 * vOverW[2]) * invWRecip;

            const int tx = std::clamp(static_cast<int>(std::floor(u * static_cast<float>(texW))), 0, texW - 1);
            const int ty = std::clamp(static_cast<int>(std::floor((1.0f - v) * static_cast<float>(texH))), 0, texH - 1);

            const auto* srcRow = reinterpret_cast<const QRgb*>(texture.constScanLine(ty));
            const QRgb src = srcRow[tx];
            const int sa = qAlpha(src);
            if (sa < 26) {
                continue;
            }

            zBuffer[idx] = z;
            if (sa == 255) {
                dstRow[x] = src;
            } else {
                const QRgb dst = dstRow[x];
                const int invA = 255 - sa;
                const int r = (qRed(src) * sa + qRed(dst) * invA) / 255;
                const int g = (qGreen(src) * sa + qGreen(dst) * invA) / 255;
                const int b = (qBlue(src) * sa + qBlue(dst) * invA) / 255;
                dstRow[x] = qRgba(r, g, b, 255);
            }
        }
    }
}

void clipAndRasterize(const std::array<ClipVertex, 3>& tri, const QImage& texture, QImage& target, std::vector<float>& zBuffer)
{
    constexpr float nearW = 0.25f;
    std::array<ClipVertex, 4> clipped{};
    int count = 0;

    for (int i = 0; i < 3; ++i) {
        const ClipVertex& cur = tri[i];
        const ClipVertex& prev = tri[(i + 2) % 3];
        const bool curInside = cur.clip.w() >= nearW;
        const bool prevInside = prev.clip.w() >= nearW;

        if (curInside != prevInside) {
            const float t = (nearW - prev.clip.w()) / (cur.clip.w() - prev.clip.w());
            clipped[count++] = {
                prev.clip + (cur.clip - prev.clip) * t,
                prev.uv + (cur.uv - prev.uv) * t,
            };
        }
        if (curInside) {
            clipped[count++] = cur;
        }
    }

    for (int i = 1; i + 1 < count; ++i) {
        rasterizeTriangle({ clipped[0], clipped[i], clipped[i + 1] }, texture, target, zBuffer);
    }
}
}  // namespace

namespace opengl {
BoxGeometry::BoxGeometry(QVector3D size, QVector3D position) : m_size(size), m_position(position) {}

BoxGeometry::BoxGeometry(QVector3D size, QVector3D position, QPoint uv, QVector3D textureDim, QSize textureSize)
    : BoxGeometry(size, position)
{
    initGeometry(uv.x(), uv.y(), textureDim.x(), textureDim.y(), textureDim.z(), textureSize.width(), textureSize.height());
}

BoxGeometry::~BoxGeometry() = default;

void BoxGeometry::rasterize(const QMatrix4x4& vpMatrix, const QImage& texture, QImage& target, std::vector<float>& zBuffer) const
{
    if (m_vertices.size() < 4 || texture.isNull() || target.isNull()) {
        return;
    }

    const QMatrix4x4 mvp = vpMatrix * m_matrix;
    for (int base = 0; base + 3 < m_vertices.size(); base += 4) {
        const ClipVertex v0{ mvp * m_vertices[base].position, m_vertices[base].texCoord };
        const ClipVertex v1{ mvp * m_vertices[base + 1].position, m_vertices[base + 1].texCoord };
        const ClipVertex v2{ mvp * m_vertices[base + 2].position, m_vertices[base + 2].texCoord };
        const ClipVertex v3{ mvp * m_vertices[base + 3].position, m_vertices[base + 3].texCoord };

        clipAndRasterize({ v0, v1, v2 }, texture, target, zBuffer);
        clipAndRasterize({ v2, v1, v3 }, texture, target, zBuffer);
    }
}

void BoxGeometry::initGeometry(float u, float v, float width, float height, float depth, float textureWidth, float textureHeight)
{
    auto textureCord = getCubeUVs(u, v, width, height, depth, textureWidth, textureHeight);

    QMatrix4x4 transformation;
    transformation.translate(m_position);
    transformation.scale(m_size);
    auto positions = transformVectors(transformation, vertices);

    m_vertices.clear();
    m_vertices.reserve(positions.size());
    for (int i = 0; i < positions.size(); ++i) {
        m_vertices.append({ positions[i], textureCord[i] });
    }
}

void BoxGeometry::rotate(float angle, const QVector3D& vector)
{
    m_matrix.rotate(angle, vector);
}

BoxGeometry* BoxGeometry::Plane()
{
    auto b = new BoxGeometry(QVector3D(), QVector3D());
    b->m_vertices = {
        { QVector4D(-1.0f, -1.0f, -0.5f, 1.0f), QVector2D(0.0f, 0.0f) },
        { QVector4D(1.0f, -1.0f, -0.5f, 1.0f), QVector2D(1.0f, 0.0f) },
        { QVector4D(-1.0f, 1.0f, -0.5f, 1.0f), QVector2D(0.0f, 1.0f) },
        { QVector4D(1.0f, 1.0f, -0.5f, 1.0f), QVector2D(1.0f, 1.0f) },
    };
    return b;
}

void BoxGeometry::scale(const QVector3D& vector)
{
    m_matrix.scale(vector);
}
}  // namespace opengl
