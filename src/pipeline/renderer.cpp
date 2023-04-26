#include "renderer.h"

#include <algorithm> //sort

#include "camera.h"
#include "../gfx/gfx.h"
#include "../gfx/shader.h"
#include "../gfx/mesh.h"
#include "../gfx/texture.h"
#include "../gfx/fbo.h"
#include "../pipeline/prefab.h"
#include "../pipeline/material.h"
#include "../pipeline/animation.h"
#include "../utils/utils.h"
#include "../extra/hdre.h"
#include "../core/ui.h"

#include "scene.h"


using namespace SCN;

//some globals
GFX::Mesh sphere;
//RENDERING MODE
eRenderMode current_mode = eRenderMode::FLAT;
eLightsRender current_lights_render = eLightsRender::MULTIPASS;
//RENDER CALLS AND PRIORITY
std::vector<RenderCall> render_calls;
std::vector<RenderCall> render_calls_opaque;
eRenderPriority current_priority = eRenderPriority::NOPRIORITY;
//SHADER
eShaders current_shader = eShaders::sFLAT;
//LIGHTS
std::vector<LightEntity*> lights;
std::vector<LightEntity> visible_lights;

Renderer::Renderer(const char* shader_atlas_filename)
{
	render_wireframe = false;
	render_boundaries = false;
	scene = nullptr;
	skybox_cubemap = nullptr;

	if (!GFX::Shader::LoadAtlas(shader_atlas_filename))
		exit(1);
	GFX::checkGLErrors();

	sphere.createSphere(1.0f);
}

void Renderer::setupScene(Camera* camera)
{
	if (scene->skybox_filename.size())
		skybox_cubemap = GFX::Texture::Get(std::string(scene->base_folder + "/" + scene->skybox_filename).c_str());
	else
		skybox_cubemap = nullptr;
	
	//PROCESS ENTITIES 
	
	//lights
	lights.clear();

	for (int i = 0; i < scene->entities.size(); ++i)
	{
		BaseEntity* ent = scene->entities[i];
		if (!ent->visible)
			continue;

		//is a prefab!
		if (ent->getType() == eEntityType::LIGHT)
		{
			LightEntity* lent = (SCN::LightEntity*)ent;
			lights.push_back(lent);
		}
	}

	//render calls
	//first of all, clear the render calls vectors
	render_calls.clear();
	render_calls_opaque.clear();

	switch (current_priority)
	{
	case(eRenderPriority::NOPRIORITY):
	{
		for (int i = 0; i < scene->entities.size(); ++i)
		{
			BaseEntity* ent = scene->entities[i];
			if (!ent->visible)
				continue;

			//is a prefab!
			if (ent->getType() == eEntityType::PREFAB)
			{
				PrefabEntity* pent = (SCN::PrefabEntity*)ent;
				if (pent->prefab)
					storeDrawCallNoPriority(&pent->root, camera);
			}
		}
		break;
	};
	case(eRenderPriority::ALPHA1):
	{
		//STORE DRAW CALLS IN VECTOR RENDER_CALLS
		for (int i = 0; i < scene->entities.size(); ++i)
		{
			BaseEntity* ent = scene->entities[i];
			if (!ent->visible)
				continue;

			//is a prefab!
			if (ent->getType() == eEntityType::PREFAB)
			{
				PrefabEntity* pent = (SCN::PrefabEntity*)ent;
				if (pent->prefab)
					storeDrawCall(&pent->root, camera);
			}
		}
		break;
	}
	case(eRenderPriority::DISTANCE2CAMERA):
	{
		//STORE DRAW CALLS IN VECTOR RENDER_CALLS
		for (int i = 0; i < scene->entities.size(); ++i)
		{
			BaseEntity* ent = scene->entities[i];
			if (!ent->visible)
				continue;

			//is a prefab!
			if (ent->getType() == eEntityType::PREFAB)
			{
				PrefabEntity* pent = (SCN::PrefabEntity*)ent;
				if (pent->prefab)
					storeDrawCall(&pent->root, camera);
			}
		}
		//ORDER RENDER CALLS BY DISTANCE TO CAMERA
		std::sort(render_calls.begin(), render_calls.end(), [](const RenderCall a, const RenderCall b) {return(a.distance_2_camera > b.distance_2_camera); });
		break;
	}
	}
}

