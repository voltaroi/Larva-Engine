#include "Collision2D.h"
#include <cmath>
#include <algorithm>

bool Collision2D::pushCircleFromSegment(float &x, float &z, float radius, const Segment &w, float segRadius, float &nx, float &nz)
{
    float dx = w.bx - w.ax, dz = w.bz - w.az;
    float len2 = dx * dx + dz * dz;
    float t = len2 > 1e-8f ? ((x - w.ax) * dx + (z - w.az) * dz) / len2 : 0.0f;
    t = std::max(0.0f, std::min(1.0f, t));
    float cx = w.ax + dx * t, cz = w.az + dz * t;
    float ox = x - cx, oz = z - cz;
    float d = std::sqrt(ox * ox + oz * oz);
    float minD = radius + segRadius;
    if (d >= minD || d <= 1e-5f)
        return false;
    nx = ox / d;
    nz = oz / d;
    x += nx * (minD - d);
    z += nz * (minD - d);
    return true;
}

bool Collision2D::pushCircleFromBox(float &x, float &z, float radius, const OrientedBox &b, float &nx, float &nz)
{
    float c = std::cos(b.rotY), s = std::sin(b.rotY);
    float wx = x - b.x, wz = z - b.z;
    // Monde -> local (inverse de Ry)
    float lx = wx * c - wz * s;
    float lz = wx * s + wz * c;
    if (std::fabs(lx) > b.hx + radius || std::fabs(lz) > b.hz + radius)
        return false;

    float cx = std::max(-b.hx, std::min(lx, b.hx));
    float cz = std::max(-b.hz, std::min(lz, b.hz));
    float ox = lx - cx, oz = lz - cz;
    float d = std::sqrt(ox * ox + oz * oz);
    float nlx, nlz, pen;
    if (d > 1e-5f)
    {
        if (d >= radius)
            return false;
        nlx = ox / d;
        nlz = oz / d;
        pen = radius - d;
    }
    else
    {
        // Centre à l'intérieur : sortie par la face la plus proche
        float px = b.hx - std::fabs(lx), pz = b.hz - std::fabs(lz);
        if (px < pz)
        {
            nlx = lx > 0.0f ? 1.0f : -1.0f;
            nlz = 0.0f;
            pen = px + radius;
        }
        else
        {
            nlx = 0.0f;
            nlz = lz > 0.0f ? 1.0f : -1.0f;
            pen = pz + radius;
        }
    }
    // Local -> monde (Ry)
    nx = nlx * c + nlz * s;
    nz = -nlx * s + nlz * c;
    x += nx * pen;
    z += nz * pen;
    return true;
}

void Collision2D::SegmentGrid::reset(float gridOrigin, float extent, int cellCount)
{
    origin = gridOrigin;
    cells = std::max(1, cellCount);
    cellSize = extent / cells;
    grid.assign(cells * cells, std::vector<int>());
}

void Collision2D::SegmentGrid::insert(int index, const Segment &w, float margin)
{
    float minX = std::min(w.ax, w.bx) - margin, maxX = std::max(w.ax, w.bx) + margin;
    float minZ = std::min(w.az, w.bz) - margin, maxZ = std::max(w.az, w.bz) + margin;
    int x0 = std::max(0, (int)((minX - origin) / cellSize)), x1 = std::min(cells - 1, (int)((maxX - origin) / cellSize));
    int z0 = std::max(0, (int)((minZ - origin) / cellSize)), z1 = std::min(cells - 1, (int)((maxZ - origin) / cellSize));
    for (int gx = x0; gx <= x1; ++gx)
        for (int gz = z0; gz <= z1; ++gz)
            grid[gx * cells + gz].push_back(index);
}

const std::vector<int> &Collision2D::SegmentGrid::query(float x, float z) const
{
    if (cells == 0)
        return empty;
    int gx = (int)((x - origin) / cellSize), gz = (int)((z - origin) / cellSize);
    if (gx < 0 || gx >= cells || gz < 0 || gz >= cells)
        return empty;
    return grid[gx * cells + gz];
}
