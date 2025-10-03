#include "Model.h"

void Model::draw() {
    // Dessiner le mod�le
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
    Model model;  // Le mod�le � retourner

    // V�rification de l'existence du fichier
    if (!fileExists(path)) {
        std::cerr << "Erreur: Fichier non trouv�: " << path << std::endl;
        return model;  // Retourner un mod�le vide si le fichier n'est pas trouv�
    }
    
    Assimp::Importer importer;

    // Tentative de chargement du mod�le avec Assimp
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

    if (!scene) {
        std::cerr << "Erreur de chargement du fichier: " << importer.GetErrorString() << std::endl;
        return model;  // Retourner un mod�le vide si le chargement �choue
    }

    // V�rification d'incompl�tude de la sc�ne ou d'un probl�me avec le noeud racine
    if (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Erreur: La sc�ne est incompl�te ou sans noeud racine." << std::endl;
        return model;  // Retourner un mod�le vide si la sc�ne est corrompue
    }

    std::cout << "Mod�le charg� avec succ�s: " << path << std::endl;
    std::cout << "Nombre de meshes dans la sc�ne: " << scene->mNumMeshes << std::endl;

    // Si aucune mesh n'est pr�sente, ne rien faire
    if (scene->mNumMeshes == 0) {
        std::cerr << "Aucune mesh trouv�e dans le fichier." << std::endl;
        return model;
    }

    // Tentative de chargement de la premi�re mesh
    aiMesh* mesh = scene->mMeshes[0];  // Prendre seulement la premi�re mesh
    if (!mesh) {
        std::cerr << "Erreur: Impossible de r�cup�rer la mesh du mod�le." << std::endl;
        return model;  // Retourner un mod�le vide si la mesh est invalide
    }

    model.mesh = mesh;

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // Chargement des vertices
    for (unsigned int j = 0; j < mesh->mNumVertices; ++j) {
        vertices.push_back(mesh->mVertices[j].x);
        vertices.push_back(mesh->mVertices[j].y);
        vertices.push_back(mesh->mVertices[j].z);

        // V�rification des normales
        if (mesh->HasNormals()) {
            vertices.push_back(mesh->mNormals[j].x);
            vertices.push_back(mesh->mNormals[j].y);
            vertices.push_back(mesh->mNormals[j].z);
        }

        // V�rification des coordonn�es de texture
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

    // V�rification si des indices ont �t� trouv�s
    if (indices.empty()) {
        std::cerr << "Aucun indice de face trouv� dans la mesh." << std::endl;
    }

    // G�n�ration du VAO, VBO et EBO
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
        std::cerr << "Aucun indice � charger pour le mod�le." << std::endl;
    }

    // Param�trage des attributs de vertex (positions, normales, texture coords)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normales (si pr�sentes)
    if (mesh->HasNormals()) {
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    // Coordonn�es de texture (si pr�sentes)
    if (mesh->HasTextureCoords(0)) {
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
    }

    glBindVertexArray(0);

    std::cout << "Mod�le charg� avec succ�s et pr�t � �tre rendu." << std::endl;
    return model;  // Retourner le mod�le charg�
}
