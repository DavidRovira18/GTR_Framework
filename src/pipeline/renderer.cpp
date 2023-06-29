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
GFX::Mesh plane;
GFX::Mesh* quad;
GFX::Mesh box;
constexpr auto MAX_LIGHTS = 12;

//create the probe
//sReflectionProbe probe;

Renderer::Renderer(const char* shader_atlas_filename)
{
	render_wireframe = false;
	render_boundaries = false;
	scene = nullptr;
	skybox_cubemap = nullptr;

	//TONEMAPPER
	enable_tonemapper = false;
	current_tonemapper = 0;
	tonemapper_scale = 1.0;
	tonemapper_avg_lum = 1.0;
	tonemapper_lumwhite = 1.0;
	gamma = 2.2;

	air_density = 0.001;

	if (!GFX::Shader::LoadAtlas(shader_atlas_filename))
		exit(1);
	GFX::checkGLErrors();

	sphere.createSphere(1.0f);
	sphere.uploadToVRAM();

	plane.createPlane(500.0);
	plane.uploadToVRAM();

	box.createCube(1.0f);
	box.uploadToVRAM();

	quad = GFX::Mesh::getQuad();

	random_points = generateSpherePoints(128, 1.0, false);

	irradiance_cache_info.num_probes = 0;

	

	////set PROBE up
	//probe->pos.set(90, 56, -72);
	//probe->cubemap = new GFX::Texture();
	//probe->cubemap->createCubemap(
	//	512, 512, 	//size
	//	NULL, 	//data
	//	GL_RGB, GL_UNSIGNED_INT, true);	//mipmaps

	//	//add it to the list
	//reflection_probes.push_back(probe);
	//probe.pos.set(50,50,50);
}

void Renderer::setupScene(Camera* camera)
{
	if (scene->skybox_filename.size())
		skybox_cubemap = GFX::Texture::Get(std::string(scene->base_folder + "/" + scene->skybox_filename).c_str());
	else
		skybox_cubemap = nullptr;
	
	processEntities();

	processRenderCalls(camera);
	
	if(current_mode != eRenderMode::FLAT)
		generateShadowMaps();

	if (capture_irradiance)
	{
		captureIrradiance();
		capture_irradiance = false;
	}
	vec2 size = CORE::getWindowSize();

	if (capture_reflectance)
	{
		captureReflection();
		capture_reflectance = false;
	}

	if (!illumination_fbo || CORE::BaseApplication::instance->window_resized)
	{
		illumination_fbo = new GFX::FBO();
		illumination_fbo->create(size.x, size.y, 3, GL_RGB, GL_HALF_FLOAT, false);
	}

}

void SCN::Renderer::processEntities()
{
	//lights
	lights.clear();
	visible_lights.clear();
	decals.clear();

	for (int i = 0; i < scene->entities.size(); ++i)
	{
		BaseEntity* ent = scene->entities[i];
		if (!ent->visible)
			continue;

		if (ent->getType() == eEntityType::LIGHT)
		{
			LightEntity* lent = (SCN::LightEntity*)ent;
			lights.push_back(lent);
		}

		if (ent->getType() == eEntityType::DECAL)
		{
			DecalEntity* dent = (SCN::DecalEntity*)ent;
			decals.push_back(dent);
		}
	} 
}

void SCN::Renderer::processRenderCalls(Camera* camera)
{
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
		case eShaders::sTEXTURE_IMPROVED: return "texture_improved";
		case eShaders::sLIGHTS_MULTI: return "lights_multi";
		case eShaders::sLIGHTS_SINGLE: return "lights_single";
		case eShaders::sLIGHTS_PBR: return "light_pbr";
		case eShaders::sDEFERRED: return "deferred_light";
		case eShaders::sDEFERRED_PBR: return "deferred_pbr";
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

	CORE::BaseApplication::instance->window_resized = false;
}

void SCN::Renderer::setupRenderFrame()
{
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	GFX::checkGLErrors();

	//render skybox
	if (skybox_cubemap && current_shader != eShaders::sFLAT)
		renderSkybox(skybox_cubemap, scene->skybox_intensity);
}

void Renderer::renderFrameForward(SCN::Scene* scene, Camera* camera)
{
	
	if (show_planer_reflection && current_shader == eShaders::sLIGHTS_MULTI)
		capturePlanerReflection(camera);

	
	//set the camera as default (used by some functions in the framework)
	camera->enable();

	illumination_fbo->bind();

		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);

		setupRenderFrame();

		renderByPriority();

		if (show_reflection_probes)
		{
			for (int i = 0; i < reflection_probes.size(); i++)
			{
				renderReflectionProbe(reflection_probes[i]);
			}
		}
		
		//planer reflection
		if (show_planer_reflection && current_shader == eShaders::sLIGHTS_MULTI)
			renderPlanerReflectionFBO(camera);
		

	illumination_fbo->unbind();
	
	if (enable_tonemapper)
		renderTonemapper(illumination_fbo->color_textures[0]);
	else
		renderGamma(illumination_fbo->color_textures[0]);


	
}

void SCN::Renderer::renderFrameDeferred(SCN::Scene* scene, Camera* camera)
{
	vec2 size = CORE::getWindowSize();
	
	initDeferredFBOs();

	camera->enable();

	gbuffers_fbo->bind();
		//gbuffers_fbo->enableBuffers(true, false, false, false);
		glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		//gbuffers_fbo->enableAllBuffers();
		renderByPriority();
	gbuffers_fbo->unbind();
	
	if (decals.size())
	{
		gbuffers_fbo->depth_texture->copyTo(depth_buffer_clone);
		gbuffers_fbo->color_textures[1]->copyTo(normal_buffer_clone);
		gbuffers_fbo->color_textures[2]->copyTo(extra_buffer_clone);


		gbuffers_fbo->bind();
			camera->enable();
			createDecals(camera);
		gbuffers_fbo->unbind();
	}

	if (show_buffers)
		showGBuffers(size, camera);
	else
	{
		//ssao
		if (add_SSAO || show_ssao)
		{
			ssao_fbo->bind();
				generateSSAO(camera);
			ssao_fbo->unbind();
		}
		
		//volumetric
		volumetric_fbo->bind();
			generateVolumetricAir(camera);
		volumetric_fbo->unbind();

		//reflections
		deferred_reflections_fbo->bind();
			generateReflectionDeferred(camera);
		deferred_reflections_fbo->unbind();


		if (!show_ssao && !show_volumetric && !show_reflection_fbo) {
			//Compute illumination
			illumination_fbo->bind();
				computeIlluminationDeferred();
			illumination_fbo->unbind();

			renderPostFX(illumination_fbo->color_textures[0], gbuffers_fbo->depth_texture);
		}	
	}

	if (show_volumetric) {
		volumetric_fbo->color_textures[0]->toViewport();
	}

	if (show_ssao) {
		ssao_fbo->color_textures[0]->toViewport();
	}
	
	if(show_reflection_fbo)
		deferred_reflections_fbo->color_textures[0]->toViewport();
}

