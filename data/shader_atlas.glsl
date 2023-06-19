//example of some shaders compiled
flat basic.vs flat.fs
texture basic.vs texture.fs
skybox basic.vs skybox.fs
depth quad.vs depth.fs
multi basic.vs multi.fs


//forward
texture_improved basic.vs texture_improved.fs
lights_multi basic.vs lights_multi.fs
light_pbr basic.vs light_pbr.fs
lights_single basic.vs lights_single.fs

//DEFERRED
gbuffers basic.vs gbuffers.fs
deferred_global quad.vs deferred_global.fs
deferred_globalpos quad.vs deferred_globalpos.fs
deferred_light_geometry basic.vs deferred_light_geometry.fs
deferred_light quad.vs deferred_light.fs
deferred_pbr_geometry basic.vs deferred_pbr_geometry.fs
deferred_pbr quad.vs deferred_pbr.fs

//SSAO
ssao quad.vs ssao.fs

//IRRADIANCE
spherical_probe basic.vs spherical_probe.fs
irradiance quad.vs irradiance.fs

//REFLECTIONS
reflectionProbe basic.vs reflectionProbe.fs
mirror basic.vs mirror.fs

//Volumetric
volumetric quad.vs volumetric.fs

//Decals
decal basic.vs decal.fs

gamma quad.vs gamma.fs

//TONEMAPPER
tonemapper quad.vs tonemapper.fs
uncharted_tonemapper quad.vs uncharted_tonemapper.fs

\basic.vs

#version 330 core

in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_coord;
in vec4 a_color;

uniform vec3 u_camera_position;

uniform mat4 u_model;
uniform mat4 u_viewprojection;

//this will store the color for the pixel shader
out vec3 v_position;
out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;
out vec4 v_color;

uniform float u_time;

void main()
{	
	//calcule the normal in camera space (the NormalMatrix is like ViewMatrix but without traslation)
	v_normal = (u_model * vec4( a_normal, 0.0) ).xyz;
	
	//calcule the vertex in object space
	v_position = a_vertex;
	v_world_position = (u_model * vec4( v_position, 1.0) ).xyz;
	
	//store the color in the varying var to use it from the pixel shader
	v_color = a_color;

	//store the texture coordinates
	v_uv = a_coord;

	//calcule the position of the vertex using the matrices
	gl_Position = u_viewprojection * vec4( v_world_position, 1.0 );
}

\quad.vs

#version 330 core

in vec3 a_vertex;
in vec2 a_coord;
out vec2 v_uv;

void main()
{	
	v_uv = a_coord;
	gl_Position = vec4( a_vertex, 1.0 );
}


\flat.fs

#version 330 core

uniform vec4 u_color;

out vec4 FragColor;

void main()
{
	FragColor = u_color;
}


\texture.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform vec4 u_color;
uniform sampler2D u_albedo_texture;
uniform float u_time;
uniform float u_alpha_cutoff;

out vec4 FragColor;

void main()
{
	vec2 uv = v_uv;
	vec4 color = u_color;
	vec4 albedo = texture( u_albedo_texture, v_uv );
	albedo.xyz = pow(albedo.xyz, vec3(2.2));

	color *= texture( u_albedo_texture, v_uv );

	if(color.a < u_alpha_cutoff)
		discard;

	FragColor = color;
}


\skybox.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;

uniform samplerCube u_texture;
uniform vec3 u_camera_position;
uniform float u_skybox_intensity;
out vec4 FragColor;

void main()
{
	vec3 E = v_world_position - u_camera_position;
	vec4 color = texture( u_texture, E ) * u_skybox_intensity;
	FragColor = color;
}


\multi.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;

uniform vec4 u_color;
uniform sampler2D u_texture;
uniform float u_time;
uniform float u_alpha_cutoff;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 NormalColor;

void main()
{
	vec2 uv = v_uv;
	vec4 color = u_color;
	color *= texture( u_texture, uv );

	if(color.a < u_alpha_cutoff)
		discard;

	vec3 N = normalize(v_normal);

	FragColor = color;
	NormalColor = vec4(N,1.0);
}


\depth.fs

#version 330 core

uniform vec2 u_camera_nearfar;
uniform sampler2D u_texture; //depth map
in vec2 v_uv;
out vec4 FragColor;

void main()
{
	float n = u_camera_nearfar.x;
	float f = u_camera_nearfar.y;
	float z = texture2D(u_texture,v_uv).x;
	if( n == 0.0 && f == 1.0 )
		FragColor = vec4(z);
	else
		FragColor = vec4( n * (z + 1.0) / (f + n - z * (f - n)) );
}


\instanced.vs

#version 330 core

in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_coord;

in mat4 u_model;

uniform vec3 u_camera_position;

uniform mat4 u_viewprojection;

//this will store the color for the pixel shader
out vec3 v_position;
out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;

void main()
{	
	//calcule the normal in camera space (the NormalMatrix is like ViewMatrix but without traslation)
	v_normal = (u_model * vec4( a_normal, 0.0) ).xyz;
	
	//calcule the vertex in object space
	v_position = a_vertex;
	v_world_position = (u_model * vec4( a_vertex, 1.0) ).xyz;
	
	//store the texture coordinates
	v_uv = a_coord;

	//calcule the position of the vertex using the matrices
	gl_Position = u_viewprojection * vec4( v_world_position, 1.0 );
}


//MY UTILS

\lights
#define NO_LIGHT 0.0
#define POINT_LIGHT 1.0
#define SPOT_LIGHT 2.0
#define DIRECTIONAL_LIGHT 3.0
#define PI 3.142

uniform vec3 u_light_pos;
uniform vec3 u_light_front;
uniform vec2 u_light_cone; //cos(min_angle), cos(max_angle)
uniform vec3 u_ambient_light;
uniform vec3 u_light_color;

uniform vec4 u_light_info; //vec4(light_type, near_distance, max_distance, enable_specular)

vec3 compute_lambertian(vec3 N, vec3 L)
{
	//Diffuse light
	float NdotL = dot(N,L);
	return max(NdotL, 0.0) * u_light_color;
}

vec3 compute_specular_phong(vec3 R, vec3 V, float ks, float alpha)
{
	float RdotV = max(dot(R,V), 0.0);

	return ks * pow(RdotV, alpha) * u_light_color;
}

// Normal Distribution Function using GGX Distribution
float compute_GGX (	const in float NoH, 
const in float linearRoughness )
{
	float a2 = linearRoughness * linearRoughness;
	float f = (NoH * NoH) * (a2 - 1.0) + 1.0;
	return a2 / (PI * f * f);
}

// Fresnel term with colorized fresnel
vec3 compute_Schlick( const in float VoH, 
const in vec3 f0)
{
	float f = pow(1.0 - VoH, 5.0);
	return f0 + (vec3(1.0) - f0) * f;
}


// Geometry Term: Geometry masking/shadowing due to microfacets
float GGX(float NdotV, float k){
	return NdotV / (NdotV * (1.0 - k) + k);
}
	
float compute_Smith( float NdotV, float NdotL, float roughness)
{
	float k = pow(roughness + 1.0, 2.0) / 8.0;
	return GGX(NdotL, k) * GGX(NdotV, k);
}

//this is the cook torrance specular reflection model
vec3 compute_specular_BRDF( float roughness, vec3 f0, float NoH, float NoV, float NoL, float LoH )
{
	float a = roughness * roughness;

	// Normal Distribution Function
	float D = compute_GGX( NoH, a );

		// Fresnel Function
		vec3 F = compute_Schlick( LoH, f0 );

		// Visibility Function (shadowing/masking)
		float G = compute_Smith( NoV, NoL, roughness );
			
		// Norm factor
		vec3 spec = D * G * F;
		spec /= (4.0 * NoL * NoV + 1e-6);

		return spec;
}

