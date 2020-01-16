#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include "simulateclothmeshes.h"
#include "positionkey.h"
#include "util.h"
#include "clothsimulator.h"

class ClothMeshesSimulator
{
public:
    ClothMeshesSimulator(std::vector<ClothMesh> *clothMeshes,
            const std::vector<QVector3D> *clothCollisionVertices,
            const std::vector<std::vector<size_t>> *clothCollisionTriangles,
            const std::vector<std::pair<QVector3D, float>> *clothTargetNodes) :
        m_clothMeshes(clothMeshes),
        m_clothCollisionVertices(clothCollisionVertices),
        m_clothCollisionTriangles(clothCollisionTriangles),
        m_clothTargetNodes(clothTargetNodes)
    {
    }
    void simulate(ClothMesh *clothMesh) const
    {
        const auto &filteredClothFaces = clothMesh->faces;
        std::map<PositionKey, std::pair<QUuid, QUuid>> positionMap;
        std::pair<QUuid, QUuid> defaultSource;
        for (const auto &it: *clothMesh->outcomeNodeVertices) {
            if (!it.second.first.isNull())
                defaultSource.first = it.second.first;
            positionMap.insert({PositionKey(it.first), it.second});
        }
        clothMesh->vertexSources.resize(clothMesh->vertices.size(), defaultSource);
        for (size_t i = 0; i < clothMesh->vertices.size(); ++i) {
            auto findSource = positionMap.find(PositionKey(clothMesh->vertices[i]));
            if (findSource == positionMap.end())
                continue;
            clothMesh->vertexSources[i] = findSource->second;
        }
        
        std::vector<QVector3D> &filteredClothVertices = clothMesh->vertices;
        std::vector<QVector3D> externalForces;
        const auto &clothForce = clothMesh->clothForce;
        float clothOffset = 0.015f + (clothMesh->clothOffset * 0.05f);
        if (ClothForce::Centripetal == clothForce) {
            externalForces.resize(filteredClothVertices.size());
            for (size_t i = 0; i < filteredClothFaces.size(); ++i) {
                const auto &face = filteredClothFaces[i];
                auto faceForceDirection = -polygonNormal(filteredClothVertices, face);
                for (const auto &vertex: face)
                    externalForces[vertex] += faceForceDirection;
            }
            for (auto &it: externalForces)
                it = (it.normalized() + QVector3D(0.0f, -1.0f, 0.0f)).normalized();
        } else {
            externalForces.resize(filteredClothVertices.size(), QVector3D(0.0f, -1.0f, 0.0f));
        }
        ClothSimulator clothSimulator(filteredClothVertices,
            filteredClothFaces,
            *m_clothCollisionVertices,
            *m_clothCollisionTriangles,
            externalForces);
        clothSimulator.setStiffness(clothMesh->clothStiffness);
        clothSimulator.create();
        for (size_t i = 0; i < 350; ++i)
            clothSimulator.step();
        clothSimulator.getCurrentVertices(&filteredClothVertices);
        for (size_t i = 0; i < filteredClothVertices.size(); ++i) {
            filteredClothVertices[i] -= externalForces[i] * clothOffset;
        }
    }
    void operator()(const tbb::blocked_range<size_t> &range) const
    {
        for (size_t i = range.begin(); i != range.end(); ++i) {
            simulate(&(*m_clothMeshes)[i]);
        }
    }
private:
    std::vector<ClothMesh> *m_clothMeshes = nullptr;
    const std::vector<QVector3D> *m_clothCollisionVertices = nullptr;
    const std::vector<std::vector<size_t>> *m_clothCollisionTriangles = nullptr;
    const std::vector<std::pair<QVector3D, float>> *m_clothTargetNodes = nullptr;
};

void simulateClothMeshes(std::vector<ClothMesh> *clothMeshes,
    const std::vector<QVector3D> *clothCollisionVertices,
    const std::vector<std::vector<size_t>> *clothCollisionTriangles,
    const std::vector<std::pair<QVector3D, float>> *clothTargetNodes)
{
    tbb::parallel_for(tbb::blocked_range<size_t>(0, clothMeshes->size()),
        ClothMeshesSimulator(clothMeshes,
            clothCollisionVertices,
            clothCollisionTriangles,
            clothTargetNodes));
}