#include "ParticleSystem.h"
#include <cmath>
#include <algorithm>

void ParticleSystem::emit(const Particle &p)
{
    if ((int)particles.size() >= maxParticles)
    {
        auto oldest = std::max_element(particles.begin(), particles.end(), [](const Particle &a, const Particle &b)
                                       { return a.age / a.life < b.age / b.life; });
        *oldest = p;
        return;
    }
    particles.push_back(p);
}

void ParticleSystem::update(float dt, float windX, float windZ, float drag, float lift)
{
    float damp = std::exp(-drag * dt);
    for (auto &p : particles)
    {
        p.age += dt;
        p.vx = p.vx * damp + windX * (1.0f - damp);
        p.vz = p.vz * damp + windZ * (1.0f - damp);
        p.vy = p.vy * damp + lift * dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.z += p.vz * dt;
        p.rot += p.rotSpeed * dt;
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle &p)
                                   { return p.age >= p.life; }),
                    particles.end());
}

int ParticleSystem::buildBillboards(const float cam[3], std::vector<float> &out) const
{
    out.clear();
    if (particles.empty())
        return 0;

    std::vector<std::pair<float, int>> order;
    order.reserve(particles.size());
    for (int i = 0; i < (int)particles.size(); ++i)
    {
        const Particle &p = particles[i];
        float dx = p.x - cam[0], dy = p.y - cam[1], dz = p.z - cam[2];
        order.push_back({dx * dx + dy * dy + dz * dz, i});
    }
    std::sort(order.begin(), order.end(), [](const std::pair<float, int> &a, const std::pair<float, int> &b)
              { return a.first > b.first; });

    out.reserve(order.size() * FLOATS_PER_BILLBOARD);
    for (const auto &o : order)
    {
        const Particle &p = particles[o.second];
        float t = p.age / p.life;
        float size = p.size0 + (p.size1 - p.size0) * (1.0f - (1.0f - t) * (1.0f - t));
        float fadeIn = std::min(1.0f, t / 0.08f);
        float alpha = p.alpha * fadeIn * std::pow(1.0f - t, 1.5f);
        out.insert(out.end(), {p.x, p.y, p.z, size, p.r, p.g, p.b, alpha, p.rot, p.seed});
    }
    return (int)order.size();
}