\normalmaps
uniform int u_enable_normalmaps;
//NORMAL MAPPING
mat3 cotangent_frame(vec3 N, vec3 p, vec2 uv)
{
	// get edge vectors of the pixel triangle
	vec3 dp1 = dFdx( p );
	vec3 dp2 = dFdy( p );
	vec2 duv1 = dFdx( uv );
	vec2 duv2 = dFdy( uv );
	
	// solve the linear system
	vec3 dp2perp = cross( dp2, N );
	vec3 dp1perp = cross( N, dp1 );
	vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
	vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
 
	// construct a scale-invariant frame 
	float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );
	return mat3( T * invmax, B * invmax, N );
}

vec3 perturbNormal(vec3 N, vec3 WP, vec2 uv, vec3 normal_pixel)
{
	normal_pixel = normal_pixel * 255./127. - 128./127.;
	mat3 TBN = cotangent_frame(N, WP, uv);
	return normalize(TBN * normal_pixel);
}

\shadowmaps
uniform vec2 u_shadow_params; // 0 o 1 shadowmap or not, bias
uniform sampler2D u_shadowmap;
uniform mat4 u_shadow_viewproj;


//SHADOW MAPPING
float testShadow(vec3 pos)
{
	//project our 3D position to the shadowmap
	vec4 proj_pos = u_shadow_viewproj * vec4(pos,1.0);

	//from homogeneus space to clip space
	vec2 shadow_uv = proj_pos.xy / proj_pos.w;

	//from clip space to uv space
	shadow_uv = shadow_uv * 0.5 + vec2(0.5);

	//get point depth [-1 .. +1] in non-linear space
	float real_depth = (proj_pos.z - u_shadow_params.y) / proj_pos.w;

	//normalize from [-1..+1] to [0..+1] still non-linear
	real_depth = real_depth * 0.5 + 0.5;

	//read depth from depth buffer in [0..+1] non-linear
	float shadow_depth = texture( u_shadowmap, shadow_uv).x;

	//it is outside on the sides
	if( shadow_uv.x < 0.0 || shadow_uv.x > 1.0 ||
		shadow_uv.y < 0.0 || shadow_uv.y > 1.0 )
			return 0.0;

	//it is before near or behind far plane
	if(real_depth < 0.0 || real_depth > 1.0)
		return 1.0;

	//compute final shadow factor by comparing
	float shadow_factor = 1.0;

	//we can compare them, even if they are not linear
	if( shadow_depth < real_depth )
		shadow_factor = 0.0;
	return shadow_factor;
}

\dithering 
uniform float u_enable_dithering;
//from https://github.com/hughsk/glsl-dither/blob/master/4x4.glsl
float dither4x4(vec2 position, float brightness)
{
  int x = int(mod(position.x, 4.0));
  int y = int(mod(position.y, 4.0));
  int index = x + y * 4;
  float limit = 0.0;

  if (x < 8) {
    if (index == 0) limit = 0.0625;
    if (index == 1) limit = 0.5625;
    if (index == 2) limit = 0.1875;
    if (index == 3) limit = 0.6875;
    if (index == 4) limit = 0.8125;
    if (index == 5) limit = 0.3125;
    if (index == 6) limit = 0.9375;
    if (index == 7) limit = 0.4375;
    if (index == 8) limit = 0.25;
    if (index == 9) limit = 0.75;
    if (index == 10) limit = 0.125;
    if (index == 11) limit = 0.625;
    if (index == 12) limit = 1.0;
    if (index == 13) limit = 0.5;
    if (index == 14) limit = 0.875;
    if (index == 15) limit = 0.375;
  }

  return brightness < limit ? 0.0 : 1.0;
}

\SphericalHarmonics
const float Pi = 3.141592654;
const float CosineA0 = Pi;
const float CosineA1 = (2.0 * Pi) / 3.0;
const float CosineA2 = Pi * 0.25;
struct SH9 { float c[9]; }; //to store weights
struct SH9Color { vec3 c[9]; }; //to store colors

void SHCosineLobe(in vec3 dir, out SH9 sh) //SH9
{
	// Band 0
	sh.c[0] = 0.282095 * CosineA0;
	// Band 1
	sh.c[1] = 0.488603 * dir.y * CosineA1; 
	sh.c[2] = 0.488603 * dir.z * CosineA1;
	sh.c[3] = 0.488603 * dir.x * CosineA1;
	// Band 2
	sh.c[4] = 1.092548 * dir.x * dir.y * CosineA2;
	sh.c[5] = 1.092548 * dir.y * dir.z * CosineA2;
	sh.c[6] = 0.315392 * (3.0 * dir.z * dir.z - 1.0) * CosineA2;
	sh.c[7] = 1.092548 * dir.x * dir.z * CosineA2;
	sh.c[8] = 0.546274 * (dir.x * dir.x - dir.y * dir.y) * CosineA2;
}

vec3 ComputeSHIrradiance(in vec3 normal, in SH9Color sh)
{
	// Compute the cosine lobe in SH, oriented about the normal direction
	SH9 shCosine;
	SHCosineLobe(normal, shCosine);
	// Compute the SH dot product to get irradiance
	vec3 irradiance = vec3(0.0);
	for(int i = 0; i < 9; ++i)
		irradiance += sh.c[i] * shCosine.c[i];

	return irradiance;
}

//MY SHADERS 

\texture_improved.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform vec4 u_color;
uniform sampler2D u_albedo_texture;
uniform sampler2D u_emissive_texture;
uniform sampler2D u_metallic_roughness_texture;
uniform vec3 u_emissive_factor;

//FOR REFLECTIONS
uniform samplerCube u_skybox;
uniform vec3 u_camera_position;

uniform vec3 u_reflections_info; //enable_reflections, reflections_factor, enable_fresnel

uniform vec3 u_ambient_light;

uniform float u_time;
uniform float u_alpha_cutoff;

out vec4 FragColor;

void main()
{
	vec2 uv = v_uv;
	vec4 albedo = u_color;
	albedo *= texture( u_albedo_texture, v_uv );
	albedo.xyz = pow(albedo.xyz, vec3(2.2));

	if(albedo.a < u_alpha_cutoff)
		discard;

	vec3 emissive_light = texture(u_emissive_texture, v_uv).xyz;
	emissive_light = pow(emissive_light, vec3(2.2));
	emissive_light *= u_emissive_factor;

	albedo.xyz += emissive_light;

	float occlusion = texture(u_metallic_roughness_texture, v_uv).r;
	vec3 ambient_light = u_ambient_light * occlusion;

	albedo.xyz *= ambient_light; 

	if(u_reflections_info.x == 1)
	{
		//compute reflected eye vector
		vec3 N = normalize(v_normal);
		vec3 V = normalize(u_camera_position - v_world_position);

		vec3 R = reflect(N,V);

		float fresnel = 1.0;
		if(u_reflections_info.z == 1)
		{
			fresnel = 1.0 - max(dot(N,V), 0.0);
			fresnel = pow(fresnel, 2.0);
		}

		//fetch the color from the texture
		albedo += texture( u_skybox, R ) * u_reflections_info.y * fresnel;
	}

	FragColor = albedo;
}

\lights_multi.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform vec3 u_camera_position;

//material properties
uniform vec4 u_color;
uniform sampler2D u_albedo_texture;
uniform sampler2D u_emissive_texture;
uniform sampler2D u_metallic_roughness_texture;
uniform sampler2D u_normal_texture;
uniform vec3 u_emissive_factor;
uniform vec2 u_mat_properties;	// (metallic_factor, roughness_factor)

