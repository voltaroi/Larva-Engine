#pragma once
#ifndef __SPLINE__
#define __SPLINE__
#include <vector>

// Splines 2D (plan XZ) : Catmull-Rom centripète, qui ne forme pas de boucle
// même quand les points de contrôle sont irrégulièrement espacés.
namespace Spline
{
    struct Point
    {
        float x, z;
    };

    // Point entre p1 et p2 (t dans [0, 1]), p0 et p3 servent de tangentes
    Point catmullRom(Point p0, Point p1, Point p2, Point p3, float t);

    // Courbe fermée passant par tous les points : perSegment échantillons entre deux points de contrôle
    std::vector<Point> sampleClosed(const std::vector<Point> &controlPoints, int perSegment);

    // Courbe ouverte du premier au dernier point (tangentes des extrémités prolongées), dernier point inclus
    std::vector<Point> sampleOpen(const std::vector<Point> &controlPoints, int perSegment);

    // Longueur d'une polyligne fermée
    float closedLength(const std::vector<Point> &polyline);

    // Rééchantillonne une polyligne fermée à pas constant : le pas demandé est ajusté pour tomber juste
    // (actualStep). totalLength reçoit la longueur de la boucle.
    std::vector<Point> resampleClosed(const std::vector<Point> &polyline, float step,
                                      float *totalLength = nullptr, float *actualStep = nullptr);
}

#endif