void Renderer::renderSkybox(GFX::Texture* cubemap, float intensity)
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
	shader->setUniform("u_skybox_intensity", intensity);
	sphere.render(GL_TRIANGLES);
	shader->disable();
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_DEPTH_TEST);
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
	
	Camera* camera = Camera::current;
	
	GFX::Texture* white = GFX::Texture::getWhiteTexture();
	GFX::Texture* albedo_texture = rc->material->textures[SCN::eTextureChannel::ALBEDO].texture;
	GFX::Texture* emissive_texture = rc->material->textures[SCN::eTextureChannel::EMISSIVE].texture;
	GFX::Texture* metallic_roughness_texture = rc->material->textures[SCN::eTextureChannel::METALLIC_ROUGHNESS].texture;
	
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
		shader->setUniform("u_ambient_light", scene->ambient_light ^ 2.2f);
		shader->setTexture("u_skybox", skybox_cubemap,3);

		cameraToShader(camera, shader);

		shader->setUniform3("u_reflections_info", vec3(enable_reflections, reflections_factor, enable_fresnel));
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
	//define locals to simplify coding
	GFX::Shader* shader = NULL;
	Camera* camera = Camera::current;

	//compute the bounding box of the object in world space (by using the mesh bounding box transformed to world space)
	BoundingBox world_bounding = transformBoundingBox(rc->model, rc->mesh->box);

	//if bounding box is inside the camera frustum then the object is probably visible
	if (!camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize))
		return;

	assert(glGetError() == GL_NO_ERROR);

	//TODO: CHECK OBJECTS INSIDE LIGHT CAMERA
	
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
	
	Camera* camera = Camera::current;
	
	GFX::Texture* normal_texture = rc->material->textures[SCN::eTextureChannel::NORMALMAP].texture;

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
	materialToShader(shader, rc->material);

	float t = getTime();
	shader->setUniform("u_time", t);

	if (normal_texture && enable_normalmap)
	{
		shader->setTexture("u_normal_texture", normal_texture, 3);
	}

	shader->setUniform("u_enable_normalmaps", enable_normalmap);

	shader->setUniform("u_emissive_factor", rc->material->emissive_factor);

	shader->setUniform("u_ambient_light", scene->ambient_light ^ 2.2f);

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", rc->material->alpha_mode == SCN::eAlphaMode::MASK ? rc->material->alpha_cutoff : 0.001f);

	
	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	setVisibleLights(rc);

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
		case(eLightsRender::MULTIPASS_TRANSPARENCIES):renderMultipassTransparencies(shader, rc); break;
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

	//planer refelctions
	bool has_planer_reflection = rc->material->planer_reflection && show_planer_reflection;
	if (has_planer_reflection)
	{
		vec2 size = CORE::getWindowSize();
		shader->setTexture("u_planer_reflection_texture", planer_reflection_fbo->color_textures[0], 5);
		shader->setUniform("u_apply_fresnel", use_fresnel_planer_reflection);
		shader->setUniform("u_iRes", vec2(1.0 / size.x, 1.0 / size.y));
	}

	shader->setUniform("u_planer_reflection", has_planer_reflection);

	for (int i = 0; i < visible_lights.size(); ++i)
	{
		LightEntity* light = visible_lights[i];

		if (light->light_type != eLightType::DIRECTIONAL && !BoundingBoxSphereOverlap(rc->bounding, light->root.model.getTranslation(), light->max_distance))
			continue;

		lightToShader(light, shader);

		sReflectionProbe* enviorment = getClosestReflectionProbe(rc->model);

		if(!capture_reflectance)
			shader->setTexture("u_environment", (enviorment) ? enviorment->cubemap : skybox_cubemap, 9);
		
		shader->setUniform("u_enable_reflections", capture_reflectance ? false : enable_reflections);
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

		Vector3f light_color = (light->color ^ 2.2f) * light->intensity;
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

void SCN::Renderer::setVisibleLights(RenderCall* rc)
{
	visible_lights.clear();

	for (int i = 0; i < lights.size(); ++i)
	{
		LightEntity* light = lights[i];
		if (light->light_type != eLightType::DIRECTIONAL && !BoundingBoxSphereOverlap(rc->bounding, light->root.model.getTranslation(), light->max_distance))
			continue;

		visible_lights.push_back(light);
	}
}

void SCN::Renderer::renderDeferredGBuffers(RenderCall* rc)
{
	//in case there is nothing to do
	if (!rc->mesh || !rc->mesh->getNumVertices() || !rc->material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	GFX::Shader* shader = NULL;

	Camera* camera = Camera::current;
	
	GFX::Texture* normal_texture = rc->material->textures[SCN::eTextureChannel::NORMALMAP].texture;

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
	materialToShader(shader, rc->material);
	float t = getTime();
	shader->setUniform("u_time", t);
	shader->setUniform("u_emissive_factor", rc->material->emissive_factor);
	if (normal_texture && enable_normalmap)
	{
		shader->setUniform("u_normal_texture", normal_texture, 3);
	}
	shader->setUniform("u_enable_normalmaps", enable_normalmap);

	shader->setUniform("u_alpha_cutoff", rc->material->alpha_mode == SCN::eAlphaMode::MASK ? rc->material->alpha_cutoff : 0.001f);

	if (enable_dithering && rc->material->alpha_mode == eAlphaMode::BLEND)
		shader->setUniform("u_enable_dithering", 1.0f);
	else
		shader->setUniform("u_enable_dithering", 0.0f);

	rc->mesh->render(GL_TRIANGLES);

	shader->disable();
}

void SCN::Renderer::renderDeferred()
{
	GFX::Shader* shader = nullptr;
	renderDeferredGlobal(shader);

	if(enable_irradiance)
		applyIrradiance();

	Camera* camera = Camera::current;
	vec2 size = CORE::getWindowSize();
	
	if (show_globalpos)
		renderDeferredGlobalPos(shader, camera);

	else
	{
		renderDeferredLights(shader, camera);

		//glDepthFunc(GL_LESS);

		if (volumetric_fbo)
		{
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);

			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				volumetric_fbo->color_textures[0]->toViewport();
			glDisable(GL_BLEND);
		}

		if (enable_reflections)
		{
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);

			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			deferred_reflections_fbo->color_textures[0]->toViewport();
			glDisable(GL_BLEND);
		}

		glEnable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);


		//Irradiance cache
		if (show_probes)
		{
			for (int i = 0; i < probes.size(); ++i)
				renderIrradianceProbe(probes[i]);
		}
	}
}

void SCN::Renderer::renderDeferredGlobal(GFX::Shader* shader)
{
	vec2 size = CORE::getWindowSize();

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	//GFX::Mesh* quad = GFX::Mesh::getQuad();

	shader = GFX::Shader::Get("deferred_global");
	shader->enable();

	bufferToShader(shader);
	shader->setUniform("u_ambient_light", enable_irradiance ? 0.0 : scene->ambient_light ^ 2.2f);
	//shader->setUniform("u_ambient_light", vec3());

	shader->setTexture("u_ao_texture", ssao_fbo->color_textures[0], 4);
	shader->setUniform("u_iRes", vec2(1.0 / size.x, 1.0 / size.y));
	shader->setUniform("u_add_SSAO", add_SSAO);
	shader->setUniform("u_control_SSAO", control_SSAO_factor);

	quad->render(GL_TRIANGLES);

	shader->disable();
}

void SCN::Renderer::renderDeferredGlobalPos(GFX::Shader* shader, Camera* camera)
{
	vec2 size = CORE::getWindowSize();

	shader = GFX::Shader::Get("deferred_globalpos");
	shader->enable();
	shader->setTexture("u_depth_texture", gbuffers_fbo->depth_texture, 3);
	shader->setMatrix44("u_ivp", camera->inverse_viewprojection_matrix);
	shader->setUniform("u_iRes", vec2(1.0 / size.x, 1.0 / size.y));

	quad->render(GL_TRIANGLES);
	shader->disable();
}

void SCN::Renderer::renderDeferredLights(GFX::Shader* shader, Camera* camera)
{

	std::string current = Renderer::getShader(current_shader);

	shader = GFX::Shader::Get(current.c_str());
	
	renderDeferredDirectionalLights(shader, camera);

	current += "_geometry";
	shader = GFX::Shader::Get(current.c_str());
	
	renderDeferredGeometryLights(shader, camera);
}

void SCN::Renderer::renderDeferredDirectionalLights(GFX::Shader* shader, Camera* camera)
{
	vec2 size = CORE::getWindowSize();

	shader->enable();
	shader->setMatrix44("u_ivp", camera->inverse_viewprojection_matrix);
	shader->setUniform("u_iRes", vec2(1.0 / size.x, 1.0 / size.y));

	bufferToShader(shader);
	cameraToShader(camera, shader);

	for (auto light : lights)
	{
		if (light->light_type == eLightType::DIRECTIONAL)
		{
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
			lightToShader(light, shader);

			quad->render(GL_TRIANGLES);

		}
	}
	shader->disable();
}

