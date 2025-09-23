#pragma once

struct AABB
{
    float minX, maxX;
    float minY, maxY;
    float minZ, maxZ;

    bool intersects(float x, float y, float z, float radiusHorizontal = 0.2f, float radiusVertical = 0.5f) const
    {
        return (x + radiusHorizontal > minX && x - radiusHorizontal < maxX) &&
               (y + radiusVertical > minY && y - radiusVertical < maxY) &&
               (z + radiusHorizontal > minZ && z - radiusHorizontal < maxZ);
    }
};
