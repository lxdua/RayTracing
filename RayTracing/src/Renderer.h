#pragma once

#include "Walnut/Image.h"

#include "Camera.h"
#include "Scene.h"

#include <memory>
#include <glm/glm.hpp>

struct Ray
{
	glm::vec3 origin;
	glm::vec3 direction;
	Ray() = default;
	Ray(glm::vec3 _origin, glm::vec3 _direction) { origin = _origin; direction = _direction; }
};

struct HitInfo
{
	bool didHit = false;
	float dist = FLT_MAX;
	glm::vec3 hitPoint;
	glm::vec3 normal;
	uint32_t materialID = 0;
};

class Renderer
{
public:
	Renderer() = default;

	void Render(Scene& scene, Camera& camera);
	void OnResize(uint32_t width, uint32_t height);

	std::shared_ptr<Walnut::Image> GetFinalImage() const { return m_FinalImage; }

	HitInfo RaySphere(Ray ray, Sphere sphere);

private:
	glm::vec4 PerPixel(uint32_t x, uint32_t y);
	glm::vec3 TraceRay(Scene& scene, Ray ray);
	glm::vec3 TraceRayOnce(Scene& scene, Ray ray, uint32_t dep = 0);
	glm::vec3 Renderer::CalculateDirectLight(Scene& scene, HitInfo& hit, Ray& ray);
	HitInfo CalculateRayCollision(Scene& scene, Ray ray);

	glm::vec3 GetSkyLight(Ray ray);

public:
	uint32_t m_NumRays = 2;
	uint32_t m_MaxBounceCount = 2;
	bool m_JustDiffuse = false;

private:
	Scene* m_Scene = nullptr;
	Camera* m_Camera = nullptr;

	uint32_t m_FrameCount = 0;

	std::shared_ptr<Walnut::Image> m_FinalImage;
	uint32_t* m_ImageData = nullptr;

	std::vector<uint32_t> m_ImageHorizontalIter, m_ImageVerticalIter;
};