void SCN::Renderer::renderDeferredGeometryLights(GFX::Shader* shader, Camera* camera)
{
	vec2 size = CORE::getWindowSize();

	shader->enable();

	bufferToShader(shader);
	shader->setMatrix44("u_ivp", camera->inverse_viewprojection_matrix);
	shader->setUniform("u_iRes", vec2(1.0 / size.x, 1.0 / size.y));
	cameraToShader(camera, shader);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_GREATER);
	glDepthMask(false);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glDisable(GL_CULL_FACE);
	glFrontFace(GL_CW);
	for (auto light : lights)
	{
		if (light->light_type == eLightType::DIRECTIONAL)
			continue;
		lightToShader(light, shader);


		vec3 center = light->root.model.getTranslation();
		float radius = light->max_distance;

		Matrix44 model;
		model.setTranslation(center.x, center.y, center.z);
		model.scale(radius, radius, radius);

		shader->setUniform("u_model", model);

		sphere.render(GL_TRIANGLES);
	}
	glDepthFunc(GL_LESS);
	glFrontFace(GL_CCW);
	glDisable(GL_BLEND);
	glDepthMask(true);

}

void SCN::Renderer::initDeferredFBOs()
{
	vec2 size = CORE::getWindowSize();

	//Generate GBuffers
	if (!gbuffers_fbo || CORE::BaseApplication::instance->window_resized) //WE WILL GENERETE BUFFERS IF NOT EXIST OR WINDOW IS RESIZED
	{
		gbuffers_fbo = new GFX::FBO();
		gbuffers_fbo->create(size.x, size.y, 3, GL_RGBA, GL_UNSIGNED_BYTE, true);
	}

	if (!depth_buffer_clone || CORE::BaseApplication::instance->window_resized) //WE WILL GENERETE BUFFERS IF NOT EXIST OR WINDOW IS RESIZED
	{
		depth_buffer_clone = new GFX::Texture(size.x, size.y, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT);
	}

	if (!normal_buffer_clone || CORE::BaseApplication::instance->window_resized) //WE WILL GENERETE BUFFERS IF NOT EXIST OR WINDOW IS RESIZED
	{
		normal_buffer_clone = new GFX::Texture(size.x, size.y, GL_RGB, GL_UNSIGNED_INT);
	}

	if (!extra_buffer_clone || CORE::BaseApplication::instance->window_resized) //WE WILL GENERETE BUFFERS IF NOT EXIST OR WINDOW IS RESIZED
	{
		extra_buffer_clone = new GFX::Texture(size.x, size.y, GL_RGB, GL_UNSIGNED_INT);
	}

	if (!ssao_fbo || CORE::BaseApplication::instance->window_resized)
	{
		ssao_fbo = new GFX::FBO();
		ssao_fbo->create(size.x, size.y, 3, GL_RGBA, GL_UNSIGNED_BYTE, false);	//TODO: is better if we use half of the resolution -> change size
	}

	if (!volumetric_fbo || CORE::BaseApplication::instance->window_resized)
	{
		volumetric_fbo = new GFX::FBO();
		volumetric_fbo->create(size.x/2, size.y/2, 3, GL_RGBA, GL_UNSIGNED_BYTE, false);	
	}

	if (!deferred_reflections_fbo || CORE::BaseApplication::instance->window_resized)
	{
		deferred_reflections_fbo = new GFX::FBO();
		deferred_reflections_fbo->create(size.x / 2, size.y / 2, 3, GL_RGBA, GL_UNSIGNED_BYTE, false);		//TODO: check parameters
	}
}

void SCN::Renderer::createDecals(Camera* camera)
{
	glEnable(GL_DEPTH_TEST);
	glDepthMask(false);
	glDepthFunc(GL_GREATER);
	glEnable(GL_BLEND);		//Remove to use metalness and roughness
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //Remove to use metalness and roughness
	glFrontFace(GL_CW);
	glEnable(GL_CULL_FACE);

	GFX::Texture* texture;
	GFX::Shader* shader = GFX::Shader::Get("decal");
	shader->enable();
	cameraToShader(camera, shader);
	shader->setTexture("u_depth_texture", depth_buffer_clone, 4);
	shader->setTexture("u_normal_texture", normal_buffer_clone, 5);
	shader->setTexture("u_extra_texture", extra_buffer_clone, 6);

	shader->setUniform("u_ivp", camera->inverse_viewprojection_matrix);
	shader->setUniform("u_iRes", vec2(1.0 / gbuffers_fbo->color_textures[0]->width, 1.0 / gbuffers_fbo->color_textures[1]->height));

	for (auto decal : decals)
	{
		if (decal->filename.size() == 0)
			continue;

		texture = GFX::Texture::Get((std::string("data/") + decal->filename).c_str());
		shader->setUniform("u_model", decal->root.model);
		Matrix44 imodel = decal->root.model;
		imodel.inverse();
		shader->setUniform("u_imodel", imodel);
		shader->setTexture("u_color_texture", texture, 1);
		shader->setUniform("u_metalness", decal->metalness);
		shader->setUniform("u_roughness", decal->roughness);

		box.render(GL_TRIANGLES);
	}
	glDisable(GL_DEPTH_TEST);
	glDepthMask(true);
	glDisable(GL_BLEND);
	glFrontFace(GL_CCW);
	glDepthFunc(GL_LESS);
}

void SCN::Renderer::generateVolumetricAir(Camera* camera)
{
	glClearColor(0.0, 0.0, 0.0, 0.0);	//TODO: check
	glClear(GL_COLOR_BUFFER_BIT);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GFX::Shader* shader = GFX::Shader::Get("volumetric");
	shader->enable();
	shader->setTexture("u_depth_texture", gbuffers_fbo->depth_texture, 2);
	shader->setMatrix44("u_ivp", camera->inverse_viewprojection_matrix);
	shader->setUniform("u_iRes", vec2(1.0 / volumetric_fbo->color_textures[0]->width, 1.0 / volumetric_fbo->color_textures[0]->height));

	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_air_density", air_density);
	shader->setUniform("u_ambient_light", scene->ambient_light);
	shader->setUniform("u_constant_denisty", constant_density);

	

	for (auto light : lights)
	{
		if (light->light_type == POINT)
			continue;

		if (!light->volumetric_fog)
			continue;

		lightToShader(light, shader);
		quad->render(GL_TRIANGLES);
	}

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
}

void SCN::Renderer::generateSSAO(Camera* camera)
{
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);


	GFX::Shader* shader = GFX::Shader::Get("ssao");
	shader->enable();
	shader->setTexture("u_normal_texture", gbuffers_fbo->color_textures[1], 1);
	shader->setTexture("u_depth_texture", gbuffers_fbo->depth_texture, 2);
	shader->setMatrix44("u_ivp", camera->inverse_viewprojection_matrix);
	shader->setUniform("u_iRes", vec2(1.0 / ssao_fbo->color_textures[0]->width, 1.0 / ssao_fbo->color_textures[0]->height));

	shader->setUniform3Array("u_random_points", (float*)(&random_points[0]), 64);
	shader->setUniform("u_radius", ssao_radius);
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_front", camera->front);

	quad->render(GL_TRIANGLES);
}