const char* Renderer::getShader(eShaders current)
{
	switch (current)
	{
		case eShaders::sFLAT: return "flat";
		case eShaders::sTEXTURE: return "texture";
		case eShaders::sLIGHTS: return "lights";
	}
}

void Renderer::renderScene(SCN::Scene* scene, Camera* camera)
{
	this->scene = scene;
	setupScene(camera);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GFX::checkGLErrors();

	//render skybox
	if(skybox_cubemap)
		renderSkybox(skybox_cubemap);
	switch (current_priority)
	{
		case(eRenderPriority::NOPRIORITY):
		{
			for (int i = 0; i < render_calls.size(); ++i)
			{
				RenderCall rc = render_calls[i];
				renderRenderCalls(&rc);
			}
			break;
		};
		case(eRenderPriority::ALPHA1):
		{
			//render opaque entities
			for (int i = 0; i < render_calls_opaque.size(); ++i)
			{
				RenderCall rc = render_calls_opaque[i];
				renderRenderCalls(&rc);
			}

			//render transparent entities 
			for (int i = 0; i < render_calls.size(); ++i)
			{
				RenderCall rc = render_calls[i];
				renderRenderCalls(&rc);
			}
			break;
		}
		case(eRenderPriority::DISTANCE2CAMERA):
		{
			//render opaque entities
			for (int i = 0; i < render_calls_opaque.size(); ++i)
			{
				RenderCall rc = render_calls_opaque[i];
				renderRenderCalls(&rc);
			}
	
			//render entities
			for (int i = 0; i < render_calls.size(); ++i)
			{
				RenderCall rc = render_calls[i];
				renderRenderCalls(&rc);
			}
			break;
		}
	}
}

void Renderer::renderSkybox(GFX::Texture* cubemap)
{
	Camera* camera = Camera::current;

	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	GFX::Shader* shader = GFX::Shader::Get("skybox");
	if (!shader)
		return;
	shader->enable();

	Matrix44 m;
	m.setTranslation(camera->eye.x, camera->eye.y, camera->eye.z);
	m.scale(10, 10, 10);
	shader->setUniform("u_model", m);
	cameraToShader(camera, shader);
	shader->setUniform("u_texture", cubemap, 0);
	sphere.render(GL_TRIANGLES);
	shader->disable();
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_DEPTH_TEST);
}

//renders a node of the prefab and its children
void Renderer::renderNode(SCN::Node* node, Camera* camera)
{
	if (!node->visible)
		return;

	//compute global matrix
	Matrix44 node_model = node->getGlobalMatrix(true);

	//does this node have a mesh? then we must render it
	if (node->mesh && node->material)
	{
		//compute the bounding box of the object in world space (by using the mesh bounding box transformed to world space)
		BoundingBox world_bounding = transformBoundingBox(node_model,node->mesh->box);
		
		//if bounding box is inside the camera frustum then the object is probably visible
		if (camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize) )
		{
			if(render_boundaries)
				node->mesh->renderBounding(node_model, true);
			switch(current_mode)
			{
				case (eRenderMode::FLAT): 
				{
					renderMeshWithMaterial(node_model, node->mesh, node->material);
					break;
				}
				case (eRenderMode::LIGHTS): 
				{
					renderMeshWithMaterialLight(node_model, node->mesh, node->material);
					break;
				}
			}
		}
	}

	//iterate recursively with children
	for (int i = 0; i < node->children.size(); ++i)
		renderNode( node->children[i], camera);
}

//renders a mesh given its transform and material
void Renderer::renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material )
		return;
    assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	GFX::Shader* shader = NULL;
	GFX::Texture* white = NULL;
	GFX::Texture* albedo_texture = NULL;
	Camera* camera = Camera::current;
	
	white = GFX::Texture::getWhiteTexture();
	albedo_texture = material->textures[SCN::eTextureChannel::ALBEDO].texture;

	//texture = material->emissive_texture;
	//texture = material->metallic_roughness_texture;
	//texture = material->normal_texture;
	//texture = material->occlusion_texture;
	if (albedo_texture == NULL)
		albedo_texture = white; //a 1x1 white texture

	//select the blending
	if (material->alpha_mode == SCN::eAlphaMode::BLEND)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glDisable(GL_BLEND);

	//select if render both sides of the triangles
	if(material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
    assert(glGetError() == GL_NO_ERROR);

	glEnable(GL_DEPTH_TEST);

	//chose a shader
	const char* current = Renderer::getShader(current_shader);
	shader = GFX::Shader::Get(current);

    assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	//upload uniforms
	shader->setUniform("u_model", model);
	cameraToShader(camera, shader);
	float t = getTime();
	shader->setUniform("u_time", t );

	shader->setUniform("u_color", material->color);
	shader->setUniform("u_albedo_texture", albedo_texture ? albedo_texture : white, 0);
	shader->setUniform("u_emissive_factor", material->emissive_factor);

	shader->setUniform("u_ambient_light", scene->ambient_light);

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", material->alpha_mode == SCN::eAlphaMode::MASK ? material->alpha_cutoff : 0.001f);

	if (render_wireframe)
		glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

	//do the draw call that renders the mesh into the screen
	mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
}

