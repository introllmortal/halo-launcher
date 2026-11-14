// SPDX-License-Identifier: GPL-3.0-only

#include <QImage>
#include <QMatrix4x4>
#include <QTest>
#include <vector>

#include "ui/dialogs/skins/draw/BoxGeometry.h"
#include "ui/dialogs/skins/draw/Scene.h"

class SkinRasterizerTest : public QObject {
    Q_OBJECT

   private slots:
    void testOpaqueBoxRasterizesAndWritesDepth()
    {
        QImage tex(64, 64, QImage::Format_ARGB32);
        tex.fill(qRgba(200, 100, 50, 255));

        QImage target(64, 64, QImage::Format_ARGB32);
        target.fill(qRgba(0, 0, 0, 255));
        std::vector<float> zBuffer(64 * 64, 2.0f);

        opengl::BoxGeometry box(QVector3D(8, 8, 8), QVector3D(0, 0, 0), QPoint(0, 0), QVector3D(8, 8, 8));

        QMatrix4x4 proj;
        proj.perspective(45.0f, 1.0f, 1.0f, 500.0f);
        QMatrix4x4 view;
        view.lookAt(QVector3D(0, 0, 24), QVector3D(0, 0, 0), QVector3D(0, 1, 0));

        box.rasterize(proj * view, tex, target, zBuffer);

        const QRgb center = target.pixel(32, 32);
        QVERIFY(qRed(center) > 0);
        QVERIFY(zBuffer[32 * 64 + 32] < 2.0f);
    }

    void testSemiTransparentOverlayPreservesZBuffer()
    {
        QImage tex(64, 64, QImage::Format_ARGB32);
        tex.fill(qRgba(200, 200, 200, 128));

        QImage target(64, 64, QImage::Format_ARGB32);
        target.fill(qRgba(20, 20, 20, 255));
        std::vector<float> zBuffer(64 * 64, 2.0f);

        opengl::BoxGeometry box(QVector3D(8, 8, 8), QVector3D(0, 0, 0), QPoint(0, 0), QVector3D(8, 8, 8));

        QMatrix4x4 proj;
        proj.perspective(45.0f, 1.0f, 1.0f, 500.0f);
        QMatrix4x4 view;
        view.lookAt(QVector3D(0, 0, 24), QVector3D(0, 0, 0), QVector3D(0, 1, 0));

        box.rasterize(proj * view, tex, target, zBuffer);

        const QRgb center = target.pixel(32, 32);
        QVERIFY(qRed(center) > 20);
        QCOMPARE(zBuffer[32 * 64 + 32], 2.0f);
    }

    void testSceneDrawsClassicAndSlimWithCapeAndElytra()
    {
        QImage skin(64, 64, QImage::Format_ARGB32);
        skin.fill(qRgba(120, 180, 220, 255));
        QImage cape(64, 32, QImage::Format_ARGB32);
        cape.fill(qRgba(220, 60, 60, 255));

        opengl::Scene scene(skin, false, cape);
        scene.setCapeVisible(true);

        QImage target(128, 128, QImage::Format_ARGB32);
        target.fill(qRgba(10, 10, 10, 255));
        std::vector<float> zBuffer(128 * 128, 2.0f);

        QMatrix4x4 proj;
        proj.perspective(45.0f, 1.0f, 1.0f, 500.0f);
        QMatrix4x4 view;
        view.lookAt(QVector3D(0, -8, 48), QVector3D(0, -8, 0), QVector3D(0, 1, 0));

        scene.draw(proj * view, target, zBuffer);
        QVERIFY(zBuffer[64 * 128 + 64] < 2.0f);

        scene.setMode(true);
        scene.setElytraVisible(true);
        std::fill(zBuffer.begin(), zBuffer.end(), 2.0f);
        scene.draw(proj * view, target, zBuffer);
        QVERIFY(zBuffer[64 * 128 + 64] < 2.0f);
    }
};

QTEST_GUILESS_MAIN(SkinRasterizerTest)
#include "SkinRasterizer_test.moc"