uniform samplerCube u_environment;
uniform bool u_enable_reflections;

//global properties

uniform float u_time;
uniform float u_alpha_cutoff;

#include "lights"
#include "normalmaps"
#include "shadowmaps"

out vec4 FragColor;

void main()
{
	vec2 uv = v_uv;
	vec4 albedo = u_color;
	albedo *= texture( u_albedo_texture, v_uv );

	albedo.xyz = pow(albedo.xyz, vec3(2.2));

	float occlusion = texture(u_metallic_roughness_texture, v_uv).r;
	float ks = texture(u_metallic_roughness_texture, v_uv).g;
	float alpha = texture(u_metallic_roughness_texture, v_uv).b;


	if(albedo.a < u_alpha_cutoff)
		discard;

	vec3 light = vec3(0.0);
	vec3 N = vec3(0.0);
	if(u_enable_normalmaps == 1)
	{
		vec3 normal_pixel = texture2D(u_normal_texture, v_uv).xyz;
		N = perturbNormal(v_normal, v_world_position, v_uv, normal_pixel);
	}
	
	else
	{
		N = normalize(v_normal);
	}

	
	vec3 R;	
	vec3 V = normalize(u_camera_position - v_world_position);

	float shadow_factor =  1.0;

	if(u_shadow_params.x != 0 && u_light_info.x != NO_LIGHT)
	{
		shadow_factor = testShadow(v_world_position);
	}

	if(u_light_info.x == DIRECTIONAL_LIGHT)
	{
		light += compute_lambertian(N, u_light_front);

		//Specular light
		if(u_light_info.a == 1 && alpha != 0.0)
		{
			R = normalize(-reflect(u_light_front, N));

			light += compute_specular_phong(R, V, ks, alpha);
		}
		
		light *= shadow_factor;
	}

	else if(u_light_info.x == POINT_LIGHT || u_light_info.x == SPOT_LIGHT)
	{
		//BASIC PHONG
		vec3 L = u_light_pos - v_world_position;
		float dist = length(L);
		L /= dist;

		light += compute_lambertian(N,L);

		//Specular light
		if(u_light_info.a == 1 && alpha != 0.0)
		{
			R = normalize(-reflect(L, N));
			light += compute_specular_phong(R, V, ks, alpha);

		}

		//LINEAR DISTANCE ATTENUATION
		float attenuation = u_light_info.z - dist;
		attenuation /= u_light_info.z;
		attenuation = max(attenuation, 0.0);


		if(u_light_info.x == SPOT_LIGHT)
		{

			float cos_angle = dot(u_light_front, L);
			if(cos_angle < u_light_cone.y)
				attenuation = 0.0;
			else if(cos_angle < u_light_cone.x)
				attenuation *= (cos_angle - u_light_cone.y) / (u_light_cone.x - u_light_cone.y);
		}
		
		light *= attenuation * shadow_factor;
	}

	if(u_light_info.x == NO_LIGHT)
	{
		light = u_ambient_light * occlusion;
	}

	else
	{
		light += u_ambient_light * occlusion; 
	}

	vec3 color = albedo.xyz * light;

	vec3 emissive_light = texture(u_emissive_texture, v_uv).xyz;
	emissive_light = pow(emissive_light, vec3(2.2));
	emissive_light *= u_emissive_factor;
	color += emissive_light;

	
	//environment reflections
	if (u_enable_reflections)
	{
		vec3 E = normalize(v_world_position - u_camera_position);
		R = (reflect(E, N));
		vec3 reflected_color = texture(u_environment, R).xyz;
		reflected_color.xyz = pow(reflected_color.xyz, vec3(2.2));
	
		float fresnel = 1.0 - max(dot(N,-E), 0.0);
		fresnel = pow(fresnel, 2.0);
		float reflective_factor = fresnel * alpha * u_mat_properties.x;

		color.xyz = mix( color.xyz, reflected_color, reflective_factor);
	}
	
	FragColor = vec4(color, albedo.a);
}

\light_pbr.fs
#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform vec3 u_camera_position;

//material properties
uniform vec4 u_color;
uniform sampler2D u_albedo_texture;
uniform sampler2D u_emissive_texture;
uniform sampler2D u_metallic_roughness_texture;
uniform sampler2D u_normal_texture;
uniform vec3 u_emissive_factor;
uniform vec2 u_mat_properties;	// (metallic_factor, roughness_factor)

uniform samplerCube u_environment;
uniform bool u_enable_reflections;

//global properties

uniform float u_time;
uniform float u_alpha_cutoff;

#include "lights"
#include "normalmaps"
#include "shadowmaps"

out vec4 FragColor;

void main()
{
	vec2 uv = v_uv;
	vec4 albedo = u_color;
	albedo *= texture( u_albedo_texture, v_uv );

	albedo.xyz = pow(albedo.xyz, vec3(2.2));

	float occlusion = texture(u_metallic_roughness_texture, v_uv).r;
	float metallic = texture(u_metallic_roughness_texture, v_uv).g;
	float roughness = texture(u_metallic_roughness_texture, v_uv).b;

	if(albedo.a < u_alpha_cutoff)
		discard;

	//we compute the reflection in base to the color and the metalness
	vec3 f0 = mix( vec3(0.5), albedo.xyz, metallic );

	//metallic materials do not have diffuse
	vec3 diffuseColor = (1.0 - metallic) * albedo.xyz;

	vec3 light = vec3(0.0);
	vec3 N = vec3(0.0);
	if(u_enable_normalmaps == 1)
	{
		vec3 normal_pixel = texture2D(u_normal_texture, v_uv).xyz;
		N = perturbNormal(v_normal, v_world_position, v_uv, normal_pixel);
	}
	
	else
	{
		N = normalize(v_normal);
	}
	
	vec3 V = normalize(u_camera_position - v_world_position);

	float shadow_factor =  1.0;

	if(u_shadow_params.x != 0 && u_light_info.x != NO_LIGHT)
	{
		shadow_factor = testShadow(v_world_position);
	}

	if(u_light_info.x == DIRECTIONAL_LIGHT)
	{
		vec3 diffuse = compute_lambertian(N, u_light_front) * diffuseColor;

		vec3 specular;
		//Specular light
		if(u_light_info.a == 1 && roughness != 0.0)
		{
			vec3 H = normalize( u_light_front + V );
			float NdotH = max(dot(N,H), 0.0);
			float NdotV = max(dot(N, V), 0.0);
			float NdotL = max(dot(N, u_light_front), 0.0);
			float LdotH = max(dot(u_light_front, H), 0.0);

			//compute the specular
			specular = compute_specular_BRDF(roughness, f0, NdotH, NdotV, NdotL, LdotH) * u_light_color;
		}

		light += (diffuse + specular) * shadow_factor;
	}

	else if(u_light_info.x == POINT_LIGHT || u_light_info.x == SPOT_LIGHT)
	{
		//BASIC PHONG
		vec3 L = u_light_pos - v_world_position;
		float dist = length(L);
		L /= dist;

		vec3 diffuse = compute_lambertian(N,L) * diffuseColor;

		vec3 specular;
		//Specular light
		if(u_light_info.a == 1 && roughness != 0.0)
		{
			vec3 H = normalize( L + V );
			float NdotH = max(dot(N,H), 0.0);
			float NdotV = max(dot(N, V), 0.0);
			float NdotL = max(dot(N, L), 0.0);
			float LdotH = max(dot(L, H), 0.0);

			//compute the specular
			specular = compute_specular_BRDF(roughness, f0, NdotH, NdotV, NdotL, LdotH) * u_light_color;
		}

		light += diffuse + specular;

		//LINEAR DISTANCE ATTENUATION
		float attenuation = u_light_info.z - dist;
		attenuation /= u_light_info.z;
		attenuation = max(attenuation, 0.0);


		if(u_light_info.x == SPOT_LIGHT)
		{

			float cos_angle = dot(u_light_front, L);
			if(cos_angle < u_light_cone.y)
				attenuation = 0.0;
			else if(cos_angle < u_light_cone.x)
				attenuation *= (cos_angle - u_light_cone.y) / (u_light_cone.x - u_light_cone.y);
		}
		
		light *= attenuation * shadow_factor;
	}

	if(u_light_info.x == NO_LIGHT)
	{
		light = u_ambient_light * occlusion;
	}

	else
	{
		light += u_ambient_light * occlusion; 
	}

	vec3 color = albedo.xyz * light;

	vec3 emissive_light = texture(u_emissive_texture, v_uv).xyz;
	emissive_light = pow(emissive_light, vec3(2.2));
	emissive_light *= u_emissive_factor;
	color += emissive_light;

	//environment reflections
	if (u_enable_reflections)
	{
		vec3 E = normalize(v_world_position - u_camera_position);
		vec3 R = (reflect(E, N));
		vec3 reflected_color = texture(u_environment, R).xyz;
		reflected_color.xyz = pow(reflected_color.xyz, vec3(2.2));
	

		float EoN = max(dot(-E, N), 0.0);
		vec3 fresnel = compute_Schlick( EoN, f0 ); //TODO realment no se si aixo te cap mena de sentit

		vec3 reflective_factor = fresnel * metallic * u_mat_properties.x;

		color.x = mix( color.x, reflected_color.x, reflective_factor.x);
		color.y = mix( color.y, reflected_color.y, reflective_factor.y);
		color.z = mix( color.z, reflected_color.z, reflective_factor.z);

	}

	FragColor = vec4(color, albedo.a);
}

