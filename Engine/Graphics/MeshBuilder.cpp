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

void MeshBuilder::roundedBox(float cx, float cy, float cz, float hx, float hy, float hz, float radius, int segments)
{
    const float HALF_PI = 1.5707963f;
    segments = segments < 1 ? 1 : segments;
    radius = std::fmin(radius, std::fmin(hx, std::fmin(hy, hz)));
    const float h[3] = {hx, hy, hz};
    // Coordonnées d'une ligne de la grille le long d'un axe : arrondi (quart de cercle), plat, arrondi
    std::vector<float> lines[3];
    for (int a = 0; a < 3; ++a)
    {
        float in = h[a] - radius;
        for (int i = 0; i <= segments; ++i)
            lines[a].push_back(-in - radius * std::cos(HALF_PI * i / segments));
        for (int i = 0; i <= segments; ++i)
            lines[a].push_back(in + radius * std::sin(HALF_PI * i / segments));
    }
    // Chaque face du cube est une grille ; ses sommets sont ramenés sur la surface arrondie
    for (int axis = 0; axis < 3; ++axis)
        for (int side = -1; side <= 1; side += 2)
        {
            int u = (axis + 1) % 3, v = (axis + 2) % 3;
            const std::vector<float> &lu = lines[u], &lv = lines[v];
            unsigned int base = (unsigned int)verts.size();
            for (size_t j = 0; j < lv.size(); ++j)
                for (size_t i = 0; i < lu.size(); ++i)
                {
                    float p[3];
                    p[axis] = side * h[axis];
                    p[u] = lu[i];
                    p[v] = lv[j];
                    float inner[3], n[3], len = 0.0f;
                    for (int k = 0; k < 3; ++k)
                    {
                        float lim = h[k] - radius;
                        inner[k] = std::fmax(-lim, std::fmin(lim, p[k]));
                        n[k] = p[k] - inner[k];
                        len += n[k] * n[k];
                    }
                    len = std::sqrt(len);
                    if (len < 1e-7f)
                    {
                        n[0] = n[1] = n[2] = 0.0f;
                        n[axis] = (float)side;
                    }
                    else
                        for (float &k : n)
                            k /= len;
                    float pos[3];
                    for (int k = 0; k < 3; ++k)
                        pos[k] = radius > 0.0f ? inner[k] + n[k] * radius : p[k];
                    verts.push_back({pos[0] + cx, pos[1] + cy, pos[2] + cz, n[0], n[1], n[2],
                                     (p[u] / h[u] + 1.0f) * 0.5f, (p[v] / h[v] + 1.0f) * 0.5f});
                }
            unsigned int w = (unsigned int)lu.size();
            for (unsigned int j = 0; j + 1 < lv.size(); ++j)
                for (unsigned int i = 0; i + 1 < w; ++i)
                {
                    unsigned int a = base + j * w + i, b = a + 1, c2 = a + w + 1, d = a + w;
                    // u x v = +axis : sens direct sur la face positive, inversé sur la face négative
                    if (side > 0)
                        indices.insert(indices.end(), {a, b, c2, a, c2, d});
                    else
                        indices.insert(indices.end(), {a, c2, b, a, d, c2});
                }
        }
}

void MeshBuilder::lathe(const std::vector<float> &profile, int segments, float cx, float cy, float cz, float creaseDeg)
{
    const float TWO_PI = 6.28318531f;
    size_t n = profile.size() / 2;
    if (n < 2)
        return;
    segments = segments < 3 ? 3 : segments;
    // Normale (radiale, verticale) de chaque segment du profil
    std::vector<float> sr(n - 1), sy(n - 1);
    for (size_t i = 0; i + 1 < n; ++i)
    {
        float dr = profile[(i + 1) * 2] - profile[i * 2], dy = profile[(i + 1) * 2 + 1] - profile[i * 2 + 1];
        float len = std::sqrt(dr * dr + dy * dy);
        sr[i] = len > 1e-7f ? dy / len : 0.0f;
        sy[i] = len > 1e-7f ? -dr / len : 0.0f;
    }
    float crease = std::cos(creaseDeg * 3.14159265f / 180.0f);
    auto normalAt = [&](size_t seg, size_t point, float &nr, float &ny)
    {
        nr = sr[seg];
        ny = sy[seg];
        size_t other = point == seg ? seg - 1 : seg + 1; // segment voisin qui partage ce point
        if ((point == seg && seg == 0) || (point != seg && seg + 1 >= n - 1))
            return;
        if (sr[seg] * sr[other] + sy[seg] * sy[other] < crease)
            return;
        nr += sr[other];
        ny += sy[other];
        float len = std::sqrt(nr * nr + ny * ny);
        if (len > 1e-7f)
        {
            nr /= len;
            ny /= len;
        }
    };
    for (size_t s = 0; s + 1 < n; ++s)
    {
        unsigned int base = (unsigned int)verts.size();
        for (int e = 0; e < 2; ++e)
        {
            size_t pi = s + e;
            float r = profile[pi * 2], y = profile[pi * 2 + 1], nr, ny;
            normalAt(s, pi, nr, ny);
            for (int k = 0; k <= segments; ++k)
            {
                float a = TWO_PI * k / segments, ca = std::cos(a), sa = std::sin(a);
                verts.push_back({cx + ca * r, cy + y, cz + sa * r, ca * nr, ny, sa * nr, (float)k / segments, (float)pi / (n - 1)});
            }
        }
        unsigned int w = (unsigned int)segments + 1;
        for (unsigned int k = 0; k < (unsigned int)segments; ++k)
        {
            unsigned int a = base + k, b = base + k + 1, c2 = base + w + k + 1, d = base + w + k;
            indices.insert(indices.end(), {a, d, c2, a, c2, b});
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
