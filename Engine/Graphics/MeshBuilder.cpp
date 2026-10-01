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

void MeshBuilder::cone(float cx, float baseY, float cz, float radius, float height, int segments)
{
    const float TWO_PI = 6.28318531f;
    segments = segments < 3 ? 3 : segments;
    // Normale des flancs : inclinée vers le haut selon la pente du cône
    float slope = radius / std::sqrt(radius * radius + height * height);
    float flat = height / std::sqrt(radius * radius + height * height);
    for (int i = 0; i < segments; ++i)
    {
        float a0 = TWO_PI * i / segments, a1 = TWO_PI * (i + 1) / segments, am = (a0 + a1) * 0.5f;
        unsigned int base = (unsigned int)verts.size();
        verts.push_back({cx + std::cos(a0) * radius, baseY, cz + std::sin(a0) * radius,
                         std::cos(a0) * flat, slope, std::sin(a0) * flat, 0.0f, 0.0f});
        verts.push_back({cx, baseY + height, cz, std::cos(am) * flat, slope, std::sin(am) * flat, 0.5f, 1.0f});
        verts.push_back({cx + std::cos(a1) * radius, baseY, cz + std::sin(a1) * radius,
                         std::cos(a1) * flat, slope, std::sin(a1) * flat, 1.0f, 0.0f});
        indices.insert(indices.end(), {base, base + 1, base + 2});
    }
}

void MeshBuilder::box(float cx, float cy, float cz, float hx, float hy, float hz)
{
    // Six faces : pour chaque axe, une face de chaque côté
    for (int axis = 0; axis < 3; ++axis)
        for (int side = -1; side <= 1; side += 2)
        {
            const float h[3] = {hx, hy, hz};
            int u = (axis + 1) % 3, v = (axis + 2) % 3;
            float n[3] = {0.0f, 0.0f, 0.0f};
            n[axis] = (float)side;
            float corners[4][3];
            const float su[4] = {-1, 1, 1, -1}, sv[4] = {-1, -1, 1, 1};
            for (int k = 0; k < 4; ++k)
            {
                // Ordre inversé sur la face négative pour garder le sens direct vu de l'extérieur
                int kk = side > 0 ? k : 3 - k;
                corners[k][axis] = side * h[axis];
                corners[k][u] = su[kk] * h[u];
                corners[k][v] = sv[kk] * h[v];
                corners[k][0] += cx;
                corners[k][1] += cy;
                corners[k][2] += cz;
            }
            quad(corners[0], corners[1], corners[2], corners[3], n[0], n[1], n[2]);
        }
}