\lights_single.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform vec3 u_camera_position;
//material properties

uniform vec4 u_color;
uniform sampler2D u_albedo_texture;
uniform sampler2D u_emissive_texture;
uniform sampler2D u_metallic_roughness_texture;
uniform sampler2D u_normal_texture;
uniform vec3 u_emissive_factor;

//light properties

#define NO_LIGHT 0.0
#define POINT_LIGHT 1.0
#define SPOT_LIGHT 2.0
#define DIRECTIONAL_LIGHT 3.0

uniform vec3 u_ambient_light;

const int MAX_LIGHTS = 4;
uniform int u_num_lights;
uniform vec3 u_lights_pos[MAX_LIGHTS];
uniform vec3 u_lights_color[MAX_LIGHTS];
uniform vec4 u_lights_info[MAX_LIGHTS];
uniform vec3 u_lights_front[MAX_LIGHTS];
uniform vec2 u_lights_cone[MAX_LIGHTS];

#include "normalmaps"

//shadowmap
uniform vec2 u_shadow_params; // 0 o 1 shadowmap or not, bias
uniform sampler2D u_shadowmap;
uniform mat4 u_shadow_viewproj;

//global properties

uniform float u_time;
uniform float u_alpha_cutoff;

out vec4 FragColor;

void main()
{
	vec2 uv = v_uv;
	vec4 albedo = u_color;
	albedo *= texture( u_albedo_texture, v_uv );
	albedo.xyz = pow(albedo.xyz, vec3(2.2));

	float occlusion = texture(u_metallic_roughness_texture, v_uv).r;
	float ks = texture(u_metallic_roughness_texture, v_uv).g;
	float alpha = 1 - texture(u_metallic_roughness_texture, v_uv).b;

	if(albedo.a < u_alpha_cutoff)
		discard;

	vec3 light = vec3(0.0);
	vec3 N = vec3(0.0);

	if(u_enable_normalmaps == 1)
	{
		vec3 normal_pixel = texture2D(u_normal_texture, v_uv).xyz;
		N = perturbNormal(v_normal, v_world_position, v_uv, normal_pixel);
	} 
	else
	{
		N = normalize(v_normal);
	}

	
	vec3 V = normalize(v_position - u_camera_position);

	for( int i = 0; i < MAX_LIGHTS; ++i )
	{
		if(i < u_num_lights)
		{
			if(u_lights_info[i].x == DIRECTIONAL_LIGHT)
			{
				//Diffuse light
				float NdotL = dot(N,u_lights_front[i]);
				light += max(NdotL, 0.0) * u_lights_color[i];

				//Specular light
				if(u_lights_info[i].a == 1 && alpha != 0.0)
				{
					vec3 R = normalize(-reflect(u_lights_front[i], N));
					float RdotV = max(dot(V,R), 0.0);

					light += ks * pow(RdotV, alpha) * u_lights_color[i];
				}
			}

			else if(u_lights_info[i].x == POINT_LIGHT || u_lights_info[i].x == SPOT_LIGHT)
			{
				//BASIC PHONG
				vec3 L = u_lights_pos[i] - v_world_position;
				float dist = length(L);
				L /= dist;

				//Diffuse light
				float NdotL = dot(N,L);

				light += max(NdotL, 0.0) * u_lights_color[i];

				//Specular light
				if(u_lights_info[i].a == 1 && alpha != 0.0)
				{
					vec3 R = normalize(-reflect(L, N));

					float RdotV = max(dot(V,R), 0.0);

					light += ks * pow(RdotV, alpha) * u_lights_color[i];
				}

				//LINEAR DISTANCE ATTENUATION
				float attenuation = u_lights_info[i].z - dist;
				attenuation /= u_lights_info[i].z;
				attenuation = max(attenuation, 0.0);


				if(u_lights_info[i].x == SPOT_LIGHT)
				{
					float cos_angle = dot(u_lights_front[i], L);
					if(cos_angle < u_lights_cone[i].y)
						attenuation = 0.0;
					else if(cos_angle < u_lights_cone[i].x)
						attenuation *= (cos_angle - u_lights_cone[i].y) / (u_lights_cone[i].x - u_lights_cone[i].y);
					
				}
				
				light = light * attenuation;
			}
		}
	}

	light += u_ambient_light * occlusion; 

	vec3 color = albedo.xyz * light;

	vec3 emissive_light = texture(u_emissive_texture, v_uv).xyz;
	emissive_light = pow(emissive_light, vec3(2.2));
	emissive_light *= u_emissive_factor;

	color += emissive_light;

	FragColor = vec4(color, albedo.a);
}

//DEFERRED

\gbuffers.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform vec4 u_color;
uniform sampler2D u_albedo_texture;
uniform sampler2D u_emissive_texture;
uniform sampler2D u_metallic_roughness_texture;
uniform vec3 u_emissive_factor;

uniform sampler2D u_normal_texture;
uniform float u_alpha_cutoff;

#include "dithering"
#include "normalmaps"

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 NormalColor;
layout(location = 2) out vec4 ExtraColor;

