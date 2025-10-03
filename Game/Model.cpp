#include "Model.h"

void Model::draw() {
    // Dessiner le modèle
    if (VAO != 0) {
        glBindVertexArray(VAO);
        if (EBO != 0) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glDrawElements(GL_TRIANGLES, mesh->mNumFaces * 3, GL_UNSIGNED_INT, 0);
        }
        else {
            glDrawArrays(GL_TRIANGLES, 0, mesh->mNumVertices);
        }
        glBindVertexArray(0);
    }
}

bool Model::fileExists(const std::string& filepath) {
    std::ifstream file(filepath);
    return file.good();
}

Model Model::LoadModel(const std::string& path) {
    Model model;  // Le modèle à retourner

    // Vérification de l'existence du fichier
    if (!fileExists(path)) {
        std::cerr << "Erreur: Fichier non trouvé: " << path << std::endl;
        return model;  // Retourner un modèle vide si le fichier n'est pas trouvé
    }
    
    Assimp::Importer importer;

    // Tentative de chargement du modèle avec Assimp
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

    if (!scene) {
        std::cerr << "Erreur de chargement du fichier: " << importer.GetErrorString() << std::endl;
        return model;  // Retourner un modèle vide si le chargement échoue
    }

    // Vérification d'incomplétude de la scène ou d'un problème avec le noeud racine
    if (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Erreur: La scène est incomplète ou sans noeud racine." << std::endl;
        return model;  // Retourner un modèle vide si la scène est corrompue
    }

    std::cout << "Modèle chargé avec succès: " << path << std::endl;
    std::cout << "Nombre de meshes dans la scène: " << scene->mNumMeshes << std::endl;

    // Si aucune mesh n'est présente, ne rien faire
    if (scene->mNumMeshes == 0) {
        std::cerr << "Aucune mesh trouvée dans le fichier." << std::endl;
        return model;
    }

    // Tentative de chargement de la première mesh
    aiMesh* mesh = scene->mMeshes[0];  // Prendre seulement la première mesh
    if (!mesh) {
        std::cerr << "Erreur: Impossible de récupérer la mesh du modèle." << std::endl;
        return model;  // Retourner un modèle vide si la mesh est invalide
    }

    model.mesh = mesh;

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // Chargement des vertices
    for (unsigned int j = 0; j < mesh->mNumVertices; ++j) {
        vertices.push_back(mesh->mVertices[j].x);
        vertices.push_back(mesh->mVertices[j].y);
        vertices.push_back(mesh->mVertices[j].z);

        // Vérification des normales
        if (mesh->HasNormals()) {
            vertices.push_back(mesh->mNormals[j].x);
            vertices.push_back(mesh->mNormals[j].y);
            vertices.push_back(mesh->mNormals[j].z);
        }

        // Vérification des coordonnées de texture
        if (mesh->HasTextureCoords(0)) {
            vertices.push_back(mesh->mTextureCoords[0][j].x);
            vertices.push_back(mesh->mTextureCoords[0][j].y);
        }
    }

    // Chargement des indices
    for (unsigned int j = 0; j < mesh->mNumFaces; ++j) {
        aiFace face = mesh->mFaces[j];
        for (unsigned int k = 0; k < face.mNumIndices; ++k) {
            indices.push_back(face.mIndices[k]);
        }
    }

    // Vérification si des indices ont été trouvés
    if (indices.empty()) {
        std::cerr << "Aucun indice de face trouvé dans la mesh." << std::endl;
    }

    // Génération du VAO, VBO et EBO
    glGenVertexArrays(1, &model.VAO);
    glBindVertexArray(model.VAO);
    
    glGenBuffers(1, &model.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, model.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

    if (!indices.empty()) {
        glGenBuffers(1, &model.EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
    }
    else {
        std::cerr << "Aucun indice à charger pour le modèle." << std::endl;
    }

    // Paramétrage des attributs de vertex (positions, normales, texture coords)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normales (si présentes)
    if (mesh->HasNormals()) {
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    // Coordonnées de texture (si présentes)
    if (mesh->HasTextureCoords(0)) {
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
    }

    glBindVertexArray(0);

    std::cout << "Modèle chargé avec succès et prêt à être rendu." << std::endl;
    return model;  // Retourner le modèle chargé
}
