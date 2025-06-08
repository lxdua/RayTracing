#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "Walnut/Timer.h"

#include "Renderer.h"
#include "Camera.h"

#include <glm/gtc/type_ptr.hpp>

using namespace Walnut;

class ExampleLayer : public Walnut::Layer
{
public:
	ExampleLayer()
		: m_Camera(45.0f, 0.1f, 100.0f)
	{
		// 创建材质
		Material groundMat;
		groundMat.albedo = { 0.2f, 0.3f, 0.1f };
		groundMat.roughness = 0.9f;
		uint32_t groundMatID = m_Scene.AddMaterial(groundMat);

		Material sphereMat; // 铜色材质
		sphereMat.albedo = { 0.8f, 0.5f, 0.2f };
		sphereMat.metallic = 0.5f;
		uint32_t sphereMatID = m_Scene.AddMaterial(sphereMat);

		Material blueSphereMat; // 蓝色材质 - 新增
		blueSphereMat.albedo = { 0.2f, 0.3f, 0.8f };
		blueSphereMat.metallic = 0.9f;
		blueSphereMat.roughness = 0.2f;
		uint32_t blueMatID = m_Scene.AddMaterial(blueSphereMat);

		// 创建球体并分配材质
		{
			Sphere sphere;
			sphere.position = { 0.0f, -101.0f, 0.0f };
			sphere.radius = 100.0f;
			sphere.materialID = groundMatID; // 地面材质
			m_Scene.AddSphere(sphere);
		}
		{
			Sphere sphere;  // 中心铜色球
			sphere.position = { 0.0f, 0.0f, 0.0f };
			sphere.radius = 1.0f;
			sphere.materialID = sphereMatID;
			m_Scene.AddSphere(sphere);
		}
		{
			Sphere sphere;  // 新增蓝色球体
			sphere.position = { 1.7f, 0.5f, 0.0f };  // 右侧位置，轻微悬浮
			sphere.radius = 0.7f;  // 比中心球稍小
			sphere.materialID = blueMatID; // 新材质
			m_Scene.AddSphere(sphere);
		}
	}

	virtual void OnUpdate(float ts) override
	{
		m_Camera.OnUpdate(ts);
	}

	virtual void OnUIRender() override
	{
		ImGui::Begin("Setting");
		ImGui::DragInt("Rays Count: ", (int*)&m_Renderer.m_NumRays, 1, 1, 50);
		ImGui::DragInt("Max Bounce Count: ", (int*)&m_Renderer.m_MaxBounceCount, 1, 1, 5);
		ImGui::Text("Last render: %.3fms", m_LastRenderTime);
		if (ImGui::Button("Render"))
		{
			Render();
		}
		ImGui::Checkbox("IsRendering", &m_IsRendering);
		ImGui::Checkbox("JustDiffuse", &m_Renderer.m_JustDiffuse);
		ImGui::End();


		ImGui::Begin("Scene");

		// 球体列表
		for (size_t i = 0; i < m_Scene.spheres.size(); i++)
		{
			ImGui::PushID(static_cast<int>(i));
			Sphere& sphere = m_Scene.spheres[i];
			Material& material = m_Scene.GetMaterial(sphere.materialID);

			ImGui::Text("Sphere %zu", i);
			ImGui::DragFloat3("Position", glm::value_ptr(sphere.position), 0.01f);
			ImGui::DragFloat("Radius", &sphere.radius, 0.01f, 0.01f);

			// 材质选择器
			ImGui::Text("Material ID: %u", sphere.materialID);
			if (ImGui::BeginCombo("Material", ("Material " + std::to_string(sphere.materialID)).c_str()))
			{
				for (uint32_t matID = 0; matID < m_Scene.materials.size(); matID++)
				{
					bool isSelected = (sphere.materialID == matID);
					if (ImGui::Selectable(("Material " + std::to_string(matID)).c_str(), isSelected))
					{
						sphere.materialID = matID;
					}
					if (isSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::Separator();
			ImGui::PopID();
		}

		// 材质列表
		ImGui::Separator();
		for (size_t i = 0; i < m_Scene.materials.size(); i++)
		{
			ImGui::PushID(static_cast<int>(i + 1000)); // 避免ID冲突
			Material& material = m_Scene.materials[i];

			ImGui::Text("Material %zu", i);
			ImGui::ColorEdit3("Albedo##global", glm::value_ptr(material.albedo));
			ImGui::DragFloat("Metallic##global", &material.metallic, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Roughness##global", &material.roughness, 0.01f, 0.0f, 1.0f);
			ImGui::ColorEdit3("Emission Color##global", glm::value_ptr(material.emissionColor));
			ImGui::DragFloat("Emission Power##global", &material.emissionPower, 0.01f, 0.0f);

			ImGui::Separator();
			ImGui::PopID();
		}

		// 定向光源控制
		ImGui::Text("Directional Light");
		ImGui::Separator();
		ImGui::SliderFloat3("Direction", glm::value_ptr(m_Scene.directionalLight.direction), -1.0f, 1.0f);
		ImGui::ColorEdit3("Color", glm::value_ptr(m_Scene.directionalLight.color));
		ImGui::SliderFloat("Intensity", &m_Scene.directionalLight.intensity, 0.0f, 10.0f);

		// 点光源列表
		ImGui::Separator();
		ImGui::Text("Point Lights");
		for (size_t i = 0; i < m_Scene.pointLights.size(); i++)
		{
			ImGui::PushID(static_cast<int>(i) + 10000);
			PointLight& light = m_Scene.pointLights[i];

			ImGui::Text("Point Light %zu", i);
			ImGui::DragFloat3("Position", glm::value_ptr(light.position), 0.1f);
			ImGui::ColorEdit3("Color", glm::value_ptr(light.color));
			ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 100.0f);
			ImGui::SliderFloat("Range", &light.range, 0.1f, 50.0f);

			if (ImGui::Button("Remove"))
			{
				m_Scene.pointLights.erase(m_Scene.pointLights.begin() + i);
				ImGui::PopID();
				break;
			}

			ImGui::Separator();
			ImGui::PopID();
		}

		if (ImGui::Button("Add Point Light"))
		{
			m_Scene.pointLights.push_back({
				glm::vec3(0.0f, 3.0f, 0.0f),
				glm::vec3(1.0f),
				5.0f,
				10.0f
				});
		}

		ImGui::End();


		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Viewport");
		m_ViewportWidth = ImGui::GetContentRegionAvail().x;
		m_ViewportHeight = ImGui::GetContentRegionAvail().y;
		std::shared_ptr<Walnut::Image> image = m_Renderer.GetFinalImage();
		if (image)
		{
			ImGui::Image(image->GetDescriptorSet(),
				{ (float)image->GetWidth(), (float)image->GetHeight() },
				ImVec2(0, 1), ImVec2(1, 0));
		}
		ImGui::End();
		ImGui::PopStyleVar();

		if (m_IsRendering)
			Render();
	}

	void Render()
	{
		Timer timer;

		m_Camera.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Renderer.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Renderer.Render(m_Scene, m_Camera);

		m_LastRenderTime = timer.ElapsedMillis();
	}

private:
	Renderer m_Renderer;
	Scene m_Scene;
	Camera m_Camera;
	
	uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

	float m_LastRenderTime = 0;
	bool m_IsRendering = false;
};

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Dua RayTracing";

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<ExampleLayer>();
	app->SetMenubarCallback([app]()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Exit"))
			{
				app->Close();
			}
			ImGui::EndMenu();
		}
	});
	return app;
}