void main()
{
	vec2 uv = v_uv;
	vec4 albedo = u_color;
	vec3 N;
	albedo *= texture( u_albedo_texture, v_uv );

	albedo.xyz = pow(albedo.xyz, vec3(2.2));

	if(albedo.a < u_alpha_cutoff)
		discard;

	if(u_enable_dithering == 1.0 && dither4x4(gl_FragCoord.xy, albedo.a) == 0.0)
		discard;

	else if (u_enable_dithering == 0.0 && albedo.a < 0.9)
		discard;

	if(u_enable_normalmaps == 1)
	{
		vec3 normal_pixel = texture2D(u_normal_texture, v_uv).xyz;
		N = perturbNormal(v_normal, v_world_position, v_uv, normal_pixel);
	}
	
	else
	{
		N = normalize(v_normal);
	}
	vec3 emissive_light = u_emissive_factor * texture(u_emissive_texture, v_uv).xyz;
	emissive_light = pow(emissive_light, vec3(2.2));

	float occlusion = texture(u_metallic_roughness_texture, v_uv).r;
	float metallic = texture(u_metallic_roughness_texture, v_uv).g;
	float roughness = texture(u_metallic_roughness_texture, v_uv).b;
	
	FragColor = vec4(albedo.xyz, occlusion);
	NormalColor = vec4(N * 0.5 + vec3(0.5), metallic);
	ExtraColor = vec4(emissive_light, roughness);
}

\deferred_global.fs

#version 330 core

in vec2 v_uv;

uniform sampler2D u_albedo_texture;
uniform sampler2D u_normal_texture;
uniform sampler2D u_extra_texture;
uniform sampler2D u_depth_texture;
uniform sampler2D u_ao_texture;

uniform bool u_add_SSAO;
uniform float u_control_SSAO;
uniform vec3 u_ambient_light;
uniform vec2 u_iRes;

out vec4 FragColor;

void main()
{
	vec2 uv = v_uv;
	vec3 color = vec3(0.0);
	
	float depth = texture(u_depth_texture, uv).r;

	if(depth == 1.0)
		discard;

	vec3 albedo = texture(u_albedo_texture, uv).rgb;
	vec3 emissive_light = texture(u_extra_texture, uv).rgb;
	float occlusion = texture(u_albedo_texture, uv).a;

	vec3 light = u_ambient_light;

	if (u_add_SSAO)
	{
		vec2 screenuv = gl_FragCoord.xy * u_iRes;

		float ao_factor = texture( u_ao_texture, screenuv ).x;

		//we could play with the curve to have more control 
		ao_factor = pow( ao_factor, u_control_SSAO );

		light *= ao_factor;

	}

	albedo *= light * occlusion;
	color.xyz += emissive_light + albedo;
	FragColor = vec4(color, 1.0);

	gl_FragDepth = depth;
}

\deferred_globalpos.fs

#version 330 core
in vec2 v_uv;

uniform mat4 u_ivp;
uniform sampler2D u_depth_texture;

uniform vec2 u_iRes;
out vec4 FragColor;

void main()
{
	vec2 uv = gl_FragCoord.xy * u_iRes.xy;
	vec3 color = vec3(0.0);
	
	float depth = texture(u_depth_texture, uv).r;

	if(depth == 1.0)
		discard;

	vec4 screen_coord = vec4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 world_proj = u_ivp * screen_coord;

	vec3 world_pos = world_proj.xyz / world_proj.w;
	color = mod(abs(world_pos * 0.01), vec3(1.0));

	FragColor = vec4(color, 1.0);
}

\deferred_light.fs

#version 330 core
in vec2 v_uv;


uniform vec3 u_camera_position;
uniform mat4 u_ivp;

//material properties

uniform sampler2D u_albedo_texture;
uniform sampler2D u_normal_texture;
uniform sampler2D u_extra_texture;
uniform sampler2D u_depth_texture;

#include "lights"
#include "shadowmaps"

uniform vec2 u_iRes;
out vec4 FragColor;

void main()
{
	//vec2 uv = v_uv;
	vec2 uv = gl_FragCoord.xy * u_iRes.xy;
		
	float depth = texture(u_depth_texture, uv).r;

	if(depth == 1.0)
		discard;

	vec4 screen_coord = vec4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 world_proj = u_ivp * screen_coord;

	vec3 world_pos = world_proj.xyz / world_proj.w;

	vec3 albedo = texture(u_albedo_texture, uv).rgb;
	vec3 emissive_light = texture(u_extra_texture, uv).rgb;
	vec3 normal = texture(u_normal_texture, uv).rgb;
	float ks = texture(u_normal_texture, uv).a;
	float alpha = texture(u_extra_texture, uv).a;

	vec3 light = vec3(0.0);
	vec3 N = normalize(normal * 2.0 - vec3(1.0)); 

	float shadow_factor =  1.0;

	if(u_shadow_params.x != 0 && u_light_info.x != NO_LIGHT)
	{
		shadow_factor = testShadow(world_pos);
	}

	vec3 V = normalize(world_pos - u_camera_position);

	if(u_light_info.x == DIRECTIONAL_LIGHT)
	{
		light += compute_lambertian(N,u_light_front);

		//Specular light
		if(u_light_info.a == 1 && alpha != 0.0)
		{
			vec3 R = normalize(-reflect(u_light_front, N));

			light += compute_specular_phong(R, V, ks, alpha);
		}
		
		light *= shadow_factor;
	}

	else if(u_light_info.x == POINT_LIGHT || u_light_info.x == SPOT_LIGHT)
	{
		//BASIC PHONG
		vec3 L = u_light_pos - world_pos;
		float dist = length(L);
		L /= dist;

		light += compute_lambertian(N,L);

		//Specular light
		if(u_light_info.a == 1 && alpha != 0.0)
		{
			vec3 R = normalize(-reflect(L, N));
			
			light += compute_specular_phong(R, V, ks, alpha);
		}

		//LINEAR DISTANCE ATTENUATION
		float attenuation = u_light_info.z - dist;
		attenuation /= u_light_info.z;
		attenuation = max(attenuation, 0.0);


		if(u_light_info.x == SPOT_LIGHT)
		{

			float cos_angle = dot(u_light_front, L);
			if(cos_angle < u_light_cone.y)
				attenuation = 0.0;
			else if(cos_angle < u_light_cone.x)
				attenuation *= (cos_angle - u_light_cone.y) / (u_light_cone.x - u_light_cone.y);
		}
		
		light *= attenuation * shadow_factor;
	}

	vec3 color = albedo.xyz * light;

	FragColor = vec4(color, 1.0);

	gl_FragDepth = depth;
	//FragColor = vec4(1.0, 0.0, 0.0, 1.0);

}

\deferred_light_geometry.fs

#version 330 core
in vec2 v_uv;


uniform vec3 u_camera_position;
uniform mat4 u_ivp;

//material properties

uniform sampler2D u_albedo_texture;
uniform sampler2D u_normal_texture;
uniform sampler2D u_extra_texture;
uniform sampler2D u_depth_texture;

#include "lights"
#include "shadowmaps"

uniform vec2 u_iRes;
out vec4 FragColor;

