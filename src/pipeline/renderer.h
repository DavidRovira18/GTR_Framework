#pragma once
#include "scene.h"
#include "prefab.h"
#include "../gfx/sphericalharmonics.h"

#include "light.h"

//forward declarations
class Camera;
class Skeleton;
namespace GFX {
	class Shader;
	class Mesh;
	class FBO;
}

//struct to store probes
struct sProbe {
	vec3 pos; //where is located
	vec3 local; //its ijk pos in the matrix
	int index; //its index in the linear array
	SphericalHarmonics sh; //coeffs
};

namespace SCN {

	class Prefab;
	class Material;

	//STRUCT TO STORE IMPORTANT INFO FOR DRAW CALLS
	struct RenderCall {
	public:
		GFX::Mesh* mesh;
		Material* material;
		Matrix44 model;
		BoundingBox bounding;

		float distance_2_camera;
	};

	enum eRenderMode {
		FLAT,
		LIGHTS,
		DEFERRED
	};

	enum eLightsRender {
		MULTIPASS,
		SINGLEPASS,
		MULTIPASS_TRANSPARENCIES
	};

	enum eShaders {
		sFLAT,
		sTEXTURE,
		sTEXTURE_IMPROVED,
		sLIGHTS_MULTI,
		sLIGHTS_SINGLE,
		sLIGHTS_PBR,
		sDEFERRED,
		sDEFERRED_PBR
	};

	enum eRenderPriority {
		NOPRIORITY,
		ALPHA1,
		DISTANCE2CAMERA
	};

	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{
	public:
		bool render_wireframe;
		bool render_boundaries;


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

		//SSAO
		GFX::FBO* ssao_fbo = nullptr;
		GFX::Texture* ssao_blur = nullptr;

		//iradiance
		GFX::FBO* irr_fbo = nullptr;
		std::vector<sProbe> probes;
		GFX::Texture* probes_texture = nullptr;

		std::vector<Vector3f> random_points;
		float ssao_radius = 5.0f;
		bool show_ssao = false;
		bool add_SSAO = true;
		float control_SSAO_factor = 3.0f;

		bool generate_gbuffers = false;
		bool show_buffers = false;
		bool show_globalpos = false;
		bool enable_dithering = true;

		bool generate_shadowmap = false;
		bool show_shadowmaps = false;

		bool enable_specular = false;
		bool enable_normalmap = false;

		bool enable_reflections = false;
		float reflections_factor = 0.0f;
		bool enable_fresnel = false;


		//TONEMAPPER
		bool enable_tonemapper;
		int current_tonemapper;
		float tonemapper_scale;
		float tonemapper_avg_lum;
		float tonemapper_lumwhite;
		float gamma;



		GFX::Texture* skybox_cubemap;

		SCN::Scene* scene;

		//updated every frame
		Renderer(const char* shaders_atlas_filename );

		//just to be sure we have everything ready for the rendering
		void setupScene(Camera* camera);
		void processLights();
		void processRenderCalls(Camera* camera);

		//add here your functions
		const char* getShader(eShaders current);

		//renders several elements of the scene
		void renderScene(SCN::Scene* scene, Camera* camera);

		void renderFrameForward(SCN::Scene* scene, Camera* camera);
		void renderFrameDeferred(SCN::Scene* scene, Camera* camera);

		void generateShadowMaps();
		std::vector<vec3> generateSpherePoints(int num, float radius, bool hemi);

		//render the skybox
		void renderSkybox(GFX::Texture* cubemap);
	
		//to render one node from the prefab and its children
		void renderNode(SCN::Node* node, Camera* camera);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(RenderCall* rc);

		void renderMeshWithMaterialFlat(RenderCall* rc);

		void renderMeshWithMaterialLight(RenderCall* rc);

		void renderMultipass(GFX::Shader* shader, RenderCall* rc);

		void renderSinglepass(GFX::Shader* shader, RenderCall* rc);

		void setVisibleLights(RenderCall* rc);

		void renderDeferredGBuffers(RenderCall* rc);
		void renderDeferred();
		void renderDeferredGlobal(GFX::Shader* shader);
		void renderDeferredGlobalPos(GFX::Shader* shader, Camera* camera);
		void renderDeferredLights(GFX::Shader* shader, Camera* camera);
		void renderDeferredDirectionalLights(GFX::Shader* shader, Camera* camera);
		void renderDeferredGeometryLights(GFX::Shader* shader, Camera* camera);
		void initDeferredFBOs();
		void generateSSAO(Camera* camera);
		void computeIlluminationDeferred();

		void renderTransparenciesForward();
		void renderMultipassTransparencies(GFX::Shader* shader, RenderCall* rc);

		void renderProbe(sProbe& probe);
		void captureProbe(sProbe& probe);
		void captureIrradiance();
		void uploadIrradianceCache();

		void showUI();

		void showGBuffers(vec2 window_size, Camera* camera);

		void cameraToShader(Camera* camera, GFX::Shader* shader); //sends camera uniforms to shader
		void lightToShader(LightEntity* light, GFX::Shader* shader); //sends light uniforms to shader
		void bufferToShader(GFX::Shader* shader);
		void materialToShader(GFX::Shader* shader, SCN::Material* material);

		void storeDrawCall(SCN::Node* node, Camera* camera);

		void storeDrawCallNoPriority(SCN::Node* node, Camera* camera);

		void prioritySwitch();

		void renderRenderCalls(RenderCall* rc);

		void renderShadowmaps();

		void renderTonemapper();
		void renderGamma();
	};

};