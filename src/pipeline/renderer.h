#pragma once
#include "scene.h"
#include "prefab.h"

#include "light.h"

//forward declarations
class Camera;
class Skeleton;
namespace GFX {
	class Shader;
	class Mesh;
	class FBO;
}

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

		//TONEMAPPER
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

		//add here your functions
		const char* getShader(eShaders current);

		//renders several elements of the scene
		void renderScene(SCN::Scene* scene, Camera* camera);

		void renderFrameForward(SCN::Scene* scene, Camera* camera);
		void renderFrameDeferred(SCN::Scene* scene, Camera* camera);

		void generateShadowMaps();

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

		void renderDeferredGBuffers(RenderCall* rc);
		void renderDeferred();

		void renderTransparenciesForward();
		void renderMultipassTransparencies(GFX::Shader* shader, RenderCall* rc);

		void showUI();

		void showGBuffers(vec2 window_size, Camera* camera);

		void cameraToShader(Camera* camera, GFX::Shader* shader); //sends camera uniforms to shader
		void lightToShader(LightEntity* light, GFX::Shader* shader); //sends light uniforms to shader

		void storeDrawCall(SCN::Node* node, Camera* camera);

		void storeDrawCallNoPriority(SCN::Node* node, Camera* camera);

		void prioritySwitch();

		void renderRenderCalls(RenderCall* rc);

		void renderShadowmaps();
	};

};