void main()
{
	//vec2 uv = v_uv;
	vec2 uv = gl_FragCoord.xy * u_iRes.xy;
		
	float depth = texture(u_depth_texture, uv).r;

	if(depth == 1.0)
		discard;

	vec4 screen_coord = vec4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 world_proj = u_ivp * screen_coord;

	vec3 world_pos = world_proj.xyz / world_proj.w;

	vec3 albedo = texture(u_albedo_texture, uv).rgb;
	vec3 emissive_light = texture(u_extra_texture, uv).rgb;
	vec3 normal = texture(u_normal_texture, uv).rgb;
	float ks = texture(u_normal_texture, uv).a;
	float alpha = texture(u_extra_texture, uv).a;

	vec3 light = vec3(0.0);
	vec3 N = normalize(normal * 2.0 - vec3(1.0)); 

	float shadow_factor =  1.0;

	if(u_shadow_params.x != 0 && u_light_info.x != NO_LIGHT)
	{
		shadow_factor = testShadow(world_pos);
	}

	vec3 V = normalize(world_pos - u_camera_position);

	if(u_light_info.x == DIRECTIONAL_LIGHT)
	{
		light += compute_lambertian(N,u_light_front);

		//Specular light
		if(u_light_info.a == 1 && alpha != 0.0)
		{
			vec3 R = normalize(-reflect(u_light_front, N));

			light += compute_specular_phong(R, V, ks, alpha);
		}
		
		light *= shadow_factor;
	}

	else if(u_light_info.x == POINT_LIGHT || u_light_info.x == SPOT_LIGHT)
	{
		//BASIC PHONG
		vec3 L = u_light_pos - world_pos;
		float dist = length(L);
		L /= dist;

		light += compute_lambertian(N,L);

		//Specular light
		if(u_light_info.a == 1 && alpha != 0.0)
		{
			vec3 R = normalize(-reflect(L, N));
			
			light += compute_specular_phong(R, V, ks, alpha);
		}

		//LINEAR DISTANCE ATTENUATION
		float attenuation = u_light_info.z - dist;
		attenuation /= u_light_info.z;
		attenuation = max(attenuation, 0.0);


		if(u_light_info.x == SPOT_LIGHT)
		{

			float cos_angle = dot(u_light_front, L);
			if(cos_angle < u_light_cone.y)
				attenuation = 0.0;
			else if(cos_angle < u_light_cone.x)
				attenuation *= (cos_angle - u_light_cone.y) / (u_light_cone.x - u_light_cone.y);
		}
		
		light *= attenuation * shadow_factor;
	}

	vec3 color = albedo.xyz * light;

	FragColor = vec4(color, 1.0);
	//FragColor = vec4(1.0, 0.0, 0.0, 1.0);

}

\deferred_pbr_geometry.fs
#version 330 core
in vec2 v_uv;


uniform vec3 u_camera_position;
uniform mat4 u_ivp;

//material properties

uniform sampler2D u_albedo_texture;
uniform sampler2D u_normal_texture;
uniform sampler2D u_extra_texture;
uniform sampler2D u_depth_texture;

#include "lights"
#include "shadowmaps"

uniform vec2 u_iRes;
out vec4 FragColor;

void main()
{
	vec2 uv = gl_FragCoord.xy * u_iRes.xy;
		
	float depth = texture(u_depth_texture, uv).r;

	if(depth == 1.0)
		discard;

	vec4 screen_coord = vec4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 world_proj = u_ivp * screen_coord;

	vec3 world_pos = world_proj.xyz / world_proj.w;

	vec3 albedo = texture( u_albedo_texture, uv ).rgb;

	float occlusion = texture(u_albedo_texture, uv).a;
	float metallic = texture(u_normal_texture, uv).a;
	float roughness = texture(u_extra_texture, uv).a;

	vec3 normal = texture(u_normal_texture, uv).rgb;
	vec3 N = normalize(normal * 2.0 - vec3(1.0)); 

	//we compute the reflection in base to the color and the metalness
	vec3 f0 = mix( vec3(0.5), albedo.xyz, metallic );

	//metallic materials do not have diffuse
	vec3 diffuseColor = (1.0 - metallic) * albedo.xyz;

	vec3 light = vec3(0.0);
	vec3 V = normalize(u_camera_position - world_pos);

	float shadow_factor =  1.0;

	if(u_shadow_params.x != 0 && u_light_info.x != NO_LIGHT)
	{
		shadow_factor = testShadow(world_pos);
	}

	if(u_light_info.x == DIRECTIONAL_LIGHT)
	{
		vec3 diffuse = compute_lambertian(N, u_light_front) * diffuseColor;

		vec3 specular;
		//Specular light
		if(roughness != 0.0)
		{
			vec3 H = normalize( u_light_front + V );
			float NdotH = max(dot(N,H), 0.0);
			float NdotV = max(dot(N, V), 0.0);
			float NdotL = max(dot(N, u_light_front), 0.0);
			float LdotH = max(dot(u_light_front, H), 0.0);

			//compute the specular
			specular = compute_specular_BRDF(roughness, f0, NdotH, NdotV, NdotL, LdotH) * u_light_color;
		}

		light += (diffuse + specular) * shadow_factor;
	}

	else if(u_light_info.x == POINT_LIGHT || u_light_info.x == SPOT_LIGHT)
	{
		//BASIC PHONG
		vec3 L = u_light_pos - world_pos;
		float dist = length(L);
		L /= dist;

		vec3 diffuse = compute_lambertian(N,L) * diffuseColor;

		vec3 specular;
		//Specular light
		if(roughness != 0.0)
		{
			vec3 H = normalize( L + V );
			float NdotH = max(dot(N,H), 0.0);
			float NdotV = max(dot(N, V), 0.0);
			float NdotL = max(dot(N, L), 0.0);
			float LdotH = max(dot(L, H), 0.0);

			//compute the specular
			specular = compute_specular_BRDF(roughness, f0, NdotH, NdotV, NdotL, LdotH) * u_light_color;
		}

		light += diffuse + specular;

		//LINEAR DISTANCE ATTENUATION
		float attenuation = u_light_info.z - dist;
		attenuation /= u_light_info.z;
		attenuation = max(attenuation, 0.0);


		if(u_light_info.x == SPOT_LIGHT)
		{

			float cos_angle = dot(u_light_front, L);
			if(cos_angle < u_light_cone.y)
				attenuation = 0.0;
			else if(cos_angle < u_light_cone.x)
				attenuation *= (cos_angle - u_light_cone.y) / (u_light_cone.x - u_light_cone.y);
		}
		
		light *= attenuation * shadow_factor;
	}

	if(u_light_info.x == NO_LIGHT)
	{
		light = u_ambient_light * occlusion;
	}

	else
	{
		light += u_ambient_light * occlusion; 
	}

	vec3 color = albedo.xyz * light;

	FragColor = vec4(color, 1.0);
}

\deferred_pbr.fs
#version 330 core
in vec2 v_uv;


uniform vec3 u_camera_position;
uniform mat4 u_ivp;

//material properties

uniform sampler2D u_albedo_texture;
uniform sampler2D u_normal_texture;
uniform sampler2D u_extra_texture;
uniform sampler2D u_depth_texture;

#include "lights"
#include "shadowmaps"

uniform vec2 u_iRes;
out vec4 FragColor;

