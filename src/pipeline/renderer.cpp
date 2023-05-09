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
eShaders current_shader = eShaders::sTEXTURE;
//LIGHTS
std::vector<LightEntity*> lights;
std::vector<LightEntity*> visible_lights;

//DEFERRED FBOs
GFX::FBO* gbuffers_fbo = nullptr;
GFX::FBO* illumination_fbo = nullptr;
bool generate_gbuffers = false;
bool show_buffers = false;
bool show_globalpos = false;

constexpr auto MAX_LIGHTS = 12;

bool generate_shadowmap = false;
bool show_shadowmaps = false;

bool enable_specular = false;
bool enable_normalmap = false;

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
	sphere.uploadToVRAM();
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
	visible_lights.clear();

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

	if(current_mode == eRenderMode::LIGHTS)
		generateShadowMaps();
}

const char* Renderer::getShader(eShaders current)
{
	switch (current)
	{
		case eShaders::sFLAT: return "flat";
		case eShaders::sTEXTURE: return "texture";
		case eShaders::sTEXTURE_IMPROVED: return "texture_improved";
		case eShaders::sLIGHTS_MULTI: return "lights_multi";
		case eShaders::sLIGHTS_SINGLE: return "lights_single";
	}
}

void Renderer::renderScene(SCN::Scene* scene, Camera* camera)
{
	this->scene = scene;
	setupScene(camera);

	if (current_mode == eRenderMode::DEFERRED)
		renderFrameDeferred(scene, camera);
	else
		renderFrameForward(scene, camera);

	if (show_shadowmaps)
		renderShadowmaps();
}

void Renderer::renderFrameForward(SCN::Scene* scene, Camera* camera)
{
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	//set the camera as default (used by some functions in the framework)
	camera->enable();

	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GFX::checkGLErrors();

	//render skybox
	if(skybox_cubemap && current_shader != eShaders::sFLAT)
		renderSkybox(skybox_cubemap);

	prioritySwitch();
}

