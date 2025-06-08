#include "Renderer.h"

#include <execution>
#include <chrono>

namespace Utils {

	static uint32_t ConvertToRGBA(const glm::vec4& color)
	{
		uint8_t r = (uint8_t)(color.r * 255.0f);
		uint8_t g = (uint8_t)(color.g * 255.0f);
		uint8_t b = (uint8_t)(color.b * 255.0f);
		uint8_t a = (uint8_t)(color.a * 255.0f);
		uint32_t result = (a << 24) | (b << 16) | (g << 8) | r;
		return result;
	}

	static uint32_t GetTimeBasedSeed()
	{
		auto now = std::chrono::high_resolution_clock::now();
		return static_cast<uint32_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(
				now.time_since_epoch()
			).count()
			);
	}

	static uint32_t PCG_Hash(uint32_t input)
	{
		uint32_t state = input * 747796405u + 2891336453u;
		uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
		return (word >> 22u) ^ word;
	}

	static float RandomFloat(uint32_t& seed)
	{
		seed = PCG_Hash(seed);
		return static_cast<float>(seed) / (static_cast<float>(std::numeric_limits<uint32_t>::max()) + 1.0f);
	}

	static glm::vec3 RandomVec3(uint32_t& seed, float min, float max)
	{
		return glm::vec3(
			RandomFloat(seed) * (max - min) + min,
			RandomFloat(seed) * (max - min) + min,
			RandomFloat(seed) * (max - min) + min);
	}

	static glm::vec3 InUnitSphere(uint32_t& seed)
	{
		return glm::normalize(glm::vec3(
			RandomFloat(seed) * 2.0f - 1.0f,
			RandomFloat(seed) * 2.0f - 1.0f,
			RandomFloat(seed) * 2.0f - 1.0f));
	}

}

void Renderer::Render(Scene& scene, Camera& camera)
{
	m_Scene = &scene;
	m_Camera = &camera;
	std::for_each(std::execution::par, m_ImageVerticalIter.begin(), m_ImageVerticalIter.end(),
		[this](uint32_t y)
		{
			std::for_each(std::execution::par, m_ImageHorizontalIter.begin(), m_ImageHorizontalIter.end(),
				[this, y](uint32_t x)
				{
					glm::vec4 color = PerPixel(x, y);
					uint32_t idx = x + y * m_FinalImage->GetWidth();
					m_ImageData[idx] = Utils::ConvertToRGBA(color);
				});
		});
	m_FinalImage->SetData(m_ImageData);
}

void Renderer::OnResize(uint32_t width, uint32_t height)
{
	if (m_FinalImage)
	{
		if (width == m_FinalImage->GetWidth() && height == m_FinalImage->GetHeight())
			return;
		m_FinalImage->Resize(width, height);
	}
	else
	{
		m_FinalImage = std::make_shared<Walnut::Image>(width, height, Walnut::ImageFormat::RGBA);
	}
	delete[] m_ImageData;
	m_ImageData = new uint32_t[width * height];

	m_ImageHorizontalIter.resize(width);
	for (uint32_t i = 0; i < width; i++)
		m_ImageHorizontalIter[i] = i;
	m_ImageVerticalIter.resize(height);
	for (uint32_t i = 0; i < height; i++)
		m_ImageVerticalIter[i] = i;
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y)
{
	uint32_t idx = x + y * m_FinalImage->GetWidth();
	Ray ray(m_Camera->GetPosition(), m_Camera->GetRayDirections()[idx]);
	glm::vec4 color = glm::vec4(TraceRay(*m_Scene, ray), 1.0f);
	return color;
}

glm::vec3 Renderer::TraceRay(Scene& scene, Ray ray)
{
	glm::vec3 totalColor(0.0f);
	for (uint32_t i = 1; i <= m_NumRays; i++)
	{
		totalColor += TraceRayOnce(scene, ray);
	}
	totalColor /= m_NumRays;
	totalColor = glm::clamp(totalColor, glm::vec3(0.0f), glm::vec3(1.0f));
	return totalColor;
}

glm::vec3 Renderer::TraceRayOnce(Scene& scene, Ray ray, uint32_t dep)
{
	if (dep >= m_MaxBounceCount)
		return GetSkyLight(ray);
	HitInfo hitInfo = CalculateRayCollision(scene, ray);
	if (!hitInfo.didHit)
		return GetSkyLight(ray);

	uint32_t seed = Utils::GetTimeBasedSeed();

	const Material& mat = scene.GetMaterial(hitInfo.materialID);

	// 直接光照
	glm::vec3 directColor = CalculateDirectLight(scene, hitInfo, ray);

	// 自发光
	glm::vec3 totalColor = mat.GetEmission();

	// 计算反射光线
	glm::vec3 reflectionDir = glm::reflect(ray.direction, hitInfo.normal);
	reflectionDir = glm::normalize(reflectionDir);

	// 添加粗糙度影响
	if (mat.roughness > 0.0f)
	{
		// 使用余弦加权的半球采样，更符合物理
		glm::vec3 randomDir = glm::normalize(Utils::RandomVec3(seed, -1.0f, 1.0f));
		if (glm::dot(randomDir, hitInfo.normal) < 0.0f) randomDir = -randomDir;

		// 混合理想反射和随机方向
		reflectionDir = glm::mix(reflectionDir, randomDir, mat.roughness);
		reflectionDir = glm::normalize(reflectionDir);
	}

	// 生成新光线
	Ray newRay;
	newRay.origin = hitInfo.hitPoint + hitInfo.normal * 0.001f;
	newRay.direction = reflectionDir;

	// 菲涅尔效应
	float cosTheta = glm::dot(glm::normalize(-ray.direction), hitInfo.normal);
	glm::vec3 fresnel = mat.FresnelSchlick(cosTheta);

	// 继续追踪反射光线
	glm::vec3 indirectColor = TraceRayOnce(scene, newRay, dep + 1);

	// 金属和非金属的不同处理
	float metallicFactor = mat.metallic;
	float diffuseFactor = (1.0f - metallicFactor) * (1.0f - fresnel.r);
	float specularFactor = metallicFactor + (1.0f - metallicFactor) * fresnel.r;

	totalColor += mat.albedo * (directColor * diffuseFactor +
		indirectColor * specularFactor);

	return totalColor;
}