void SCN::Renderer::computeIlluminationDeferred()
{
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	setupRenderFrame();

	//render illumination
	renderDeferred();

	/*if (!enable_dithering)
	{
		current_lights_render = eLightsRender::MULTIPASS_TRANSPARENCIES;
		renderTransparenciesForward();
	}*/
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
	shader->setUniform("u_light_color", (light->color ^ gamma) * light->intensity);
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

void SCN::Renderer::bufferToShader(GFX::Shader* shader)
{
	shader->setTexture("u_albedo_texture", gbuffers_fbo->color_textures[0], 0);
	shader->setTexture("u_normal_texture", gbuffers_fbo->color_textures[1], 1);
	shader->setTexture("u_extra_texture", gbuffers_fbo->color_textures[2], 2);
	shader->setTexture("u_depth_texture", gbuffers_fbo->depth_texture, 3);
}

void SCN::Renderer::materialToShader(GFX::Shader* shader, SCN::Material* material)
{
	
	GFX::Texture* white = GFX::Texture::getWhiteTexture();
	GFX::Texture* albedo_texture = material->textures[SCN::eTextureChannel::ALBEDO].texture;
	GFX::Texture* emissive_texture = material->textures[SCN::eTextureChannel::EMISSIVE].texture;
	GFX::Texture* metallic_roughness_texture = material->textures[SCN::eTextureChannel::METALLIC_ROUGHNESS].texture; //r channel occlusion, g metallic, b roughness

	shader->setUniform("u_color", material->color);
	shader->setTexture("u_albedo_texture", albedo_texture ? albedo_texture : white, 0);
	shader->setTexture("u_emissive_texture", emissive_texture ? emissive_texture : white, 1);
	shader->setTexture("u_metallic_roughness_texture", metallic_roughness_texture ? metallic_roughness_texture : white, 2);
	shader->setUniform("u_mat_properties", vec2(material->metallic_factor, material->roughness_factor));
	
}

void Renderer::renderTransparenciesForward()
{
	for (auto rc : render_calls)
	{
		renderMeshWithMaterialLight(&rc);
	}
}

void Renderer::renderMultipassTransparencies(GFX::Shader* shader, RenderCall* rc)
{
	glDepthFunc(GL_EQUAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	for (int i = 0; i < visible_lights.size(); ++i)
	{
		LightEntity* light = visible_lights[i];

		if (light->light_type != eLightType::DIRECTIONAL && !BoundingBoxSphereOverlap(rc->bounding, light->root.model.getTranslation(), light->max_distance))
			continue;

		lightToShader(light, shader);

		//do the draw call that renders the mesh into the screen
		rc->mesh->render(GL_TRIANGLES);

		shader->setUniform("u_ambient_light", vec3(0.0));
		shader->setUniform("u_emissive_factor", vec3(0.0));
	}
}

void SCN::Renderer::renderIrradianceProbe(sProbe& probe)
{
	Camera* camera = Camera::current;
	GFX::Shader* shader = GFX::Shader::Get("spherical_probe");
	shader->enable();

	Matrix44 model;
	model.setTranslation(probe.pos.x, probe.pos.y, probe.pos.z);
	model.scale(10, 10, 10);

	cameraToShader(camera, shader);
	shader->setUniform("u_model", model);
	shader->setUniform3Array("u_coeffs", (float*)probe.sh.coeffs, 9);

	sphere.render(GL_TRIANGLES);
}

void SCN::Renderer::captureIrradianceProbe(sProbe& probe)
{
	FloatImage images[6]; //here we will store the six views

	Camera* app_camera = Camera::current;
	Camera cam;
	//set the fov to 90 and the aspect to 1
	cam.setPerspective(90, 1, 0.1, app_camera->far_plane);

	eShaders state_shader = current_shader;
	current_shader = eShaders::sLIGHTS_MULTI;
	
	if (!irr_fbo)
	{
		irr_fbo = new GFX::FBO();
		irr_fbo->create(64, 64, 1, GL_RGB, GL_FLOAT);
	}

	for (int i = 0; i < 6; ++i) //for every cubemap face
	{
		//compute camera orientation using defined vectors
		vec3 eye = probe.pos;
		vec3 front = cubemapFaceNormals[i][2];
		vec3 center = probe.pos + front;
		vec3 up = cubemapFaceNormals[i][1];
		cam.lookAt(eye, center, up);
		cam.enable();

		//render the scene from this point of view
		irr_fbo->bind();
			glDisable(GL_BLEND);
			glEnable(GL_DEPTH_TEST);

			setupRenderFrame();

			renderByPriority(eRenderMode::LIGHTS);
		irr_fbo->unbind();

		//read the pixels back and store in a FloatImage
		images[i].fromTexture(irr_fbo->color_textures[0]);
	}

	//compute the coefficients given the six images
	probe.sh = computeSH(images);

	current_shader = state_shader;

}

void SCN::Renderer::captureIrradiance()
{

	//define the corners of the axis aligned grid
	//this can be done using the boundings of our scene
	vec3 start_pos(-300, 5, -400);
	vec3 end_pos(300, 150, 400);

	//define how many probes you want per dimension
	vec3 dim(10, 4, 10);

	//compute the vector from one corner to the other
	vec3 delta = (end_pos - start_pos);

	//and scale it down according to the subdivisions
	//we substract one to be sure the last probe is at end pos
	delta.x /= (dim.x - 1);
	delta.y /= (dim.y - 1);
	delta.z /= (dim.z - 1);

	probes.resize(dim.x * dim.y * dim.z);

	//now delta give us the distance between probes in every axis
	//lets compute the centers
	//pay attention at the order at which we add them
	for (int z = 0; z < dim.z; ++z)
		for (int y = 0; y < dim.y; ++y)
			for (int x = 0; x < dim.x; ++x)
			{
				int index = x + y * dim.x + z * dim.x * dim.y;
				sProbe& p = probes[index];

				p.local.set(x, y, z);

				//index in the linear array
				p.index = index;

				//and its position
				p.pos = start_pos + delta * vec3(x, y, z);
			}

	bool last_state = show_probes;

	show_probes = false;
	//now compute the coeffs for every probe
	for (int iP = 0; iP < probes.size(); ++iP)
	{
		int probe_index = iP;
		sProbe& p = probes[iP];
		captureIrradianceProbe(p);
	}
	show_probes = last_state;

	irradiance_cache_info.dims = dim;
	//irradiance_cache_info.num_probes = 

	FILE* f = fopen("irradiance_cache.bin", "wb");
	if (f == NULL)
		return;

	irradiance_cache_info.dims = dim;
	irradiance_cache_info.start = start_pos;
	irradiance_cache_info.end = end_pos;
	irradiance_cache_info.num_probes = probes.size();
	//save data
	fwrite(&irradiance_cache_info, sizeof(irradiance_cache_info), 1, f);
	fwrite(&probes[0], sizeof(sProbe), probes.size(), f);
	fclose(f);

	uploadIrradianceCache();
}


void SCN::Renderer::uploadIrradianceCache()
{
	if (probes_texture)
		delete probes_texture;

	vec3 dim = irradiance_cache_info.dims;

	//create the texture to store the probes (do this ONCE!!!)
	probes_texture = new GFX::Texture(
		9, //9 coefficients per probe
		probes.size(), //as many rows as probes
		GL_RGB, //3 channels per coefficient
		GL_FLOAT); //they require a high range

	//we must create the color information for the texture. because every SH are 27 floats in the RGB,RGB,... order, we can create an array of SphericalHarmonics and use it as pixels of the texture
	SphericalHarmonics* sh_data = NULL;
	sh_data = new SphericalHarmonics[dim.x * dim.y * dim.z];

	//here we fill the data of the array with our probes in x,y,z order
	for (int i = 0; i < probes.size(); ++i)
		sh_data[i] = probes[i].sh;

	//now upload the data to the GPU as a texture
	probes_texture->upload(GL_RGB, GL_FLOAT, false, (uint8*)sh_data);

	//disable any texture filtering when reading
	probes_texture->bind();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	//always free memory after allocating it!!!
	delete[] sh_data;

}

void SCN::Renderer::applyIrradiance()
{
	if (!probes_texture)
		return;

	Camera* camera = Camera::current;
	GFX::Shader* shader = GFX::Shader::Get("irradiance");

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	shader->enable();
	bufferToShader(shader);
	shader->setMatrix44("u_ivp", camera->inverse_viewprojection_matrix);
	shader->setUniform("u_iRes", vec2(1.0 / gbuffers_fbo->width, 1.0 / gbuffers_fbo->height));
	shader->setUniform("u_camera_position", camera->eye);

	shader->setUniform("u_irr_start", irradiance_cache_info.start);
	shader->setUniform("u_irr_end", irradiance_cache_info.end);
	shader->setUniform("u_irr_dims", irradiance_cache_info.dims);
	shader->setUniform("u_num_probes", irradiance_cache_info.num_probes);
	shader->setTexture("u_probes_texture", probes_texture, 5);

	shader->setUniform("u_irr_normal_distance", 5.0f);
	shader->setUniform("u_trilinear_interpolation", enable_trilinear_interpolation);

	//compute the vector from one corner to the other
	vec3 delta = (irradiance_cache_info.end - irradiance_cache_info.start);

	//and scale it down according to the subdivisions
	//we substract one to be sure the last probe is at end pos
	delta.x /= (irradiance_cache_info.dims.x - 1);
	delta.y /= (irradiance_cache_info.dims.y - 1);
	delta.z /= (irradiance_cache_info.dims.z - 1);

	shader->setUniform("u_irr_delta", delta);

	shader->setUniform("u_irr_multiplier", irradiance_multiplier);

	quad->render(GL_TRIANGLES);
}

void SCN::Renderer::loadIrradianceCache()
{
	FILE* f = fopen("irradiance_cache.bin", "rb");
	if (f == NULL)
		return;

	//load data
	fread(&irradiance_cache_info, sizeof(irradiance_cache_info), 1, f);
	probes.resize(irradiance_cache_info.num_probes);
	fread(&probes[0], sizeof(sProbe), irradiance_cache_info.num_probes, f);
	fclose(f);

	uploadIrradianceCache();
}

void SCN::Renderer::captureReflection()
{
	if (!reflections_fbo)
		reflections_fbo = new GFX::FBO();

	Camera camera;
	camera.setPerspective(90, 1, 0.1, 1000);


	//define the corners of the axis aligned grid
	//this can be done using the boundings of our scene
	vec3 start_pos(-300, 50, -400);
	vec3 end_pos(300, 150, 400);

	//define how many probes you want per dimension
	vec3 dim(3, 2, 4);

	//compute the vector from one corner to the other
	vec3 delta = (end_pos - start_pos);

	//and scale it down according to the subdivisions
	//we substract one to be sure the last probe is at end pos
	delta.x /= (dim.x - 1);
	delta.y /= (dim.y - 1);
	delta.z /= (dim.z - 1);

	reflection_probes.resize(dim.x * dim.y * dim.z);

	//now delta give us the distance between probes in every axis
	//lets compute the centers
	//pay attention at the order at which we add them
	for (int z = 0; z < dim.z; ++z)
		for (int y = 0; y < dim.y; ++y)
			for (int x = 0; x < dim.x; ++x)
			{
				sReflectionProbe p;
				p.cubemap = new GFX::Texture();
				p.cubemap->createCubemap(
					256, 256, 	//size
					nullptr, 	//data
					GL_RGB, GL_FLOAT);	//mipmaps
				
				p.pos = start_pos + delta * vec3(x, y, z);

				//render the view from every side
				for (int i = 0; i < 6; ++i)
				{
					//assign cubemap face to FBO
					reflections_fbo->setTexture(p.cubemap, i);

					vec3 eye = p.pos;
					vec3 center = p.pos + cubemapFaceNormals[i][2];
					vec3 up = cubemapFaceNormals[i][1];
					camera.lookAt(eye, center, up);
					camera.enable();

					reflections_fbo->bind();

					setupRenderFrame();

					renderByPriority(eRenderMode::LIGHTS);	//TODO: check mode

					reflections_fbo->unbind();
				}
				//generate the mipmaps
				glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
				p.cubemap->generateMipmaps();
				reflection_probes.push_back(p);

			}
}

void SCN::Renderer::renderReflectionProbe(sReflectionProbe& probe)
{
	GFX::Texture* texture = probe.cubemap ? probe.cubemap : skybox_cubemap;
	Camera* camera = Camera::current;
	GFX::Shader* shader = GFX::Shader::Get("reflectionProbe");
	shader->enable();

	cameraToShader(camera, shader);
	Matrix44 model;
	model.setTranslation(probe.pos.x, probe.pos.y, probe.pos.z);
	model.scale(10, 10, 10);
	shader->setUniform("u_model", model);
	shader->setUniform("u_texture", texture , 0);
	sphere.render(GL_TRIANGLES);
}

sReflectionProbe* SCN::Renderer::getClosestReflectionProbe(Matrix44 model)
{
	int closest_index = -1; 
	float min_dist = INFINITY;
	for (int i = 0; i < reflection_probes.size(); i++)
	{
		vec3 aux = reflection_probes[i].pos - model.getTranslation();
		float length = aux.length();

		closest_index = (length < min_dist) ? i : closest_index;
	}	
	if (closest_index != -1)
		return &reflection_probes[closest_index];
	
	else
		return nullptr;
}

void SCN::Renderer::capturePlanerReflection(Camera* camera)
{

	static Camera simetric_camera;
	vec2 size = CORE::getWindowSize();

	//render planer reflection
	if (!planer_reflection_fbo)		//TODO: put it somewhere else ?
	{
		planer_reflection_fbo = new GFX::FBO();
		planer_reflection_fbo->create(size.x, size.y, 1, GL_RGBA, GL_FLOAT);
	}

	simetric_camera = *camera;
	vec3 pos = camera->eye;
	pos.y *= -1;
	vec3 target = camera->center;
	target.y *= -1;
	simetric_camera.lookAt(pos, target, camera->up * -1.0f);

	simetric_camera.enable();

	planer_reflection_fbo->bind();
		
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);

		setupRenderFrame();

		renderByPriority(SCN::eRenderMode::FLAT);

	planer_reflection_fbo->unbind();
}

void SCN::Renderer::renderPlanerReflectionFBO(Camera* camera)
{
	vec2 size = CORE::getWindowSize();

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	GFX::Texture* reflection = planer_reflection_fbo->color_textures[0];

	GFX::Shader* shader = GFX::Shader::Get("mirror");
	shader->enable();
	cameraToShader(camera, shader);
	Matrix44 model;
	shader->setUniform("u_model", model);
	shader->setUniform("u_iRes", vec2(1.0 / size.x, 1.0 / size.y));
	shader->setUniform("u_texture", reflection, 0);
	shader->setUniform("u_apply_fresnel", use_fresnel_planer_reflection);

	plane.render(GL_TRIANGLES);
}

void SCN::Renderer::generateReflectionDeferred(Camera* camera)
{
	glClearColor(0.0, 0.0, 0.0, 0.0);	//TODO: check
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);


	GFX::Shader* shader = GFX::Shader::Get("ambient_refelctions");
	shader->enable();

	bufferToShader(shader);
	shader->setMatrix44("u_ivp", camera->inverse_viewprojection_matrix);
	shader->setUniform("u_iRes", vec2(1.0 / deferred_reflections_fbo->color_textures[0]->width, 1.0 / deferred_reflections_fbo->color_textures[0]->height));
	shader->setTexture("u_environment_texture", skybox_cubemap, 9);
	shader->setUniform("u_camera_position", camera->eye);
	quad->render(GL_TRIANGLES);

	glEnable(GL_DEPTH_TEST);
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
		static int priority_current = current_priority;
		ImGui::Combo("Priority", &priority_current, priority, IM_ARRAYSIZE(priority), 3);

		if (priority_current == 0)
		{
			enable_dithering = true;
			current_priority = eRenderPriority::NOPRIORITY;
		}
		if (priority_current == 1) current_priority = eRenderPriority::ALPHA1;
		if (priority_current == 2) current_priority = eRenderPriority::DISTANCE2CAMERA;

		ImGui::TreePop();
	}

	//RENDER MODE
	if (ImGui::TreeNode("Rendering Mode"))
	{
		const char* mode[] = { "Flat", "Lights", "Deferred" };
		static int mode_current = current_mode;
		ImGui::Combo("RenderMode",  &mode_current, mode, IM_ARRAYSIZE(mode), 2);

		if (mode_current == 0)
		{
			current_mode = eRenderMode::FLAT;
			current_shader = eShaders::sTEXTURE;

			if (ImGui::TreeNode("Available Shaders"))
			{
				const char* shaders[] = { "Flat", "Texture", "Texture++" };
				static int shader_current = current_shader;
				ImGui::Combo("Shader", &shader_current, shaders, IM_ARRAYSIZE(shaders), 3);

				if (shader_current == 0) current_shader = eShaders::sFLAT;
				if (shader_current == 1) current_shader = eShaders::sTEXTURE;
				if (shader_current == 2)
				{
					current_shader = eShaders::sTEXTURE_IMPROVED;
					ImGui::Checkbox("Enable reflections", &enable_reflections);
					ImGui::SliderFloat("Reflection intensity ", &reflections_factor, 0.0, 1.0);
					ImGui::Checkbox("Enable fresnel", &enable_fresnel);
				}
				ImGui::TreePop();
			}
		}
		if (mode_current == 1)
		{
			current_mode = eRenderMode::LIGHTS;
			current_shader = eShaders::sLIGHTS_MULTI;
			if (ImGui::TreeNode("Available Shaders"))
			{
				static int shader_current = current_shader;
				ImGui::Combo("Shader", &shader_current, "MULTI\0SINGLE\0PBR\0");

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

				if (shader_current == 2)
				{
					current_shader = eShaders::sLIGHTS_PBR;
					current_lights_render = eLightsRender::MULTIPASS;
					enable_specular = true;
				}


				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Rendering parameters"))
			{
				if (current_shader != eShaders::sLIGHTS_PBR)
					ImGui::Checkbox("Enable Specular", &enable_specular);
				if (current_shader == eShaders::sLIGHTS_MULTI)
					ImGui::Checkbox("Show Shadowmaps", &show_shadowmaps);
				ImGui::Checkbox("Use normalmaps", &enable_normalmap);


				ImGui::TreePop();
			}

			if (current_shader != eShaders::sLIGHTS_SINGLE)
			{
				if (ImGui::TreeNode("Reflections"))
				{
					ImGui::Checkbox("Enable reflections", &enable_reflections);
					ImGui::Checkbox("Show reflection cache", &show_reflection_probes); 

					if (ImGui::Button("Update Reflections"))
						capture_reflectance = true;

					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Planer Reflections"))
				{
					ImGui::Checkbox("Show planer reflection ", &show_planer_reflection);
					ImGui::Checkbox("Enable fresnel", &use_fresnel_planer_reflection);

					ImGui::TreePop();
				}
			}
		}
		if (mode_current == 2)
		{
			current_mode = eRenderMode::DEFERRED;
			current_shader = eShaders::sDEFERRED;
			if (ImGui::TreeNode("Available shaders"))
			{
				static int shader_current = current_shader;
				ImGui::Combo("Shader", &shader_current, "PHONG\0PBR\0");
				if (shader_current == 0) current_shader = eShaders::sDEFERRED;
				if (shader_current == 1) current_shader = eShaders::sDEFERRED_PBR;
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Rendering parameters"))
			{
				if (current_shader == eShaders::sDEFERRED) ImGui::Checkbox("Enable specular", &enable_specular);

				ImGui::Checkbox("Use normalmaps", &enable_normalmap);

				ImGui::Checkbox("Show shadowmaps", &show_shadowmaps);

				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Deferred Parameters"))
			{
				ImGui::Checkbox("Show Buffers", &show_buffers);
				ImGui::Checkbox("Show Global Pos", &show_globalpos);
				if (current_priority != eRenderPriority::NOPRIORITY)
					ImGui::Checkbox("Dithering", &enable_dithering); 
				ImGui::Checkbox("Add SSA", &add_SSAO);
				ImGui::SliderFloat("SSAO radius", &ssao_radius, 0.0, 50);
				ImGui::SliderFloat("SSAO control factor", &control_SSAO_factor, 0.0, 50);

				ImGui::Checkbox("Show SSAO fbo", &show_ssao);

				

				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Irradiance"))
			{
				ImGui::Checkbox("Render with irradiance", &enable_irradiance);
				ImGui::Checkbox("Trilinear Interpolation", &enable_trilinear_interpolation);

				if (ImGui::Button("Update Probes"))
					capture_irradiance = true;
				ImGui::SameLine();
				if (ImGui::Button("Load Probes"))
					loadIrradianceCache();
				ImGui::Checkbox("Show irradiance cache", &show_probes);

				ImGui::SliderFloat("Irradiance multiplier", &irradiance_multiplier, 0.0, 10.0);

				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Reflections"))
			{
				ImGui::Checkbox("Enable reflections", &enable_reflections);
				ImGui::Checkbox("Show reflection fbo", &show_reflection_fbo);

				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Volumetric"))
			{
				ImGui::DragFloat("Air density", &air_density, 0.0001, 0.0, 0.1);
				ImGui::Checkbox("Use constant density", &constant_density);
				ImGui::Checkbox("Show volumetric fbo", &show_volumetric);

				ImGui::TreePop();
			}

			if (ImGui::TreeNode("PostFX"))
			{
				if(ImGui::TreeNode("Color Correction"))
				{
					ImGui::Checkbox("Enable Color Correction", &enable_color_correction);
					if (enable_color_correction)
					{
						ImGui::SliderFloat("Brightness", &fx_brightness, 0.0, 5.0);
						ImGui::SliderFloat("Contrast", &fx_contrast, 0.5, 1.5);
						ImGui::SliderFloat3("Midtone", &fx_midtone.x, 0.0, 1.0);
						ImGui::SliderFloat("Red Balance", &fx_red_balance, 0.0, 5.0);
						ImGui::SliderFloat("Green Balance", &fx_green_balance, 0.0, 5.0);
						ImGui::SliderFloat("Blue Balance", &fx_blue_balance, 0.0, 5.0);
					}
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Vigneting"))
				{
					ImGui::Checkbox("Enable Vigneting", &enable_vigneting);
					if(enable_vigneting)
						ImGui::SliderFloat("Vigneting", &fx_vigneting, 0.0, 5.0);
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Grain"))
				{
					ImGui::Checkbox("Enable Grain", &enable_grain);
					if (enable_grain)
						ImGui::SliderFloat("Grain Intensity", &fx_grain, 0.0, 5.0);
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Lens Distortion"))
				{
					ImGui::Checkbox("Enable Distortion", &enable_lens_distortion);
					if (enable_lens_distortion)
					{
						static int current = current_distortion;
						ImGui::Combo("Type", &(int)current, "PINCUSHION\0BARREL\0CHROMATIC\0");
						if (current == 0) current_distortion = eDistortionType::PINCUSHION;
						if (current == 1) current_distortion = eDistortionType::BARREL;
						if (current == 2) current_distortion = eDistortionType::CHROMATIC;
						if (current_distortion != eDistortionType::CHROMATIC) ImGui::SliderFloat("Distorion Intensity", &fx_distortion, 0.0, 5.0);
					}
					ImGui::TreePop();
				}
				if (ImGui::TreeNode("Motion Blur"))
				{
					ImGui::Checkbox("Enable Motion Blur", &enable_motion_blur);
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Depth of Field"))
				{
					ImGui::Checkbox("Enable DoF", &enable_DoF);
					if (enable_DoF)
					{
						ImGui::SliderFloat("DoF Focus", &fx_focal_distance, 0.0, 1.0);
						ImGui::SliderFloat("DoF Min", &fx_dof_min, 0.0, 1.0);
						ImGui::SliderFloat("DoF Max", &fx_dof_max, 0.0, 1.0);
					}
					ImGui::TreePop();
				}
				if (ImGui::TreeNode("Bloom"))
				{
					ImGui::Checkbox("Enable Blur", &enable_blur);
					ImGui::Checkbox("Enable Bloom", &enable_bloom);
					
					if (enable_bloom)
					{
						enable_blur = false;
						static int current = current_bloom;
						ImGui::Combo("Type", &(int)current, "SIMPLE\0ADVANCED\0");
						if (current == 0) current_bloom = eBloomType::SIMPLE;
						if (current == 1)
						{
							current_bloom = eBloomType::ADVANCED;
							ImGui::SliderInt("Downsample iterations", &fx_downsample_iter, 1, 10);
						}
					}
					if (enable_blur)
						enable_bloom = false;

					if (enable_bloom || enable_blur)
					{
						ImGui::SliderFloat("Blur Intensity", &fx_blur_intensity, 0.0, 5.0);
						ImGui::SliderInt("Num iterations", &fx_blur_num_iter, 1, 10);
					}
					ImGui::TreePop();
				}

				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Tonemapper Parameters"))
	{
		ImGui::Checkbox("Enable Tonemapper", &enable_tonemapper);
		if (enable_tonemapper)
		{
			const char* tonemappers[] = { "Simple", "Uncharted" };
			ImGui::Combo("Tonemappers", &current_tonemapper, tonemappers, IM_ARRAYSIZE(tonemappers), 2);

			if (current_tonemapper == 0)
			{
				ImGui::SliderFloat("Scale", &tonemapper_scale, 0.0, 2.0);
				ImGui::SliderFloat("Average Lum", &tonemapper_avg_lum, 0.0, 2.0);
				ImGui::SliderFloat("Lum White", &tonemapper_lumwhite, 0.0, 2.0);
				//ImGui::SliderFloat("Gamma", &gamma, 0.0, 2.0);
			}
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
		if (camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize) || capture_reflectance)
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
		if (camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize) || capture_reflectance)
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

void Renderer::renderByPriority(eRenderMode mode)
{
	switch (current_priority)
	{
	case(eRenderPriority::NOPRIORITY):
	{
		for (int i = 0; i < render_calls.size(); ++i)
		{
			RenderCall rc = render_calls[i];
			renderRenderCalls(&rc, mode);
		}
		break;
	};
	case(eRenderPriority::ALPHA1):
	{
		//render opaque entities
		for (int i = 0; i < render_calls_opaque.size(); ++i)
		{
			RenderCall rc = render_calls_opaque[i];
			renderRenderCalls(&rc, mode);
		}

		//render transparent entities 
		for (int i = 0; i < render_calls.size(); ++i)
		{
			RenderCall rc = render_calls[i];
			renderRenderCalls(&rc, mode);
		}
		break;
	}
	case(eRenderPriority::DISTANCE2CAMERA):
	{
		//render opaque entities
		for (int i = 0; i < render_calls_opaque.size(); ++i)
		{
			RenderCall rc = render_calls_opaque[i];
			renderRenderCalls(&rc, mode);
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

		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		camera->enable();

		glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		renderByPriority(eRenderMode::SHADOWMAP);
		
		light->shadowmap_fbo->unbind();

		light->shadow_viewproj = camera->viewprojection_matrix;
	}
	GFX::endGPULabel();
}

std::vector<vec3> SCN::Renderer::generateSpherePoints(int num, float radius, bool hemi)
{
	std::vector<vec3> points;
	points.resize(num);
	for (int i = 0; i < num; i += 1)
	{
		vec3& p = points[i];
		float u = random();
		float v = random();
		float theta = u * 2.0 * PI;
		float phi = acos(2.0 * v - 1.0);
		float r = cbrt(random() * 0.9 + 0.1) * radius;
		float sinTheta = sin(theta);
		float cosTheta = cos(theta);
		float sinPhi = sin(phi);
		float cosPhi = cos(phi);
		p.x = r * sinPhi * cosTheta;
		p.y = r * sinPhi * sinTheta;
		p.z = r * cosPhi;
		if (hemi && p.z < 0)
			p.z *= -1.0;
	}
	return points;

}

void Renderer::renderRenderCalls(RenderCall* rc, eRenderMode mode)
{
	if (rc->mesh && rc->material)
	{
		if(render_boundaries)
			rc->mesh->renderBounding(rc->model, true);

		mode = (mode == eRenderMode::NULLMODE) ? current_mode : mode;
		switch (mode)
		{
		case (eRenderMode::FLAT):
		{
			renderMeshWithMaterial(rc);
			break;
		}
		case (eRenderMode::LIGHTS):
		{
			renderMeshWithMaterialLight(rc);
			break;
		}
		case (eRenderMode::DEFERRED):
		{
			renderDeferredGBuffers(rc);
			break;
		}
		case(eRenderMode::SHADOWMAP):
			renderMeshWithMaterialFlat(rc);
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

void SCN::Renderer::renderPostFX(GFX::Texture* color_buffer, GFX::Texture* depth_buffer)
{
	if (!postFX_bufferIN || CORE::BaseApplication::instance->window_resized)
	{
		postFX_bufferIN = new GFX::FBO();
		postFX_bufferIN->create(color_buffer->width, color_buffer->height, 1, GL_RGB, GL_HALF_FLOAT);
	}

	if (!postFX_bufferOUT || CORE::BaseApplication::instance->window_resized)
	{
		postFX_bufferOUT = new GFX::FBO();
		postFX_bufferOUT->create(color_buffer->width, color_buffer->height, 1, GL_RGB, GL_HALF_FLOAT);
	}

	if (!postFX_bufferTEMP || CORE::BaseApplication::instance->window_resized)
	{
		postFX_bufferTEMP = new GFX::FBO();
		postFX_bufferTEMP->create(color_buffer->width, color_buffer->height, 1, GL_RGB, GL_HALF_FLOAT);
	}

	if (!TEMP || CORE::BaseApplication::instance->window_resized)
	{
		TEMP = new GFX::FBO();
		TEMP->create(color_buffer->width, color_buffer->height, 1, GL_RGB, GL_HALF_FLOAT);
	}

	GFX::Shader* shader = nullptr;
	postFX_bufferIN->bind();
		color_buffer->toViewport();	
	postFX_bufferIN->unbind();

	if (enable_color_correction)
		renderColorCorrection(shader);
	
	if (enable_vigneting)
		renderVigneting(shader);

	if (enable_grain)
		renderGrain(shader);

	if (enable_lens_distortion)
		renderLensDistortion(shader);

	if (enable_motion_blur)
		renderMotionBlur(shader, depth_buffer);

	if (enable_DoF)
		renderDoF(shader, depth_buffer);

	if (enable_blur)
		renderBlur(shader, postFX_bufferIN);

	if (enable_bloom)
	{
		postFX_bufferTEMP->bind();
			postFX_bufferIN->color_textures[0]->toViewport();
		postFX_bufferTEMP->unbind();

		
		if (current_bloom == eBloomType::SIMPLE)
		{
			renderBlur(shader, postFX_bufferIN);

			postFX_bufferIN->bind();
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				postFX_bufferTEMP->color_textures[0]->toViewport();
			postFX_bufferIN->unbind();
			glDisable(GL_BLEND);
		}

		else
		{
			renderBloomAdvanced(shader);
		}
	}

	if (enable_tonemapper)
		renderTonemapper(postFX_bufferIN->color_textures[0]);
	else
		renderGamma(postFX_bufferIN->color_textures[0]);
}

void SCN::Renderer::renderColorCorrection(GFX::Shader* shader)
{
	postFX_bufferOUT->bind();
		shader = GFX::Shader::Get("fx_color_correction");
		shader->enable();
		shader->setUniform("u_brightness", fx_brightness);
		shader->setUniform("u_contrast", fx_contrast);
		shader->setVector3("u_midtone", fx_midtone);
		shader->setUniform("u_r_balance", fx_red_balance);
		shader->setUniform("u_g_balance", fx_green_balance);
		shader->setUniform("u_b_balance", fx_blue_balance);
		postFX_bufferIN->color_textures[0]->toViewport(shader);
	postFX_bufferOUT->unbind();

	std::swap(postFX_bufferIN, postFX_bufferOUT);
}

void SCN::Renderer::renderVigneting(GFX::Shader* shader)
{
	postFX_bufferOUT->bind();
		shader = GFX::Shader::Get("fx_vigneting");
		shader->enable();
		shader->setUniform("u_vigneting", fx_vigneting);
		postFX_bufferIN->color_textures[0]->toViewport(shader);
	postFX_bufferOUT->unbind();

	std::swap(postFX_bufferIN, postFX_bufferOUT);
}

void SCN::Renderer::renderGrain(GFX::Shader* shader)
{
	postFX_bufferOUT->bind();
		shader = GFX::Shader::Get("fx_grain");
		shader->enable();
		shader->setUniform("u_grain", fx_grain);
		postFX_bufferIN->color_textures[0]->toViewport(shader);
	postFX_bufferOUT->unbind();

	std::swap(postFX_bufferIN, postFX_bufferOUT);
}

void SCN::Renderer::renderLensDistortion(GFX::Shader* shader)
{
	postFX_bufferOUT->bind();
		float width = postFX_bufferIN->color_textures[0]->width;
		float height = postFX_bufferIN->color_textures[0]->height;
		if (current_distortion != eDistortionType::CHROMATIC)
		{
			shader = GFX::Shader::Get("fx_lens_distortion");
			shader->enable();
			shader->setUniform("u_center", vec2((width/2)/width, (height/2)/height));
			shader->setUniform("u_iRes", vec2(1.0 / width, 1.0 / height));
			shader->setUniform("u_type", (int)current_distortion);
			shader->setUniform("u_distortion", fx_distortion);
		}
		else 
		{
			shader = GFX::Shader::Get("fx_lens_chromatic");
			shader->enable();
			shader->setUniform("u_resolution", vec2(width, height));
		}
		postFX_bufferIN->color_textures[0]->toViewport(shader);
	postFX_bufferOUT->unbind();

	std::swap(postFX_bufferIN, postFX_bufferOUT);
}

void SCN::Renderer::renderMotionBlur(GFX::Shader* shader, GFX::Texture* depth_buffer)
{
	Camera* camera = Camera::current;
	postFX_bufferOUT->bind();
		shader = GFX::Shader::Get("fx_motion_blur");
		shader->enable();
		shader->setUniform("u_depth_texture", depth_buffer, 1);
		shader->setUniform("u_iRes", vec2(1.0 / postFX_bufferIN->color_textures[0]->width, 1.0 / postFX_bufferIN->color_textures[0]->height));
		shader->setUniform("u_ivp", camera->inverse_viewprojection_matrix);
		shader->setUniform("u_prev_vp", prev_view_proj);
		postFX_bufferIN->color_textures[0]->toViewport(shader);
	postFX_bufferOUT->unbind();

	std::swap(postFX_bufferIN, postFX_bufferOUT);

	prev_view_proj = camera->viewprojection_matrix;
}

void SCN::Renderer::renderDoF(GFX::Shader* shader, GFX::Texture* depth_buffer)
{
	float width = postFX_bufferIN->color_textures[0]->width;
	float height = postFX_bufferIN->color_textures[0]->height;
	if (!dof_fbo || CORE::BaseApplication::instance->window_resized)
	{
		dof_fbo = new GFX::FBO();
		dof_fbo->create(width, height, 1, GL_RGB, GL_HALF_FLOAT, false);
	}

	dof_fbo->bind();
		postFX_bufferIN->color_textures[0]->toViewport();
	dof_fbo->unbind();

	//BLUR THE IMAGE
	fx_blur_intensity = 1.0f;
	renderBlur(shader, dof_fbo);

	Camera* camera = Camera::current;
	postFX_bufferOUT->bind();
		shader = GFX::Shader::Get("fx_dof");
		shader->enable();
		shader->setUniform("u_outFocus_texture", dof_fbo->color_textures[0], 1);
		shader->setUniform("u_depth_texture", depth_buffer, 2);
		shader->setUniform("u_focal_distance", fx_focal_distance);
		shader->setUniform("u_min_distance", fx_dof_min);
		shader->setUniform("u_max_distance", fx_dof_max);
		shader->setUniform("u_camera_nearfar", vec2(camera->near_plane, camera->far_plane));
		postFX_bufferIN->color_textures[0]->toViewport(shader);
	postFX_bufferOUT->unbind();

	std::swap(postFX_bufferIN, postFX_bufferOUT);

}

void SCN::Renderer::renderBlur(GFX::Shader* shader, GFX::FBO* bufferIN)
{
	int power = 1;
	for (int i = 0; i < fx_blur_num_iter; ++i)
	{
		//Horizontal blur
		postFX_bufferOUT->bind();
			shader = GFX::Shader::Get("fx_blur");
			shader->enable();
			shader->setUniform("u_intensity", fx_blur_intensity);
			shader->setUniform("u_offset", vec2(1.0f / bufferIN->color_textures[0]->width, 0.0) * (float)power);
			bufferIN->color_textures[0]->toViewport(shader);
		postFX_bufferOUT->unbind();

		std::swap(bufferIN, postFX_bufferOUT);

		//Vertical blur
		postFX_bufferOUT->bind();
			shader->enable();
			shader->setUniform("u_offset", vec2(0.0, 1.0f / bufferIN->color_textures[0]->height) * (float)power);
			bufferIN->color_textures[0]->toViewport(shader);
		postFX_bufferOUT->unbind();

		std::swap(bufferIN, postFX_bufferOUT);
		power = power << 1;
	}
}

void SCN::Renderer::renderDownsample(GFX::Shader* shader)
{
	float width = postFX_bufferIN->color_textures[0]->width;
	float height = postFX_bufferIN->color_textures[0]->height;
	for (int i = 0; i < fx_downsample_iter; ++i)
	{
		width /= 2;
		height /= 2;

		if (height < 2 || width < 2)
			break;
		GFX::FBO* fbo = bloom_fbo[i];
		if (!fbo|| CORE::BaseApplication::instance->window_resized)
		{
			fbo = bloom_fbo[i] = new GFX::FBO();
			fbo->create(width, height, 1, GL_RGB, GL_HALF_FLOAT, false);
		}

		fbo->bind();
			postFX_bufferIN->color_textures[0]->toViewport();
		fbo->unbind();

		std::swap(postFX_bufferIN, postFX_bufferOUT);

	}
}

void SCN::Renderer::renderBloomAdvanced(GFX::Shader* shader)
{
	//DOWNSAMPLE THE IMAGE
	renderDownsample(shader);
	
	for (int i = 0; i < fx_downsample_iter; ++i)
	{
		//BLUR THE DOWNSAMPLED IMAGES
		renderBlur(shader, bloom_fbo[i]);
	}

	postFX_bufferOUT->bind();
		
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		//FROM THE SMALLEST TO THE GREATEST UPSAMPLE ADDING THEM
		for (int i = fx_downsample_iter; i > 0; --i)
		{
			bloom_fbo[i-1]->color_textures[0]->toViewport();
		}

		//RENDER THE SCENE ADDED TO THE PREVIOUS ONES
		postFX_bufferIN->color_textures[0]->toViewport();
	postFX_bufferOUT->unbind();
	glDisable(GL_BLEND);

	std::swap(postFX_bufferIN, postFX_bufferOUT);

}

void SCN::Renderer::renderTonemapper(GFX::Texture* color_buffer)
{
	GFX::Shader* shader;
	//TONEMAPPER
	if (current_tonemapper == 0)
	{
		shader = GFX::Shader::Get("tonemapper");
		shader->enable();
		shader->setUniform("u_scale", tonemapper_scale);
		shader->setUniform("u_average_lum", tonemapper_avg_lum);
		shader->setUniform("u_lumwhite2", tonemapper_lumwhite);
		shader->setUniform("u_igamma", 1.0f / gamma);
	}
	else {
		shader = GFX::Shader::Get("uncharted_tonemapper");
		shader->enable();
	}

	color_buffer->toViewport(shader);
	//reflections_fbo->color_textures[0]->toViewport(shader);
	shader->disable();
}

void SCN::Renderer::renderGamma(GFX::Texture* color_buffer)
{
	GFX::Shader* shader = GFX::Shader::Get("gamma");
	shader->enable();
	shader->setUniform("u_igamma", 1.0f / gamma);
	color_buffer->toViewport(shader);
}