void SCN::Renderer::renderFrameDeferred(SCN::Scene* scene, Camera* camera)
{
	vec2 size = CORE::getWindowSize();

	generate_gbuffers = true;
	//Generate GBuffers
	if (!gbuffers_fbo || CORE::BaseApplication::instance->window_resized) //WE WILL GENERETE BUFFERS IF NOT EXIST OR WINDOW IS RESIZED
	{
		gbuffers_fbo = new GFX::FBO();
		gbuffers_fbo->create(size.x, size.y, 3, GL_RGBA, GL_UNSIGNED_BYTE, true);
		CORE::BaseApplication::instance->window_resized = false;
	}

	camera->enable();
	gbuffers_fbo->bind();

	//gbuffers_fbo->enableBuffers(true, false, false, false);
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	//gbuffers_fbo->enableAllBuffers();

	prioritySwitch();

	gbuffers_fbo->unbind();

	generate_gbuffers = false;

	//Compute illumination
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//render skybox
	if (skybox_cubemap)
		renderSkybox(skybox_cubemap);
	
	//render global illumination
	prioritySwitch();

	if (show_buffers)
		showGBuffers(size, camera);
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
			RenderCall* rc;
			rc->mesh = node->mesh;
			rc->material = node->material;
			rc->model = node->model;
			if(render_boundaries)
				node->mesh->renderBounding(node_model, true);
			switch(current_mode)
			{
				case (eRenderMode::FLAT): 
				{
					if (current_shader == eShaders::sFLAT)
					{
						renderMeshWithMaterialFlat(rc);
						break;
					}
					renderMeshWithMaterial(rc);
					break;
				}
				case (eRenderMode::LIGHTS): 
				{
					if (generate_shadowmap)
						renderMeshWithMaterialFlat(rc);
					else
						renderMeshWithMaterialLight(rc);
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
void Renderer::renderMeshWithMaterial(RenderCall* rc)
{
	//in case there is nothing to do
	if (!rc->mesh || !rc->mesh->getNumVertices() || !rc->material )
		return;
    assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	GFX::Shader* shader = NULL;
	GFX::Texture* white = NULL;
	GFX::Texture* albedo_texture = NULL;
	GFX::Texture* emissive_texture = NULL;
	GFX::Texture* metallic_roughness_texture = NULL;
	Camera* camera = Camera::current;
	
	white = GFX::Texture::getWhiteTexture();
	albedo_texture = rc->material->textures[SCN::eTextureChannel::ALBEDO].texture;
	emissive_texture = rc->material->textures[SCN::eTextureChannel::EMISSIVE].texture;
	metallic_roughness_texture = rc->material->textures[SCN::eTextureChannel::METALLIC_ROUGHNESS].texture;
	//texture = material->emissive_texture;
	//texture = material->metallic_roughness_texture;
	//texture = material->normal_texture;
	//texture = material->occlusion_texture;
	if (albedo_texture == NULL)
		albedo_texture = white; //a 1x1 white texture

	//select the blending
	if (rc->material->alpha_mode == SCN::eAlphaMode::BLEND)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glDisable(GL_BLEND);

	//select if render both sides of the triangles
	if(rc->material->two_sided)
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
	shader->setUniform("u_model", rc->model);
	cameraToShader(camera, shader);
	float t = getTime();
	shader->setUniform("u_time", t );

	shader->setUniform("u_color", rc->material->color);
	shader->setUniform("u_albedo_texture", albedo_texture ? albedo_texture : white, 0);

	if (current_shader == eShaders::sTEXTURE_IMPROVED)
	{
		shader->setUniform("u_emissive_texture", emissive_texture ? emissive_texture : white, 1);
		shader->setUniform("u_metallic_roughness_texture", metallic_roughness_texture ? metallic_roughness_texture : white, 2);
		shader->setUniform("u_emissive_factor", rc->material->emissive_factor);
		shader->setUniform("u_ambient_light", scene->ambient_light);
	}

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", rc->material->alpha_mode == SCN::eAlphaMode::MASK ? rc->material->alpha_cutoff : 0.001f);

	if (render_wireframe)
		glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

	//do the draw call that renders the mesh into the screen
	rc->mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
}

void Renderer::renderMeshWithMaterialFlat(RenderCall* rc)
{
	//in case there is nothing to do
	if (!rc->mesh || !rc->mesh->getNumVertices() || !rc->material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	GFX::Shader* shader = NULL;
	Camera* camera = Camera::current;

	//select if render both sides of the triangles
	if (rc->material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
	assert(glGetError() == GL_NO_ERROR);

	glEnable(GL_DEPTH_TEST);

	shader = GFX::Shader::Get("flat");

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	//upload uniforms
	shader->setUniform("u_model", rc->model);
	cameraToShader(camera, shader);
	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	//do the draw call that renders the mesh into the screen
	rc->mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

//renders a mesh given its transform and material
void Renderer::renderMeshWithMaterialLight(RenderCall* rc)
{
	//in case there is nothing to do
	if (!rc->mesh || !rc->mesh->getNumVertices() || !rc->material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	GFX::Shader* shader = NULL;
	GFX::Texture* black = NULL;
	GFX::Texture* white = NULL;
	GFX::Texture* albedo_texture = NULL;
	GFX::Texture* emissive_texture = NULL;
	GFX::Texture* metallic_roughness_texture = NULL;
	GFX::Texture* normal_texture = NULL;
	Camera* camera = Camera::current;

	black = GFX::Texture::getBlackTexture();
	white = GFX::Texture::getWhiteTexture();
	albedo_texture = rc->material->textures[SCN::eTextureChannel::ALBEDO].texture;
	emissive_texture = rc->material->textures[SCN::eTextureChannel::EMISSIVE].texture;
	metallic_roughness_texture = rc->material->textures[SCN::eTextureChannel::METALLIC_ROUGHNESS].texture; //r channel occlusion, g metallic, b roughness
	normal_texture = rc->material->textures[SCN::eTextureChannel::NORMALMAP].texture;

	//texture = material->emissive_texture;
	//texture = material->normal_texture;
	//texture = material->occlusion_texture;
	if (albedo_texture == NULL)
		albedo_texture = white; //a 1x1 white texture

	//select the blending
	if (rc->material->alpha_mode == SCN::eAlphaMode::BLEND)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glDisable(GL_BLEND);

	//select if render both sides of the triangles
	if (rc->material->two_sided)
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
	shader->setUniform("u_model", rc->model);
	cameraToShader(camera, shader);
	float t = getTime();
	shader->setUniform("u_time", t);

	shader->setUniform("u_color", rc->material->color);	
	shader->setTexture("u_albedo_texture", albedo_texture ? albedo_texture : white, 0);
	shader->setTexture("u_emissive_texture", emissive_texture ? emissive_texture : white, 1);
	shader->setTexture("u_metallic_roughness_texture", metallic_roughness_texture ? metallic_roughness_texture : white, 2);
	
	if (normal_texture && enable_normalmap)
	{
		shader->setTexture("u_normal_texture", normal_texture, 3);
	}

	shader->setUniform("u_enable_normalmaps", enable_normalmap);

	shader->setUniform("u_emissive_factor", rc->material->emissive_factor);

	shader->setUniform("u_ambient_light", scene->ambient_light);

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", rc->material->alpha_mode == SCN::eAlphaMode::MASK ? rc->material->alpha_cutoff : 0.001f);

	
	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	visible_lights.clear();

	for (int i = 0; i < lights.size(); ++i)
	{
		LightEntity* light = lights[i];
		if (light->light_type != eLightType::DIRECTIONAL && !BoundingBoxSphereOverlap(rc->bounding, light->root.model.getTranslation(), light->max_distance))
			continue;

		visible_lights.push_back(light);
	}

	if (visible_lights.size() == 0)
	{
		shader->setUniform("u_light_info", vec4((int)eLightType::NO_LIGHT, 0, 0, 0));
		rc->mesh->render(GL_TRIANGLES);
		//disable shader
		shader->disable();

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		return;
	}

	switch (current_lights_render)
	{
	case(eLightsRender::MULTIPASS): renderMultipass(shader, rc); break;
	case(eLightsRender::SINGLEPASS): renderSinglepass(shader,rc); break;
	}
	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDepthFunc(GL_LESS); 
}

void SCN::Renderer::renderMultipass(GFX::Shader* shader, RenderCall* rc)
{
	glDepthFunc(GL_LEQUAL); //render if the z is the same or closer to the camera

	for (int i = 0; i < visible_lights.size(); ++i)
	{
		LightEntity* light = visible_lights[i];

		if (light->light_type != eLightType::DIRECTIONAL && !BoundingBoxSphereOverlap(rc->bounding, light->root.model.getTranslation(), light->max_distance))
			continue;

		lightToShader(light, shader);

		//do the draw call that renders the mesh into the screen
		rc->mesh->render(GL_TRIANGLES);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);

		shader->setUniform("u_ambient_light", vec3(0.0));
		shader->setUniform("u_emissive_factor", vec3(0.0));
	}
}

void SCN::Renderer::renderSinglepass(GFX::Shader* shader, RenderCall* rc)
{
	int num_lights = visible_lights.size();

	shader->setUniform("u_num_lights", num_lights);
	//We set 12 as max lights
	Vector3f lights_pos[MAX_LIGHTS];
	Vector3f lights_color[MAX_LIGHTS];
	Vector4f lights_info[MAX_LIGHTS];
	Vector3f lights_front[MAX_LIGHTS];
	Vector2f lights_cone[MAX_LIGHTS];
	
	Vector2f shadows_params[MAX_LIGHTS];
	

	for (int i = 0; i < visible_lights.size(); ++i)
	{
		LightEntity* light = visible_lights[i];

		Vector3f light_pos = light->root.model.getTranslation();
		lights_pos[i] = light_pos;

		Vector3f light_color = light->color * light->intensity;
		lights_color[i] = light_color;

		//Light info 
		int light_type = (int)light->light_type;
		float light_near = light->near_distance;
		float light_max = light->max_distance;
		lights_info[i] = vec4(light_type, light_near, light_max, enable_specular);

		if (light_type != eLightType::POINT)
		{
			lights_front[i] = light->root.model.rotateVector(vec3(0, 0, 1));
			if (light_type != eLightType::DIRECTIONAL)
				lights_cone[i] = vec2(cos(light->cone_info.x * DEG2RAD), cos(light->cone_info.y * DEG2RAD));
		}

		shader->setUniform("u_shadow_params", vec2(light->shadowmap && light->cast_shadows ? 1 : 0, light->shadow_bias));
		if (light->shadowmap)
		{
			shader->setTexture("u_shadowmap", light->shadowmap, 8);
			shader->setUniform("u_shadow_viewproj", light->shadow_viewproj);
		}
	}
	shader->setUniform3Array("u_lights_pos", (float*)lights_pos, MAX_LIGHTS);
	shader->setUniform3Array("u_lights_color",(float*)lights_color, MAX_LIGHTS);
	shader->setUniform4Array("u_lights_info", (float*)lights_info, MAX_LIGHTS);
	shader->setUniform3Array("u_lights_front", (float*)lights_front, MAX_LIGHTS);
	shader->setUniform2Array("u_lights_cone", (float*)lights_cone, MAX_LIGHTS);

	//do the draw call that renders the mesh into the screen
	rc->mesh->render(GL_TRIANGLES);

	shader->setUniform("u_ambient_light", vec3(0.0));
	shader->setUniform("u_emissive_factor", vec3(0.0));
}

void SCN::Renderer::renderDeferredGBuffers(RenderCall* rc)
{
	//in case there is nothing to do
	if (!rc->mesh || !rc->mesh->getNumVertices() || !rc->material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	if (rc->material->alpha_mode == eAlphaMode::BLEND)
		return;
	//define locals to simplify coding
	GFX::Shader* shader = NULL;
	GFX::Texture* white = NULL;
	GFX::Texture* albedo_texture = NULL;
	GFX::Texture* emissive_texture = NULL;
	GFX::Texture* metallic_roughness_texture = NULL;
	Camera* camera = Camera::current;

	white = GFX::Texture::getWhiteTexture();
	albedo_texture = rc->material->textures[SCN::eTextureChannel::ALBEDO].texture;
	emissive_texture = rc->material->textures[SCN::eTextureChannel::EMISSIVE].texture;
	metallic_roughness_texture = rc->material->textures[SCN::eTextureChannel::METALLIC_ROUGHNESS].texture;

	if (albedo_texture == NULL)
		albedo_texture = white; //a 1x1 white texture

	glDisable(GL_BLEND);

	//select if render both sides of the triangles
	if (rc->material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
	assert(glGetError() == GL_NO_ERROR);

	glEnable(GL_DEPTH_TEST);

	//chose a shader
	shader = GFX::Shader::Get("gbuffers");

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	//upload uniforms
	shader->setUniform("u_model", rc->model);
	cameraToShader(camera, shader);
	float t = getTime();
	shader->setUniform("u_time", t);
	shader->setUniform("u_color", rc->material->color);
	shader->setUniform("u_albedo_texture", albedo_texture ? albedo_texture : white, 0);
	shader->setUniform("u_emissive_texture", emissive_texture ? emissive_texture : white, 1);
	shader->setUniform("u_metallic_roughness_texture", metallic_roughness_texture ? metallic_roughness_texture : white, 2);
	shader->setUniform("u_emissive_factor", rc->material->emissive_factor);
	shader->setUniform("u_alpha_cutoff", rc->material->alpha_mode == SCN::eAlphaMode::MASK ? rc->material->alpha_cutoff : 0.001f);

	rc->mesh->render(GL_TRIANGLES);

	shader->disable();
}

void SCN::Renderer::renderDeferred(RenderCall* rc)
{

	GFX::Mesh* quad = GFX::Mesh::getQuad();

	GFX::Shader* shader = GFX::Shader::Get("deferred_global");
	shader->enable();

	shader->setTexture("u_albedo_texture", gbuffers_fbo->color_textures[0], 0);
	shader->setTexture("u_normal_texture", gbuffers_fbo->color_textures[1], 1);
	shader->setTexture("u_extra_texture", gbuffers_fbo->color_textures[2], 2);
	shader->setTexture("u_depth_texture", gbuffers_fbo->depth_texture, 3);
	shader->setUniform("u_ambient_light", scene->ambient_light);

	quad->render(GL_TRIANGLES);

	for (int i = 0; i < lights.size(); ++i)
	{
		LightEntity* light = lights[i];
		if (light->light_type != eLightType::DIRECTIONAL && !BoundingBoxSphereOverlap(rc->bounding, light->root.model.getTranslation(), light->max_distance))
			continue;

		visible_lights.push_back(light);
	}

	//if (visible_lights.size() == 0)
	//{
	//	shader->setUniform("u_light_info", vec4((int)eLightType::NO_LIGHT, 0, 0, 0));
	//	rc->mesh->render(GL_TRIANGLES);
	//	//disable shader
	//	shader->disable();

	//	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	//	return;
	//}

	shader->disable();

	vec2 size = CORE::getWindowSize();
	Camera* camera = Camera::current;

	if (show_globalpos)
	{
		shader = GFX::Shader::Get("deferred_globalpos");
		shader->enable();
		shader->setTexture("u_depth_texture", gbuffers_fbo->depth_texture, 3);
		shader->setMatrix44("u_ivp", camera->inverse_viewprojection_matrix);
		shader->setUniform("u_iRes", vec2(1.0 / size.x, 1.0 / size.y));

		quad->render(GL_TRIANGLES);
		shader->disable();
	}

	else
	{
		shader = GFX::Shader::Get("deferred_light");
		shader->enable();

		shader->setTexture("u_albedo_texture", gbuffers_fbo->color_textures[0], 0);
		shader->setTexture("u_normal_texture", gbuffers_fbo->color_textures[1], 1);
		shader->setTexture("u_extra_texture", gbuffers_fbo->color_textures[2], 2);
		shader->setTexture("u_depth_texture", gbuffers_fbo->depth_texture, 3);
		shader->setUniform("u_ambient_light", scene->ambient_light);

		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		for (auto light : visible_lights)
		{
			lightToShader(light, shader);
			shader->setMatrix44("u_ivp", camera->inverse_viewprojection_matrix);
			shader->setUniform("u_iRes", vec2(1.0 / size.x, 1.0 / size.y));

			quad->render(GL_TRIANGLES);
		}
		glDisable(GL_BLEND);
	}

}

void SCN::Renderer::cameraToShader(Camera* camera, GFX::Shader* shader)
{
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix );
	shader->setUniform("u_camera_position", camera->eye);
}

void SCN::Renderer::lightToShader(LightEntity* light, GFX::Shader* shader)
{
	//shared variables between types of lights
	shader->setUniform("u_light_pos", light->root.model.getTranslation());
	shader->setUniform("u_light_color", light->color * light->intensity);
	shader->setUniform("u_light_info", vec4((int)light->light_type, light->near_distance, light->max_distance, enable_specular));

	if (light->light_type != eLightType::POINT)
	{
		shader->setUniform("u_light_front", light->root.model.rotateVector(vec3(0, 0, 1)));
		if (light->light_type != eLightType::DIRECTIONAL)
			shader->setUniform("u_light_cone", vec2(cos(light->cone_info.x * DEG2RAD), cos(light->cone_info.y * DEG2RAD)));
	}

	shader->setUniform("u_shadow_params", vec2(light->shadowmap && light->cast_shadows ? 1 : 0, light->shadow_bias));
	if (light->shadowmap && light->cast_shadows)
	{
		shader->setTexture("u_shadowmap", light->shadowmap, 8);
		shader->setUniform("u_shadow_viewproj", light->shadow_viewproj);
	}
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
		const char* mode[] = { "Flat", "Lights", "Deferred"};
		static int mode_current = 0;
		ImGui::Combo("RenderMode", &mode_current, mode, IM_ARRAYSIZE(mode), 2);

		if (mode_current == 0)
		{
			current_mode = eRenderMode::FLAT;
			current_shader = eShaders::sTEXTURE;

			if (ImGui::TreeNode("Available Shaders"))
			{
				const char* shaders[] = { "Flat", "Texture", "Texture++"};
				static int shader_current = current_shader;
				ImGui::Combo("Shader", &shader_current, shaders, IM_ARRAYSIZE(shaders), 3);

				if (shader_current == 0) current_shader = eShaders::sFLAT;
				if (shader_current == 1) current_shader = eShaders::sTEXTURE;
				if (shader_current == 2) current_shader = eShaders::sTEXTURE_IMPROVED;

				ImGui::TreePop();
			}
		}
		if (mode_current == 1)
		{
			current_mode = eRenderMode::LIGHTS;
			current_shader = eShaders::sLIGHTS_MULTI;
			if (ImGui::TreeNode("Available Shaders"))
			{
				const char* shaders[] = { "Multi", "Single"};
				static int shader_current = current_shader;
				ImGui::Combo("Shader", &shader_current, shaders, IM_ARRAYSIZE(shaders), 1);

				if (shader_current == 0)
				{
					current_shader = eShaders::sLIGHTS_MULTI;
					current_lights_render = eLightsRender::MULTIPASS;
				}

				if (shader_current == 1)
				{
					current_shader = eShaders::sLIGHTS_SINGLE;
					current_lights_render = eLightsRender::SINGLEPASS;
				}


				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Rendering parameters"))
			{
				ImGui::Checkbox("Enable Specular", &enable_specular);
				if(current_shader == eShaders::sLIGHTS_MULTI)
					ImGui::Checkbox("Show Shadowmaps", &show_shadowmaps);
				ImGui::Checkbox("Use normalmaps", &enable_normalmap);

				ImGui::TreePop();
			}
		}
		if (mode_current == 2)
		{
			current_mode = eRenderMode::DEFERRED;

			ImGui::Checkbox("Show Buffers", &show_buffers);
			ImGui::Checkbox("Show Global Pos", &show_globalpos);
		}
		ImGui::TreePop();
	}
}

#else
void Renderer::showUI() {}
#endif

void Renderer::showGBuffers(vec2 window_size, Camera* camera)
{
	glViewport(0, window_size.y/2, window_size.x / 2, window_size.y / 2);
	gbuffers_fbo->color_textures[0]->toViewport();
	glViewport(window_size.x / 2, window_size.y / 2, window_size.x / 2, window_size.y / 2);
	gbuffers_fbo->color_textures[1]->toViewport();
	glViewport(0, 0, window_size.x / 2, window_size.y / 2);
	gbuffers_fbo->color_textures[2]->toViewport();
	glViewport(window_size.x / 2, 0, window_size.x / 2, window_size.y / 2);
	GFX::Shader* shader = GFX::Shader::getDefaultShader("linear_depth");
	shader->enable();
	shader->setUniform("u_camera_nearfar", vec2(camera->near_plane, camera->far_plane));
	gbuffers_fbo->depth_texture->toViewport(shader);
	shader->disable();
	glViewport(0, 0, window_size.x, window_size.y);
}
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
			rc.bounding = world_bounding;

			rc.material->alpha_mode == eAlphaMode::NO_ALPHA ? render_calls_opaque.push_back(rc) : render_calls.push_back(rc);
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
			rc.bounding = world_bounding;

			render_calls.push_back(rc);
		}
	}

	//iterate recursively with children
	for (int i = 0; i < node->children.size(); ++i)
		storeDrawCallNoPriority(node->children[i], camera);
}

void Renderer::prioritySwitch()
{
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

void Renderer::generateShadowMaps()
{
	GFX::startGPULabel("Generate shadowmaps");

	generate_shadowmap = true;

	for (auto light : lights)
	{
		Camera* camera = new Camera();

		if (!light->cast_shadows)
			continue;

		//CHECK IF LIGHT INSIDE CAMERA
		//TODO

		if (light->light_type == eLightType::POINT || light->light_type == eLightType::NO_LIGHT)
			continue;

		if (!light->shadowmap_fbo)
		{
			light->shadowmap_fbo = new GFX::FBO;
			light->shadowmap_fbo->setDepthOnly(1024, 1024);
			light->shadowmap = light->shadowmap_fbo->depth_texture;
		}

		Vector3f pos = light->root.model.getTranslation();
		Vector3f front = light->root.model.rotateVector(Vector3f(0, 0, -1));
		Vector3f up = Vector3f(0, 1, 0);
		//SET UP IF IT IS SPOTLIGHT
		if (light->light_type == eLightType::SPOT)
			camera->setPerspective(light->cone_info.y * 2, 1.0, light->near_distance, light->max_distance); //BECAUSE IS A SPOTLIGHT IT IS PERSPECTIVE CAMERA

		//SET UP IF ITS DIRECTIONAL
		if (light->light_type == eLightType::DIRECTIONAL)
		{
			//use light area to define how big the frustum is
			float halfarea = light->area / 2;
			camera->setOrthographic(-halfarea, halfarea, halfarea, -halfarea, 0.1, light->max_distance);
		}
		
		camera->lookAt(pos, pos + front, up);

		light->shadowmap_fbo->bind();

		renderFrameForward(scene, camera);
		
		light->shadowmap_fbo->unbind();

		light->shadow_viewproj = camera->viewprojection_matrix;
	}

	generate_shadowmap = false;
	GFX::endGPULabel();
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
			renderMeshWithMaterial(rc);
			break;
		}
		case (eRenderMode::LIGHTS):
		{
			if (generate_shadowmap)
				renderMeshWithMaterialFlat(rc);
			else
				renderMeshWithMaterialLight(rc);
			break;
		}
		case (eRenderMode::DEFERRED):
		{
			if (generate_gbuffers)
				renderDeferredGBuffers(rc);
			else
				renderDeferred(rc);
			break;
		}
		}
	}
}


void Renderer::renderShadowmaps()
{
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	int x = 310;
	for (auto light : lights)
	{
		if (!light->shadowmap)
			continue;
		
		GFX::Shader* shader = GFX::Shader::getDefaultShader("linear_depth");
		shader->enable();
		shader->setUniform("u_camera_nearfar", vec2(light->near_distance, light->max_distance));
		glViewport(x, 100, 256, 256);

		x += 260;
		light->shadowmap->toViewport(shader);
	}

	vec2 size = CORE::getWindowSize();
	glViewport(0, 0, size.x, size.y);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
}