//renders a mesh given its transform and material
void Renderer::renderMeshWithMaterialLight(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	GFX::Shader* shader = NULL;
	GFX::Texture* white = NULL;
	GFX::Texture* albedo_texture = NULL;
	GFX::Texture* emissive_texture = NULL;
	Camera* camera = Camera::current;

	white = GFX::Texture::getWhiteTexture();
	albedo_texture = material->textures[SCN::eTextureChannel::ALBEDO].texture;
	emissive_texture = material->textures[SCN::eTextureChannel::EMISSIVE].texture;

	//texture = material->emissive_texture;
	//texture = material->metallic_roughness_texture;
	//texture = material->normal_texture;
	//texture = material->occlusion_texture;
	if (albedo_texture == NULL)
		albedo_texture = white; //a 1x1 white texture

	//select the blending
	if (material->alpha_mode == SCN::eAlphaMode::BLEND)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glDisable(GL_BLEND);

	//select if render both sides of the triangles
	if (material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
	assert(glGetError() == GL_NO_ERROR);

	glEnable(GL_DEPTH_TEST);

	//chose a shader
	const char* current = Renderer::getShader(current_shader);
	shader = GFX::Shader::Get(current);

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	//upload uniforms
	shader->setUniform("u_model", model);
	cameraToShader(camera, shader);
	float t = getTime();
	shader->setUniform("u_time", t);

	shader->setUniform("u_color", material->color);	
	shader->setUniform("u_albedo_texture", albedo_texture ? albedo_texture : white, 0);
	shader->setUniform("u_emissive_texture", emissive_texture ? emissive_texture : white, 1);

	shader->setUniform("u_emissive_factor", material->emissive_factor);

	shader->setUniform("u_ambient_light", scene->ambient_light);

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", material->alpha_mode == SCN::eAlphaMode::MASK ? material->alpha_cutoff : 0.001f);

	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	switch (current_lights_render)
	{
	case(eLightsRender::MULTIPASS):
	{
		glDepthFunc(GL_LEQUAL); //render if the z is the same or closer to the camera
		if (lights.size() == 0)
		{
			shader->setUniform("u_light_type", (int)eLightType::NO_LIGHT);
			mesh->render(GL_TRIANGLES);
		}
		for (int i = 0; i < lights.size(); ++i)
		{
			LightEntity* light = lights[i];

			//shared variables between types of lights
			shader->setUniform("u_light_pos", light->root.model.getTranslation());
			shader->setUniform("u_light_color", light->color * light->intensity);
			shader->setUniform("u_light_type", (int)light->light_type);
			shader->setUniform("u_light_max_dist", light->max_distance);

			if (light->light_type == eLightType::POINT)
			{
			}

			else
				continue;
			//do the draw call that renders the mesh into the screen
			mesh->render(GL_TRIANGLES);

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		}
		break;
	}
	}
	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDepthFunc(GL_LESS); 
}


void SCN::Renderer::cameraToShader(Camera* camera, GFX::Shader* shader)
{
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix );
	shader->setUniform("u_camera_position", camera->eye);
}

#ifndef SKIP_IMGUI

