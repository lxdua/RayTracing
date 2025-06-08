#pragma once

#include <glm/glm.hpp>
#include <vector>

struct Material
{
    glm::vec3 albedo = { 0.8f, 0.8f, 0.8f };
    float metallic = 0.0f;              // 金属度 (0-1)
    float roughness = 0.5f;             // 粗糙度 (0-1)
    glm::vec3 emissionColor{ 0.0f };    // 自发光
    float emissionPower = 0.0f;         // 自发光

    glm::vec3 GetEmission() const { return emissionColor * emissionPower; }

    glm::vec3 FresnelSchlick(float cosTheta) const
    {
        float minReflectance = 0.04f;
        glm::vec3 reflectance = albedo * metallic +
            glm::vec3(minReflectance) * (1.0f - metallic);
        float powerTerm = glm::pow(glm::clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
        return reflectance + (glm::vec3(1.0f) - reflectance) * powerTerm;
    }

};

struct DirectionalLight
{
    glm::vec3 direction = glm::normalize(glm::vec3(-1, -1, -1));
    glm::vec3 color = glm::vec3(1.0f, 0.95f, 0.9f);
    float intensity = 1.5f;
};

struct PointLight
{
    glm::vec3 position = glm::vec3(0.0f, 5.0f, 0.0f);
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
    float range = 10.0f;
};

struct Sphere
{
    glm::vec3 position{ 0.0f,0.0f,0.0f };
    float radius = 0.5f;

    uint32_t materialID = 0;
};

struct Scene
{
    std::vector<Material> materials;
    std::vector<Sphere> spheres;

    DirectionalLight directionalLight;
    std::vector<PointLight> pointLights;

    uint32_t AddMaterial(const Material& material)
    {
        materials.push_back(material);
        return static_cast<uint32_t>(materials.size() - 1);
    }

    Material& GetMaterial(uint32_t materialID)
    {
        return materials[materialID];
    }

    void AddPointLight(
        const glm::vec3& position,
        const glm::vec3& color = glm::vec3(1.0f),
        float intensity = 1.0f,
        float range = 10.0f)
    {
        pointLights.push_back({ position,color,intensity,range });
    }

    void AddSphere(const Sphere& sphere)
    {
        spheres.push_back(sphere);
    }
};