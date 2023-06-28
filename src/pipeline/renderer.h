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
		DEFERRED,
		SHADOWMAP = 90,
		NULLMODE = 100,
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

	struct sIrradianceHeader {
		int num_probes;
		vec3 dims;
		vec3 start;
		vec3 end;
	};

	//struct to store reflection probes info
	struct sReflectionProbe {
		vec3 pos;
		GFX::Texture* cubemap = nullptr;
	};


	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{
	public:
		bool render_wireframe;
		bool render_boundaries;


		//RENDERING MODE
		eRenderMode current_mode = eRenderMode::DEFERRED;
		eLightsRender current_lights_render = eLightsRender::MULTIPASS;
		//RENDER CALLS AND PRIORITY
		std::vector<RenderCall> render_calls;
		std::vector<RenderCall> render_calls_opaque;
		eRenderPriority current_priority = eRenderPriority::DISTANCE2CAMERA;
		//SHADER
		eShaders current_shader = eShaders::sDEFERRED;
		//LIGHTS
		std::vector<LightEntity*> lights;
		std::vector<LightEntity*> visible_lights;

		//DEFERRED FBOs
		GFX::FBO* gbuffers_fbo = nullptr;
		GFX::Texture* depth_buffer_clone = nullptr;		//for decals
		GFX::FBO* illumination_fbo = nullptr;

		//SSAO
		GFX::FBO* ssao_fbo = nullptr;
		GFX::Texture* ssao_blur = nullptr;
		std::vector<Vector3f> random_points;
		float ssao_radius = 5.0f;
		bool show_ssao = false;
		bool add_SSAO = true;
		float control_SSAO_factor = 3.0f;

		//iradiance
		GFX::FBO* irr_fbo = nullptr;
		std::vector<sProbe> probes;
		GFX::Texture* probes_texture = nullptr;
		bool capture_irradiance = false;
		bool show_probes = false;
		sIrradianceHeader irradiance_cache_info;
		float irradiance_multiplier = 1.0f;
		bool enable_irradiance = false;

		//reflection
		GFX::FBO* reflections_fbo = nullptr;
		GFX::FBO* planer_reflection_fbo = nullptr;
		std::vector<sReflectionProbe> reflection_probes;
		bool show_reflection_probes = false;
		bool capture_reflectance = false;
		bool show_planer_reflection = false;
		bool use_fresnel_planer_reflection = false;
		GFX::FBO* deferred_reflections_fbo = nullptr;
		bool show_reflection_fbo = false;
		
		bool show_buffers = false;
		bool show_globalpos = false;
		bool enable_dithering = true;

		bool show_shadowmaps = false;

		bool enable_specular = false;
		bool enable_normalmap = false;

		bool enable_reflections = false;
		float reflections_factor = 0.0f;
		bool enable_fresnel = false;

		//volumetric render
		GFX::FBO* volumetric_fbo = nullptr;
		bool show_volumetric = false;
		float air_density;
		bool constant_density = true;

		//DECALS
		std::vector<DecalEntity*> decals;

		//POSTFX
		GFX::FBO* postFX_bufferIN = nullptr;
		GFX::FBO* postFX_bufferOUT = nullptr;
		GFX::FBO* postFX_bufferTEMP = nullptr;
			//ColorCorrection
			bool enable_color_correction = false;	

			float fx_brightness = 1.0f;
			float fx_contrast = 1.0f;
			vec3 fx_midtone = vec3(0.5);
			float fx_red_balance = 1.0f;
			float fx_green_balance = 1.0f;
			float fx_blue_balance = 1.0f;

			//Vigneting
			bool enable_vigneting = false;
			float fx_vigneting = 1.0f;

			//Grain
			bool enable_grain = false;
			float fx_grain = 1.0f;

			//MotionBlur
			bool enable_motion_blur = false;
			Matrix44 prev_view_proj;

			//Bloom
			bool enable_blur = false;
			bool enable_bloom = false;
			float fx_blur_intensity = 1.0;
			int fx_blur_num_iter = 1;
			enum eBloomType {
				SIMPLE,
				ADVANCED
			};
			eBloomType current_bloom = eBloomType::SIMPLE;
			int fx_downsample_iter = 4;
			GFX::FBO* bloom_fbo[10] = { nullptr };


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
		void processEntities();
		void processRenderCalls(Camera* camera);

		//add here your functions
		const char* getShader(eShaders current);

		//renders several elements of the scene
		void renderScene(SCN::Scene* scene, Camera* camera);

		void setupRenderFrame();

		void renderFrameForward(SCN::Scene* scene, Camera* camera);
		void renderFrameDeferred(SCN::Scene* scene, Camera* camera);

		void generateShadowMaps();
		std::vector<vec3> generateSpherePoints(int num, float radius, bool hemi);

		//render the skybox
		void renderSkybox(GFX::Texture* cubemap, float intensity);

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
		void createDecals(Camera* camera);
		void generateVolumetricAir(Camera* camera);
		void generateSSAO(Camera* camera);
		void computeIlluminationDeferred();

		void renderTransparenciesForward();
		void renderMultipassTransparencies(GFX::Shader* shader, RenderCall* rc);

		// irradiance
		void renderIrradianceProbe(sProbe& probe);
		void captureIrradianceProbe(sProbe& probe);
		void captureIrradiance();
		void uploadIrradianceCache();
		void applyIrradiance();
		void loadIrradianceCache();

		//reflections
		void captureReflection();
		void renderReflectionProbe(sReflectionProbe& probe);
		sReflectionProbe* getClosestReflectionProbe(Matrix44 model);
		void capturePlanerReflection(Camera* camera);
		void renderPlanerReflectionFBO(Camera* camera);
		void generateReflectionDeferred(Camera* camera);


		void showUI();

		void showGBuffers(vec2 window_size, Camera* camera);

		void cameraToShader(Camera* camera, GFX::Shader* shader); //sends camera uniforms to shader
		void lightToShader(LightEntity* light, GFX::Shader* shader); //sends light uniforms to shader
		void bufferToShader(GFX::Shader* shader);
		void materialToShader(GFX::Shader* shader, SCN::Material* material);

		void storeDrawCall(SCN::Node* node, Camera* camera);

		void storeDrawCallNoPriority(SCN::Node* node, Camera* camera);

		void renderByPriority(eRenderMode mode = eRenderMode::NULLMODE);

		void renderRenderCalls(RenderCall* rc, eRenderMode mode = eRenderMode::NULLMODE);

		void renderShadowmaps();

		//POSTFX
		void renderPostFX(GFX::Texture* color_buffer, GFX::Texture* depth_buffer);
		void renderColorCorrection(GFX::Shader* shader);
		void renderVigneting(GFX::Shader* shader);
		void renderGrain(GFX::Shader* shader);
		void renderMotionBlur(GFX::Shader* shader, GFX::Texture* depth_buffer);
		void renderBlur(GFX::Shader* shader);
		void renderDownsample(GFX::Shader* shader);
		void renderBloomAdvanced(GFX::Shader* shader);
		void renderTonemapper(GFX::Texture* color_buffer);
		void renderGamma(GFX::Texture* color_buffer);
	};

};