void MeshBuilder::sphere(float cx, float cy, float cz, float rx, float ry, float rz, int rings, int segments)
{
    const float PI = 3.14159265f;
    rings = rings < 2 ? 2 : rings;
    segments = segments < 3 ? 3 : segments;
    unsigned int base = (unsigned int)verts.size();
    for (int i = 0; i <= rings; ++i)
    {
        float phi = PI * i / rings;
        float sp = std::sin(phi), cp = std::cos(phi);
        for (int j = 0; j <= segments; ++j)
        {
            float th = 2.0f * PI * j / segments;
            float nx = sp * std::cos(th), ny = cp, nz = sp * std::sin(th);
            // Normale d'ellipsoïde : divisée par les rayons
            float gx = nx / rx, gy = ny / ry, gz = nz / rz;
            float len = std::sqrt(gx * gx + gy * gy + gz * gz);
            verts.push_back({cx + nx * rx, cy + ny * ry, cz + nz * rz, gx / len, gy / len, gz / len,
                             (float)j / segments, (float)i / rings});
        }
    }
    for (int i = 0; i < rings; ++i)
        for (int j = 0; j < segments; ++j)
        {
            unsigned int a = base + i * (segments + 1) + j, b = a + segments + 1;
            indices.insert(indices.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
}

void MeshBuilder::torus(float majorRadius, float minorRadius, int segments, int sides)
{
    const float TWO_PI = 6.28318531f;
    segments = segments < 3 ? 3 : segments;
    sides = sides < 3 ? 3 : sides;
    unsigned int base = (unsigned int)verts.size();
    for (int i = 0; i <= segments; ++i)
    {
        float a = TWO_PI * i / segments, ca = std::cos(a), sa = std::sin(a);
        for (int j = 0; j <= sides; ++j)
        {
            float b = TWO_PI * j / sides, cb = std::cos(b), sb = std::sin(b);
            float r = majorRadius + minorRadius * cb;
            verts.push_back({ca * r, sa * r, minorRadius * sb, ca * cb, sa * cb, sb,
                             (float)i / segments, (float)j / sides});
        }
    }
    for (int i = 0; i < segments; ++i)
        for (int j = 0; j < sides; ++j)
        {
            unsigned int a = base + i * (sides + 1) + j, b = a + sides + 1;
            indices.insert(indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
}

void MeshBuilder::tube(const float a[3], const float b[3], float radius, int sides, bool caps)
{
    const float TWO_PI = 6.28318531f;
    sides = sides < 3 ? 3 : sides;
    float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len < 1e-6f)
        return;
    for (float &c : d)
        c /= len;
    // Deux axes perpendiculaires au tube
    float ref[3] = {0.0f, 1.0f, 0.0f};
    if (std::fabs(d[1]) > 0.9f)
    {
        ref[0] = 1.0f;
        ref[1] = 0.0f;
    }
    float u[3] = {d[1] * ref[2] - d[2] * ref[1], d[2] * ref[0] - d[0] * ref[2], d[0] * ref[1] - d[1] * ref[0]};
    float ul = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
    for (float &c : u)
        c /= ul;
    float v[3] = {d[1] * u[2] - d[2] * u[1], d[2] * u[0] - d[0] * u[2], d[0] * u[1] - d[1] * u[0]};

    unsigned int base = (unsigned int)verts.size();
    for (int i = 0; i <= sides; ++i)
    {
        float t = TWO_PI * i / sides, ct = std::cos(t), st = std::sin(t);
        float n[3] = {u[0] * ct + v[0] * st, u[1] * ct + v[1] * st, u[2] * ct + v[2] * st};
        for (int end = 0; end < 2; ++end)
        {
            const float *p = end == 0 ? a : b;
            verts.push_back({p[0] + n[0] * radius, p[1] + n[1] * radius, p[2] + n[2] * radius, n[0], n[1], n[2],
                             (float)i / sides, (float)end});
        }
    }
    for (int i = 0; i < sides; ++i)
    {
        unsigned int k = base + i * 2;
        indices.insert(indices.end(), {k, k + 2, k + 1, k + 1, k + 2, k + 3});
    }
    if (!caps)
        return;
    for (int end = 0; end < 2; ++end)
    {
        const float *p = end == 0 ? a : b;
        float s = end == 0 ? -1.0f : 1.0f;
        unsigned int center = (unsigned int)verts.size();
        verts.push_back({p[0], p[1], p[2], d[0] * s, d[1] * s, d[2] * s, 0.5f, 0.5f});
        for (int i = 0; i <= sides; ++i)
        {
            float t = TWO_PI * i / sides, ct = std::cos(t), st = std::sin(t);
            float n[3] = {u[0] * ct + v[0] * st, u[1] * ct + v[1] * st, u[2] * ct + v[2] * st};
            verts.push_back({p[0] + n[0] * radius, p[1] + n[1] * radius, p[2] + n[2] * radius, d[0] * s, d[1] * s, d[2] * s,
                             0.5f + ct * 0.5f, 0.5f + st * 0.5f});
        }
        for (int i = 0; i < sides; ++i)
        {
            if (end == 0)
                indices.insert(indices.end(), {center, center + 2 + i, center + 1 + i});
            else
                indices.insert(indices.end(), {center, center + 1 + i, center + 2 + i});
        }
    }
}

void MeshBuilder::uploadTo(Model &m, float r, float g, float b) const
{
    m.createFromData(verts, indices);
    m.setPosition(0.0f, 0.0f, 0.0f);
    m.setScale(1.0f, 1.0f, 1.0f);
    m.setRotation(0.0f, 0.0f, 0.0f);
    m.setColorRGBA(r, g, b, 1.0f);
}
