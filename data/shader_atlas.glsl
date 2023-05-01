//example of some shaders compiled
flat basic.vs flat.fs
texture basic.vs texture.fs
skybox basic.vs skybox.fs
depth quad.vs depth.fs
multi basic.vs multi.fs

texture_improved basic.vs texture_improved.fs
lights_multi basic.vs lights_multi.fs
lights_single basic.vs lights_single.fs

\basic.vs

#version 330 core

in vec3 a_vertex;
in vec3 a_normal;
in vec2 a_coord;
in vec4 a_color;

uniform vec3 u_camera_pos;

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

uniform vec3 u_camera_pos;

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

uniform vec3 u_ambient_light;

uniform float u_time;
uniform float u_alpha_cutoff;

out vec4 FragColor;

void main()
{
	vec2 uv = v_uv;
	vec4 albedo = u_color;
	albedo *= texture( u_albedo_texture, v_uv );

	if(albedo.a < u_alpha_cutoff)
		discard;

	vec3 emissive_light = u_emissive_factor * texture(u_emissive_texture, v_uv).xyz;
	albedo.xyz += emissive_light;

	float occlusion = texture(u_metallic_roughness_texture, v_uv).r;
	vec3 ambient_light = u_ambient_light * occlusion;

	albedo.xyz *= ambient_light; 
	FragColor = albedo;
}

\lights_multi.fs

#version 330 core

in vec3 v_position;
in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_color;

uniform vec3 u_camera_pos;
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

uniform vec3 u_light_pos;
uniform vec3 u_light_front;
uniform vec2 u_light_cone; //cos(min_angle), cos(max_angle)
uniform vec3 u_ambient_light;
uniform vec3 u_light_color;

uniform vec4 u_light_info; //vec4(light_type, near_distance, max_distance, enable_specular)

//shadowmap
uniform vec2 u_shadow_params; // 0 o 1 shadowmap or not, bias
uniform sampler2D u_shadowmap;
uniform mat4 u_shadow_viewproj;

//global properties

uniform float u_time;
uniform float u_alpha_cutoff;

out vec4 FragColor;

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

void main()
{
	vec2 uv = v_uv;
	vec4 albedo = u_color;
	albedo *= texture( u_albedo_texture, v_uv );

	float occlusion = texture(u_metallic_roughness_texture, v_uv).r;
	float ks = texture(u_metallic_roughness_texture, v_uv).g;
	float alpha = 1 - texture(u_metallic_roughness_texture, v_uv).b;

	if(albedo.a < u_alpha_cutoff)
		discard;

	vec3 light = vec3(0.0);

	//vec3 N = normalize(v_normal);
	
	vec3 normal_pixel = texture2D(u_normal_texture, v_uv).xyz;
	vec3 N = perturbNormal(v_normal, v_world_position, v_uv, normal_pixel);
	
	vec3 V = normalize(v_position - u_camera_pos);

	float shadow_factor =  1.0;

	if(u_shadow_params.x != 0)
	{
		shadow_factor = testShadow(v_world_position);
	}

	if(u_light_info.x == DIRECTIONAL_LIGHT)
	{
		//Diffuse light
		float NdotL = dot(N,u_light_front);
		light += max(NdotL, 0.0) * u_light_color;

		//Specular light
		if(u_light_info.a == 1)
		{
			vec3 R = normalize(reflect(u_light_front, N));

			float RdotV = max(dot(V,R), 0.0);

			light += ks * pow(RdotV, alpha) * u_light_color;
		}
	}

	else if(u_light_info.x == POINT_LIGHT || u_light_info.x == SPOT_LIGHT)
	{
		//BASIC PHONG
		vec3 L = u_light_pos - v_world_position;
		float dist = length(L);
		L /= dist;

		//Diffuse light
		float NdotL = dot(N,L);

		light += max(NdotL, 0.0) * u_light_color;

		//Specular light
		if(u_light_info.a == 1)
		{
			vec3 R = normalize(reflect(L, N));

			float RdotV = max(dot(V,R), 0.0);

			light += ks * pow(RdotV, alpha) * u_light_color;
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
		
		light = light * attenuation * shadow_factor;
	}

	light += u_ambient_light * occlusion; 

	vec3 color = albedo.xyz * light;

	vec3 emissive_light = u_emissive_factor * texture(u_emissive_texture, v_uv).xyz;
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

uniform vec3 u_camera_pos;
//material properties

uniform vec4 u_color;
uniform sampler2D u_albedo_texture;
uniform sampler2D u_emissive_texture;
uniform sampler2D u_metallic_roughness_texture;
uniform vec3 u_emissive_factor;

//light properties

#define NO_LIGHT 0.0
#define POINT_LIGHT 1.0
#define SPOT_LIGHT 2.0
#define DIRECTIONAL_LIGHT 3.0

uniform vec3 u_ambient_light;

const int MAX_LIGHTS = 12;
uniform int u_num_lights;
uniform vec3 u_lights_pos[MAX_LIGHTS];
uniform vec3 u_lights_color[MAX_LIGHTS];
uniform vec4 u_lights_info[MAX_LIGHTS];

//global properties

uniform float u_time;
uniform float u_alpha_cutoff;

out vec4 FragColor;

void main()
{
	vec2 uv = v_uv;
	vec4 albedo = u_color;
	albedo *= texture( u_albedo_texture, v_uv );

	float alpha = 1 - texture(u_metallic_roughness_texture, v_uv).b;
	float ks = texture(u_metallic_roughness_texture, v_uv).g;

	if(albedo.a < u_alpha_cutoff)
		discard;

	vec3 light = vec3(0.0);

	vec3 N = normalize(v_normal);

	vec3 V = normalize(v_position - u_camera_pos);

	for( int i = 0; i < MAX_LIGHTS; i++ )
	{
		if(i <= u_num_lights)
		{
			if(u_lights_info[i].x == POINT_LIGHT)
			{
				//BASIC PHONG
				vec3 L = u_lights_pos[i] - v_world_position;
				float dist = length(L);
				L /= dist;

				//Diffuse light
				float NdotL = dot(N,L);

				light += max(NdotL, 0.0) * u_lights_color[i];

				//LINEAR DISTANCE ATTENUATION
				float attenuation = u_lights_info[i].z - dist;
				attenuation /= u_lights_info[i].z;
				attenuation = max(attenuation, 0.0);

				light = light * attenuation;
			}
		}
	}

	light += u_ambient_light; 

	vec3 color = albedo.xyz * light;

	vec3 emissive_light = u_emissive_factor * texture(u_emissive_texture, v_uv).xyz;
	color += emissive_light;

	FragColor = vec4(color, albedo.a);
}
