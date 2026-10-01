#pragma once
#ifndef __PARTICLE_SYSTEM__
#define __PARTICLE_SYSTEM__
#include <vector>

// Particule billboard (fumée, poussière, vapeur...) simulée sur le CPU
struct Particle
{
    float x, y, z;
    float vx, vy, vz;
    float age, life;      // secondes
    float size0, size1;   // taille à la naissance et en fin de vie
    float rot, rotSpeed;  // rotation dans le plan de l'écran
    float r, g, b;
    float alpha;          // opacité maximale
    float seed;           // variation par particule pour le shader (0..1)
};

// Réserve de particules à taille bornée : quand elle est pleine, la particule la plus avancée
// dans sa vie est remplacée. Le rendu (shader et buffer d'instances) reste à la charge du jeu :
// buildBillboards() prépare les données triées de l'arrière vers l'avant.
class ParticleSystem
{
public:
    // Données par instance produites par buildBillboards()
    static const int FLOATS_PER_BILLBOARD = 10; // x, y, z, taille, r, g, b, alpha, rotation, seed

    explicit ParticleSystem(int maxParticles = 2000) : maxParticles(maxParticles) {}

    void emit(const Particle &p);
    // drag : amortissement (1/s) vers la vitesse du vent ; lift : accélération verticale (m/s²)
    void update(float dt, float windX, float windZ, float drag = 1.6f, float lift = 0.35f);
    void clear() { particles.clear(); }

    int getMaxParticles() const { return maxParticles; }
    const std::vector<Particle> &getParticles() const { return particles; }

    // Remplit out (FLOATS_PER_BILLBOARD par particule) trié du plus loin au plus proche de la caméra,
    // avec la taille qui grandit et l'opacité qui s'estompe au fil de la vie. Renvoie le nombre d'instances.
    int buildBillboards(const float cameraPos[3], std::vector<float> &out) const;

private:
    int maxParticles;
    std::vector<Particle> particles;
};

#endif