glm::vec3 Renderer::CalculateDirectLight(Scene& scene, HitInfo& hitInfo, Ray& ray)
{
	const Material& mat = scene.GetMaterial(hitInfo.materialID);
	const DirectionalLight& light = scene.directionalLight;

	// 计算光照方向
	glm::vec3 lightDir = glm::normalize(light.direction);

	// 阴影测试
	Ray shadowRay;
	shadowRay.origin = hitInfo.hitPoint + hitInfo.normal * 0.001f;
	shadowRay.direction = -lightDir;

	float visibility = 1.0f;
	for (const Sphere& sphere : scene.spheres)
	{
		HitInfo shadowHit = RaySphere(shadowRay, sphere);
		if (shadowHit.didHit && shadowHit.dist > 0.001f)
		{
			visibility = 0.3f; // 部分阴影
			break;
		}
	}

	// 漫反射计算
	float NdotL = glm::max(0.0f, glm::dot(hitInfo.normal, -lightDir));
	glm::vec3 diffuse = mat.albedo * NdotL * light.color * light.intensity * visibility;

	if (m_JustDiffuse)
		return diffuse;

	// 镜面反射计算 - 使用半向量方法
	glm::vec3 viewDir = glm::normalize(ray.origin - hitInfo.hitPoint);
	glm::vec3 halfDir = glm::normalize(-lightDir + viewDir);

	// 粗糙度影响
	float roughness = glm::max(0.01f, mat.roughness);
	float NdotH = glm::max(0.0f, glm::dot(hitInfo.normal, halfDir));
	float specular = glm::pow(NdotH, 1.0f / roughness) * light.intensity;

	// 金属和非金属不同的高光颜色
	glm::vec3 specularColor = mat.metallic > 0.5f ? mat.albedo : glm::vec3(0.8f);

	return diffuse + specularColor * specular * visibility;
}

HitInfo Renderer::CalculateRayCollision(Scene& scene, Ray ray)
{
	HitInfo finalHitInfo;
	for (const Sphere& sphere : scene.spheres)
	{
		HitInfo hitInfo = RaySphere(ray, sphere);
		if (hitInfo.didHit && (!finalHitInfo.didHit || hitInfo.dist <= finalHitInfo.dist))
		{
			finalHitInfo = hitInfo;
		}
	}
	return finalHitInfo;
}

glm::vec3 Renderer::GetSkyLight(Ray ray)
{
	// 方向归一化
	glm::vec3 dir = glm::normalize(ray.direction);

	// 天空渐变参数
	float skyGradientT = glm::smoothstep(0.0f, 0.4f, dir.y); // 地平线平滑过渡
	float horizonIntensity = 1.0f - glm::abs(dir.y); // 地平线增强

	// 天空基色 - 从地平线到天顶渐变
	glm::vec3 horizonColor(0.3f, 0.6f, 1.0f);   // 浅蓝色
	glm::vec3 zenithColor(0.05f, 0.1f, 0.3f);    // 深蓝色
	glm::vec3 skyGradient = glm::mix(horizonColor, zenithColor, skyGradientT);

	// 地平线增强效果
	glm::vec3 horizonGlow = glm::vec3(1.0f, 0.7f, 0.4f) * horizonIntensity * horizonIntensity;
	skyGradient += horizonGlow * 0.25f;

	// 组合所有效果
	glm::vec3 finalColor = skyGradient;

	return glm::clamp(finalColor, 0.0f, 10.0f); // 限制最大亮度
}

HitInfo Renderer::RaySphere(Ray ray, Sphere sphere)
{
	HitInfo hitInfo;
	glm::vec3 offsetRayOrigin = ray.origin - sphere.position;
	float a = glm::dot(ray.direction, ray.direction);
	float b = 2.0 * glm::dot(offsetRayOrigin, ray.direction);
	float c = glm::dot(offsetRayOrigin, offsetRayOrigin) - sphere.radius * sphere.radius;
	float d = b * b - 4 * a * c;
	if (d > 0)
	{
		float ct = (-b - glm::sqrt(d)) / (2.0f * a);
		if (ct >= 0)
		{
			hitInfo.didHit = true;
			hitInfo.dist = ct;
			hitInfo.hitPoint = ray.origin + ray.direction * hitInfo.dist;
			hitInfo.normal = glm::normalize(hitInfo.hitPoint - sphere.position);
			hitInfo.materialID = sphere.materialID;
		}
	}
	return hitInfo;
}
