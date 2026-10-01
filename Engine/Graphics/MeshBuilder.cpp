#include "MeshBuilder.h"
#include <cmath>

void MeshBuilder::clear()
{
    verts.clear();
    indices.clear();
}

void MeshBuilder::quad(const float a[3], const float b[3], const float c[3], const float d[3], float nx, float ny, float nz)
{
    unsigned int base = (unsigned int)verts.size();
    const float *p[4] = {a, b, c, d};
    for (int i = 0; i < 4; ++i)
        verts.push_back({p[i][0], p[i][1], p[i][2], nx, ny, nz, 0.0f, 0.0f});
    indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

void MeshBuilder::addTransformed(const Model::Mesh &src, float px, float py, float pz,
                                 float sx, float sy, float sz, float rotXDeg, float rotYDeg)
{
    const float DEG = 3.14159265f / 180.0f;
    unsigned int base = (unsigned int)verts.size();
    float ax = rotXDeg * DEG, ay = rotYDeg * DEG;
    float cx = std::cos(ax), snx = std::sin(ax), cy = std::cos(ay), sny = std::sin(ay);
    auto rotate = [&](float &x, float &y, float &z)
    {
        float y1 = y * cx - z * snx;
        float z1 = y * snx + z * cx;
        float x2 = x * cy + z1 * sny;
        float z2 = -x * sny + z1 * cy;
        x = x2;
        y = y1;
        z = z2;
    };
    for (const auto &v : src.verts)
    {
        float x = v.x * sx, y = v.y * sy, z = v.z * sz;
        rotate(x, y, z);
        // Normales : inverse de l'échelle pour rester perpendiculaires aux surfaces
        float nx = v.nx / sx, ny = v.ny / sy, nz = v.nz / sz;
        rotate(nx, ny, nz);
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 1e-6f)
        {
            nx /= len;
            ny /= len;
            nz /= len;
        }
        verts.push_back({x + px, y + py, z + pz, nx, ny, nz, v.u, v.v});
    }
    for (unsigned int i : src.indices)
        indices.push_back(base + i);
}

void MeshBuilder::segmentBox(float ax, float az, float bx, float bz, float halfThick, float height)
{
    float dx = bx - ax, dz = bz - az;
    float len = std::sqrt(dx * dx + dz * dz);
    if (len < 1e-4f)
        return;
    float px = -dz / len * halfThick, pz = dx / len * halfThick;
    float a0[3] = {ax + px, 0.0f, az + pz}, a1[3] = {ax - px, 0.0f, az - pz};
    float b0[3] = {bx + px, 0.0f, bz + pz}, b1[3] = {bx - px, 0.0f, bz - pz};
    float a0t[3] = {a0[0], height, a0[2]}, a1t[3] = {a1[0], height, a1[2]};
    float b0t[3] = {b0[0], height, b0[2]}, b1t[3] = {b1[0], height, b1[2]};
    float nx = px / halfThick, nz = pz / halfThick;
    quad(a0, b0, b0t, a0t, nx, 0.0f, nz);
    quad(b1, a1, a1t, b1t, -nx, 0.0f, -nz);
    quad(a0t, b0t, b1t, a1t, 0.0f, 1.0f, 0.0f);
    float ex = dx / len, ez = dz / len;
    quad(b0, b1, b1t, b0t, ex, 0.0f, ez);
    quad(a1, a0, a0t, a1t, -ex, 0.0f, -ez);
}

void MeshBuilder::uploadTo(Model &m, float r, float g, float b) const
{
    m.createFromData(verts, indices);
    m.setPosition(0.0f, 0.0f, 0.0f);
    m.setScale(1.0f, 1.0f, 1.0f);
    m.setRotation(0.0f, 0.0f, 0.0f);
    m.setColorRGBA(r, g, b, 1.0f);
}