void Renderer::showUI()
{
		
	ImGui::Checkbox("Wireframe", &render_wireframe);
	ImGui::Checkbox("Boundaries", &render_boundaries);

	//RENDER PRIORITY
	if (ImGui::TreeNode("Rendering Priority"))
	{
		const char* priority[] = { "Normal", "Opacity", "Dist2cam" };
		static int priority_current = 0;
		ImGui::Combo("Priority", &priority_current, priority, IM_ARRAYSIZE(priority), 3);

		if (priority_current == 0) current_priority = eRenderPriority::NOPRIORITY;
		if (priority_current == 1) current_priority = eRenderPriority::ALPHA1;
		if (priority_current == 2) current_priority = eRenderPriority::DISTANCE2CAMERA;

		ImGui::TreePop();
	}

	//RENDER MODE
	if (ImGui::TreeNode("Rendering Mode"))
	{
		const char* mode[] = { "Flat", "Lights" };
		static int mode_current = 0;
		ImGui::Combo("RenderMode", &mode_current, mode, IM_ARRAYSIZE(mode), 2);

		if (mode_current == 0)
		{
			current_mode = eRenderMode::FLAT;
			current_shader = eShaders::sFLAT;

			if (ImGui::TreeNode("Available Shaders"))
			{
				const char* shaders[] = { "Flat", "Texture" };
				static int shader_current = 0;
				ImGui::Combo("Shader", &shader_current, shaders, IM_ARRAYSIZE(shaders), 3);

				if (shader_current == 0) current_shader = eShaders::sFLAT;
				if (shader_current == 1) current_shader = eShaders::sTEXTURE;

				ImGui::TreePop();
			}
		}
		if (mode_current == 1)
		{
			current_mode = eRenderMode::LIGHTS;
			current_shader = eShaders::sLIGHTS;
			if (ImGui::TreeNode("Available Shaders"))
			{
				const char* shaders[] = { "Lights" };
				static int shader_current = 0;
				ImGui::Combo("Shader", &shader_current, shaders, IM_ARRAYSIZE(shaders), 1);

				if (shader_current == 0) current_shader = eShaders::sLIGHTS;

				ImGui::TreePop();
			}
		}

		ImGui::TreePop();
	}
}

#else
void Renderer::showUI() {}
#endif

void Renderer::storeDrawCall(SCN::Node* node, Camera* camera)
{
	if (!node->visible)
		return;

	//compute global matrix
	Matrix44 node_model = node->getGlobalMatrix(true);

	//does this node have a mesh? then we must render it
	if (node->mesh && node->material)
	{
		//compute the bounding box of the object in world space (by using the mesh bounding box transformed to world space)
		BoundingBox world_bounding = transformBoundingBox(node_model, node->mesh->box);

		//if bounding box is inside the camera frustum then the object is probably visible
		if (camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize))
		{
			Vector3f nodepos = node_model.getTranslation();
			RenderCall rc;
			rc.mesh = node->mesh;
			rc.material = node->material;
			rc.model = node_model;
			rc.distance_2_camera = camera->eye.distance(nodepos);

			rc.material->alpha_mode == NO_ALPHA ? render_calls_opaque.push_back(rc) : render_calls.push_back(rc);
		}
	}

	//iterate recursively with children
	for (int i = 0; i < node->children.size(); ++i)
		storeDrawCall(node->children[i], camera);
}

void Renderer::storeDrawCallNoPriority(SCN::Node* node, Camera* camera)
{
	if (!node->visible)
		return;

	//compute global matrix
	Matrix44 node_model = node->getGlobalMatrix(true);

	//does this node have a mesh? then we must render it
	if (node->mesh && node->material)
	{
		//compute the bounding box of the object in world space (by using the mesh bounding box transformed to world space)
		BoundingBox world_bounding = transformBoundingBox(node_model, node->mesh->box);

		//if bounding box is inside the camera frustum then the object is probably visible
		if (camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize))
		{
			Vector3f nodepos = node_model.getTranslation();
			RenderCall rc;
			rc.mesh = node->mesh;
			rc.material = node->material;
			rc.model = node_model;

			render_calls.push_back(rc);
		}
	}

	//iterate recursively with children
	for (int i = 0; i < node->children.size(); ++i)
		storeDrawCallNoPriority(node->children[i], camera);
}

void Renderer::renderRenderCalls(RenderCall* rc)
{
	if (rc->mesh && rc->material)
	{
		if(render_boundaries)
			rc->mesh->renderBounding(rc->model, true);

		switch (current_mode)
		{
		case (eRenderMode::FLAT):
		{
			renderMeshWithMaterial(rc->model, rc->mesh, rc->material);
			break;
		}
		case (eRenderMode::LIGHTS):
		{
			renderMeshWithMaterialLight(rc->model, rc->mesh, rc->material);
			break;
		}
		}
	}
}