void main()
{
	vec2 uv = gl_FragCoord.xy * u_iRes.xy;
		
	float depth = texture(u_depth_texture, uv).r;

	if(depth == 1.0)
		discard;

	vec4 screen_coord = vec4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 world_proj = u_ivp * screen_coord;

	vec3 world_pos = world_proj.xyz / world_proj.w;

	vec3 albedo = texture( u_albedo_texture, uv ).rgb;

	float occlusion = texture(u_albedo_texture, uv).a;
	float metallic = texture(u_normal_texture, uv).a;
	float roughness = texture(u_extra_texture, uv).a;

	vec3 normal = texture(u_normal_texture, uv).rgb;
	vec3 N = normalize(normal * 2.0 - vec3(1.0)); 

	//we compute the reflection in base to the color and the metalness
	vec3 f0 = mix( vec3(0.5), albedo.xyz, metallic );

	//metallic materials do not have diffuse
	vec3 diffuseColor = (1.0 - metallic) * albedo.xyz;

	vec3 light = vec3(0.0);
	vec3 V = normalize(u_camera_position - world_pos);

	float shadow_factor =  1.0;

	if(u_shadow_params.x != 0 && u_light_info.x != NO_LIGHT)
	{
		shadow_factor = testShadow(world_pos);
	}

	if(u_light_info.x == DIRECTIONAL_LIGHT)
	{
		vec3 diffuse = compute_lambertian(N, u_light_front) * diffuseColor;

		vec3 specular;
		//Specular light
		if(roughness != 0.0)
		{
			vec3 H = normalize( u_light_front + V );
			float NdotH = max(dot(N,H), 0.0);
			float NdotV = max(dot(N, V), 0.0);
			float NdotL = max(dot(N, u_light_front), 0.0);
			float LdotH = max(dot(u_light_front, H), 0.0);

			//compute the specular
			specular = compute_specular_BRDF(roughness, f0, NdotH, NdotV, NdotL, LdotH) * u_light_color;

		}

		light += (diffuse + specular) * shadow_factor;
	}

	else if(u_light_info.x == POINT_LIGHT || u_light_info.x == SPOT_LIGHT)
	{
		//BASIC PHONG
		vec3 L = u_light_pos - world_pos;
		float dist = length(L);
		L /= dist;

		vec3 diffuse = compute_lambertian(N,L) * diffuseColor;

		vec3 specular;
		//Specular light
		if(roughness != 0.0)
		{
			vec3 H = normalize( L + V );
			float NdotH = max(dot(N,H), 0.0);
			float NdotV = max(dot(N, V), 0.0);
			float NdotL = max(dot(N, L), 0.0);
			float LdotH = max(dot(L, H), 0.0);

			//compute the specular
			specular = compute_specular_BRDF(roughness, f0, NdotH, NdotV, NdotL, LdotH) * u_light_color;
		}

		light += diffuse + specular;

		//LINEAR DISTANCE ATTENUATION
		float attenuation = u_light_info.z - dist;
		attenuation /= u_light_info.z;
		attenuation = max(attenuation, 0.0);


		if(u_light_info.x == SPOT_LIGHT)
		{

			float cos_angle = dot(u_light_front, L);
			if(cos_angle < u_light_cone.y)
				attenuation = 0.0;
			else if(cos_angle < u_light_cone.x)
				attenuation *= (cos_angle - u_light_cone.y) / (u_light_cone.x - u_light_cone.y);
		}
		
		light *= attenuation * shadow_factor;
	}

	vec3 color = albedo.xyz * light;

	FragColor = vec4(color, 1.0);
}

\ssao.fs

#version 330 core

in vec2 v_uv;

uniform sampler2D u_depth_texture;
uniform sampler2D u_normal_texture;

uniform mat4 u_viewprojection;
uniform mat4 u_ivp;
uniform vec2 u_iRes;

#define NUM_POINTS 64

uniform vec3 u_random_points[NUM_POINTS];
uniform float u_radius; 
uniform vec3 u_front; 

#include "normalmaps"


layout(location = 0) out vec4 FragColor;

//random value from uv
float rand(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453123);
}

//create rotation matrix from arbitrary axis and angle
mat4 rotationMatrix( vec3 axis, float angle )
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat4(oc * axis.x * axis.x + c, oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0, oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c, oc * axis.y * axis.z - axis.x * s,  0.0,oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0, 0.0, 0.0, 0.0, 1.0);
}


void main()
{
	vec2 uv = gl_FragCoord.xy * u_iRes.xy;
		
	float depth = texture(u_depth_texture, uv).r;

	if(depth >= 1.0)  //skip skybox pixels
	{
		FragColor = vec4(1.0);
		return;
	}

	
	vec4 screen_coord = vec4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 world_proj = u_ivp * screen_coord;

	vec3 world_pos = world_proj.xyz / world_proj.w;

	
	vec3 normal = texture(u_normal_texture, uv).rgb;
	vec3 N = normalize(normal * 2.0 - vec3(1.0)); 

	int outside = 0;
	for( int i = 0; i < NUM_POINTS; i++)
	{
		vec3 offset = u_random_points[i] * u_radius;	
		mat4 rot =  rotationMatrix( u_front, rand(gl_FragCoord.xy) );
		offset = (rot * vec4(offset, 1.0)).xyz;
			
		vec3 p = offset;

		// rotate the point in the hemisphere
		mat3 rotmat = cotangent_frame( N, world_pos, uv );
		p =  rotmat * p;
		p += world_pos;

		vec4 proj = u_viewprojection * vec4(p,1.0);
		proj.xy /= proj.w; 
		
		proj.z = (proj.z - 0.005) / proj.w;
		proj.xyz = proj.xyz * 0.5 + vec3(0.5); //to [0..1]
		
		float pdepth = texture( u_depth_texture, proj.xy ).x;
		
		float diff = pdepth - proj.z;
		//diff = pow(diff, 2.2);  // TODO: linierize values, this is not working 

		//if(diff > 0.00005)
		if(pdepth > proj.z)
			outside++; 

	}

	float v = float(outside) / float(NUM_POINTS);
	

	FragColor = vec4(v, v, v, 1.0);
	//FragColor = vec4(N.x, N.y, N.z, 1.0);

	
}

\volumetric.fs

#version 330 core

in vec2 v_uv;

uniform sampler2D u_depth_texture;

uniform mat4 u_viewprojection;
uniform mat4 u_ivp;
uniform vec2 u_iRes;
uniform vec3 u_camera_position;
uniform float u_air_density;

#define SAMPLES 64

#include "lights"
#include "shadowmaps"

out vec4 FragColor;

//random value from uv
float rand(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453123);
}

vec3 computeLight( vec3 pos )
{
	vec3 light = vec3(0.0);
	
	vec3 V = normalize(u_camera_position - pos);

	float shadow_factor =  1.0;

	if(u_shadow_params.x != 0 && u_light_info.x != NO_LIGHT)
	{
		shadow_factor = testShadow(pos);
	}

	if(u_light_info.x == DIRECTIONAL_LIGHT)
	{
		light += u_light_color;
		
		light *= shadow_factor;
	}

	if(u_light_info.x == SPOT_LIGHT)
	{
		light += u_light_color;
		
		vec3 L = u_light_pos - pos;
		float dist = length(L);
		L /= dist;

		//LINEAR DISTANCE ATTENUATION
		float attenuation = u_light_info.z - dist;
		attenuation /= u_light_info.z;
		attenuation = max(attenuation, 0.0);

		float cos_angle = dot(u_light_front, L);
		if(cos_angle < u_light_cone.y)
			attenuation = 0.0;
		else if(cos_angle < u_light_cone.x)
		attenuation *= (cos_angle - u_light_cone.y) / (u_light_cone.x - u_light_cone.y);
		
		
		light *= attenuation * shadow_factor;
	}

	light += u_ambient_light;
	return light;
}

void main()
{
	vec2 uv = gl_FragCoord.xy * u_iRes.xy;
		
	float depth = texture(u_depth_texture, uv).r;
	
	vec4 screen_coord = vec4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 world_proj = u_ivp * screen_coord;
	vec3 world_pos = world_proj.xyz / world_proj.w;

	vec3 ray_start = u_camera_position;
	vec3 ray_dir = ( world_pos - ray_start );
	float ray_length = length(ray_dir);
	ray_dir /= ray_length;
	ray_dir = normalize(ray_dir);
	ray_length = min( 500.0, ray_length ); //max ray

	float step_dist = ray_length / float(SAMPLES);
	ray_start += ray_dir * rand(uv) * step_dist;

	vec3 current_pos = ray_start;
	vec3 ray_offset = ray_dir * step_dist;

	vec3 color = vec3(0.0);
	float transparency = 1.0;

	float air_step = u_air_density * step_dist;

	for(int i = 0; i < SAMPLES; ++i)
	{
		//evaluate contribution
		vec3 light = computeLight( current_pos );

		//accumulate the amount of light
		color += light * transparency * air_step;

		//advance to next position
		current_pos.xyz += ray_offset;

		//reduce visibility
		transparency -= air_step;

		//too dense, nothing can be seen behind
		if(transparency < 0.001)
			break;
	}

	FragColor = vec4(color, 1.0 - clamp(transparency, 0.0, 1.0));
}

