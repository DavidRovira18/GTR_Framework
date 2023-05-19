//example of some shaders compiled
flat basic.vs flat.fs
texture basic.vs texture.fs
skybox basic.vs skybox.fs
depth quad.vs depth.fs
multi basic.vs multi.fs

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


//TONEMAPPER
tonemapper quad.vs tonemapper.fs
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
out vec4 FragColor;

void main()
{
	vec3 E = v_world_position - u_camera_position;
	vec4 color = texture( u_texture, E );
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
			vec3 R = normalize(-reflect(u_light_front, N));

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

uniform vec3 u_ambient_light;

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

	albedo *= u_ambient_light * occlusion;
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
	float roughness = texture(u_extra_texture, uv).b;

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
	float roughness = texture(u_extra_texture, uv).b;

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

\tonemapper.fs
#version 330 core

in vec2 v_uv;

uniform sampler2D u_texture;

out vec4 FragColor;

uniform float u_scale; //color scale before tonemapper
uniform float u_average_lum; 
uniform float u_lumwhite2;
uniform float u_igamma; //inverse gamma


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