\decal.fs
#version 330 core

uniform sampler2D u_depth_texture;
uniform sampler2D u_color_texture;

uniform mat4 u_ivp;
uniform mat4 u_imodel;
uniform vec2 u_iRes;

out vec4 FragColor;
void main()
{
	//vec2 uv = v_uv;
	vec2 uv = gl_FragCoord.xy * u_iRes.xy;
		
	float depth = texture(u_depth_texture, uv).r;

	if(depth == 1.0)
		discard;

	vec4 screen_coord = vec4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 world_proj = u_ivp * screen_coord;

	vec3 world_pos = world_proj.xyz / world_proj.w;

	vec3 decal_space = (u_imodel * vec4(world_pos, 1.0)).xyz;
	decal_space = decal_space + vec3(0.5);

	vec4 color = texture(u_color_texture, decal_space.xy);

	FragColor = color;
}

\gamma.fs
#version 330 core

in vec2 v_uv;

uniform sampler2D u_texture;
uniform float u_igamma; //inverse gamma

out vec4 FragColor;

void main() {
	vec4 color = texture2D( u_texture, v_uv );
	vec3 rgb = color.xyz;
	rgb = pow( rgb, vec3( u_igamma ) );
	FragColor = vec4( rgb, 1.0 );
}


\tonemapper.fs
#version 330 core

in vec2 v_uv;

uniform sampler2D u_texture;

uniform float u_scale; //color scale before tonemapper
uniform float u_average_lum; 
uniform float u_lumwhite2;
uniform float u_igamma; //inverse gamma

out vec4 FragColor;

void main() {
	vec4 color = texture2D( u_texture, v_uv );
	vec3 rgb = color.xyz;

	float lum = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
	float L = (u_scale / u_average_lum) * lum;
	float Ld = (L * (1.0 + L / u_lumwhite2)) / (1.0 + L);

	rgb = (rgb / lum) * Ld;
	rgb = max(rgb,vec3(0.001));
	rgb = pow( rgb, vec3( u_igamma ) );
	FragColor = vec4( rgb, 1.0 );
}

\uncharted_tonemapper.fs

#version 330 core
in vec2 v_uv;
uniform sampler2D u_texture;
out vec4 FragColor;

// Source http://filmicworlds.com/blog/filmic-tonemapping-operators/
const float A = 0.15;
const float B = 0.50;
const float C = 0.10;
const float D = 0.20;
const float E = 0.02;
const float F = 0.30;
const float W = 11.2;

vec3 gamma(const in vec3 color) {return pow(color, vec3(1.0/2.2)); }

vec3 Uncharted2TonemapPartial(vec3 x) {
   return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

void main() {
    vec3 color = texture(u_texture, v_uv).rgb;
    vec3 tonemapped_color = Uncharted2TonemapPartial(color * 2.0);
    vec3 W = vec3(11.2f);
    vec3 white_scale = vec3(1.0f) / Uncharted2TonemapPartial(W);
    FragColor = vec4(gamma(tonemapped_color * white_scale), 1.0);
}

\spherical_probe.fs

#version 330 core

#define NUM_PROBES 9

#include "SphericalHarmonics"

in vec3 v_world_position;
in vec3 v_normal;

uniform vec3 u_coeffs[NUM_PROBES];
out vec4 FragColor;

void main()
{
	vec4 color = vec4(1.0);
	vec3 N = normalize(v_normal);

	SH9Color sh;
	for(int i = 0; i < NUM_PROBES; ++i)
		sh.c[i] = u_coeffs[i];
	
	color.xyz = max(ComputeSHIrradiance(N, sh), vec3(0.0));
	FragColor = color;
}

\irradiance.fs

#version 330 core

#include "SphericalHarmonics"

uniform sampler2D u_albedo_texture;
uniform sampler2D u_normal_texture;
uniform sampler2D u_extra_texture;
uniform sampler2D u_depth_texture;

uniform vec3 u_irr_start;
uniform vec3 u_irr_end;
uniform vec3 u_irr_dims;
uniform int u_num_probes;

uniform float u_irr_normal_distance;
uniform vec3 u_irr_delta;

uniform sampler2D u_probes_texture;

uniform vec2 u_iRes;
uniform mat4 u_ivp;

uniform float u_irr_multiplier;

out vec4 FragColor;

void main()
{
	vec2 uv = gl_FragCoord.xy * u_iRes.xy;

	float depth = texture(u_depth_texture, uv).r;

	if(depth == 1.0)
		discard;

	vec4 screen_coord = vec4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	vec4 world_proj = u_ivp * screen_coord;

	vec3 world_pos = world_proj.xyz / world_proj.w;

	vec3 color = vec3(0.0);

	vec3 albedo = texture(u_albedo_texture, uv).rgb;
	vec3 emissive_light = texture(u_extra_texture, uv).rgb;
	vec4 normal = texture(u_normal_texture, uv);
	vec3 N = normalize(normal.xyz*2.0 - vec3(1.0));

	//computing nearest probe index based on world position
	vec3 irr_range = u_irr_end - u_irr_start;
	vec3 irr_local_pos = clamp( world_pos - u_irr_start + N * u_irr_normal_distance, //offset a little 
	vec3(0.0), irr_range );

	//convert from world pos to grid pos
	vec3 irr_norm_pos = irr_local_pos / u_irr_delta;

	//round values as we cannot fetch between rows for now
	vec3 local_indices = round( irr_norm_pos );

	//compute in which row is the probe stored
	float row = local_indices.x + 
	local_indices.y * u_irr_dims.x + 
	local_indices.z * u_irr_dims.x * u_irr_dims.y;

	//find the UV.y coord of that row in the probes texture
	float row_uv = (row + 1.0) / (u_num_probes + 1.0);

	SH9Color sh;

	//fill the coefficients
	const float d_uvx = 1.0 / 9.0;
	for(int i = 0; i < 9; ++i)
	{
		vec2 coeffs_uv = vec2( (float(i)+0.5) * d_uvx, row_uv );
		sh.c[i] = texture( u_probes_texture, coeffs_uv).xyz;
	}

	//now we can use the coefficients to compute the irradiance
	vec3 irradiance = max(vec3(0.0), ComputeSHIrradiance( N, sh ) * u_irr_multiplier);
	irradiance *= albedo.xyz;

	FragColor = vec4(irradiance, 1.0);
}


\reflectionProbe.fs

#version 330 core

in vec3 v_position;
in vec3 v_normal;
in vec3 v_world_position;

uniform samplerCube u_texture;
uniform vec3 u_camera_position;
out vec4 FragColor;

void main()
{
	vec3 N = normalize( v_normal );
	vec3 E = v_world_position - u_camera_position;
	vec3 R = reflect( E, N );
	vec4 color = textureLod( u_texture, R, 0.0 ) ;
	FragColor = color;
}


\mirror.fs

#version 330 core

in vec3 v_position;
in vec3 v_normal;
in vec3 v_world_position;

uniform sampler2D u_texture;
uniform vec3 u_camera_position;
uniform vec2 u_iRes;

out vec4 FragColor;



void main()
{
	vec2 uv = gl_FragCoord.xy * u_iRes;
	uv.x = 1.0 - uv.x;

	vec3 N = normalize( v_normal );
	vec3 E = v_world_position - u_camera_position;
	vec3 R = reflect( E, N );
	vec4 color = texture2D( u_texture, uv ) ;
	FragColor = color;
}
