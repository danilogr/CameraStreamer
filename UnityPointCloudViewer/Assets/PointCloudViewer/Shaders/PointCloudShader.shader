﻿// Upgrade NOTE: replaced 'mul(UNITY_MATRIX_MVP,*)' with 'UnityObjectToClipPos(*)'

// Upgrade NOTE: replaced '_World2Object' with 'unity_WorldToObject'
// Upgrade NOTE: replaced 'mul(UNITY_MATRIX_MVP,*)' with 'UnityObjectToClipPos(*)'

// Upgrade NOTE: replaced 'mul(UNITY_MATRIX_MVP,*)' with 'UnityObjectToClipPos(*)'

Shader "PointCloudViewer/PointCloudShader"
{
	Properties
	{
		_MainTex("Texture", 2D) = "white" {}
		_DepthTex("Depth Channel", 2D) = "black" {}
		_ColorTex("Color Channel", 2D) = "black" {}
		_ProjectionTex("Point Projection Channel", 2D) = "black" {}
		_PointSize("Point Size", Float) = 0.0005
		_Alpha("Alpha", Range(0.00, 1.00)) = 0.8
	}
		SubShader
		{
			Blend SrcAlpha OneMinusSrcAlpha
			Tags { "Queue" = "Transparent" "IgnoreProjector" = "True"  "RenderType" = "Transparent" }
			
			Pass
			{
				CGPROGRAM
				#pragma vertex vert
				#pragma fragment frag alpha
				#pragma geometry geom
				// make fog work
				//#pragma multi_compile_fog
				#pragma multi_compile_fog
				#pragma multi_compile _ UNITY_COLORSPACE_GAMMA

				#include "UnityCG.cginc"

				#define CVC 36 // CubeVectexCount

				struct appdata
				{
					float4 vertex : POSITION;
					float2 uv : TEXCOORD0;
				};

				struct v2g
				{
					float2 uv : TEXCOORD0;
					half psize : PSIZE;
					float4 vertex : SV_POSITION;
					UNITY_FOG_COORDS(0)
				};

				struct g2f
				{
					float2 uv : TEXCOORD0;
					float4 vertex : SV_POSITION;
				};

				sampler2D _MainTex;
				sampler2D _DepthTex;
				sampler2D _ColorTex;
				sampler2D _ProjectionTex;
				float4 _MainTex_ST;
				uniform float4 _DepthTex_TexelSize;
				//uniform float _PointCastTable[640*576];
				half _PointSize;
				float _Alpha;

				v2g vert(appdata v)
				{
					v2g o;

					//float depth = tex2D(_DepthTex, v.uv).a;
					//v.vertex.z = depth;
					//float2 pCoord;
					//float2 uv = (pCoord + 0.5) * _DepthTex_TexelSize.xy;

					float2 inv_uv = float2(v.uv.x, 1.0-v.uv.y);
					float depth = tex2Dlod(_DepthTex, float4(inv_uv, 0, 0)).r;

					float4 projV = tex2Dlod(_ProjectionTex, float4(inv_uv, 0, 0));

					//float3 nProjV = normalize(float3(projV.x, projV.y, 1.0f));

					//float4 a = float4(nProjV.x * depth, nProjV.y * depth, nProjV.z * depth, 0.0);
					float4 a = float4(-1.0 * projV.x * depth, -1.0 * projV.y * depth, 1.0 * depth, 0.0);
					v.vertex.x = a.x * 1000.0;
					v.vertex.y = a.y * 1000.0;
					v.vertex.z = a.z * 1000.0;
					/*v.vertex.x = 100.0f;
					v.vertex.y = 0.0f;
					v.vertex.z = 0.0f;*/

					//v.vertex.z = depth * -100.0;
					o.vertex = v.vertex;
					//o.vertex.z = depth;
					o.psize = _PointSize;
					o.uv = TRANSFORM_TEX(v.uv, _MainTex);

					return o;
				}

				// "point" as input, output cube
				[maxvertexcount(CVC)] // CVC refers to Cube Vectex Count - 36
				void geom(point v2g p[1], inout TriangleStream<g2f> triStream)
				{
					float f = _PointSize / 20.0f; //half size

					const float4 vc[CVC] = { float4(-f,  f,  f, 0.0f), float4(f,  f,  f, 0.0f), float4(f,  f, -f, 0.0f),    //Top                                 
											 float4(f,  f, -f, 0.0f), float4(-f,  f, -f, 0.0f), float4(-f,  f,  f, 0.0f),    //Top

											 float4(f,  f, -f, 0.0f), float4(f,  f,  f, 0.0f), float4(f, -f,  f, 0.0f),     //Right
											 float4(f, -f,  f, 0.0f), float4(f, -f, -f, 0.0f), float4(f,  f, -f, 0.0f),     //Right

											 float4(-f,  f, -f, 0.0f), float4(f,  f, -f, 0.0f), float4(f, -f, -f, 0.0f),     //Front
											 float4(f, -f, -f, 0.0f), float4(-f, -f, -f, 0.0f), float4(-f,  f, -f, 0.0f),     //Front

											 float4(-f, -f, -f, 0.0f), float4(f, -f, -f, 0.0f), float4(f, -f,  f, 0.0f),    //Bottom                                         
											 float4(f, -f,  f, 0.0f), float4(-f, -f,  f, 0.0f), float4(-f, -f, -f, 0.0f),     //Bottom

											 float4(-f,  f,  f, 0.0f), float4(-f,  f, -f, 0.0f), float4(-f, -f, -f, 0.0f),    //Left
											 float4(-f, -f, -f, 0.0f), float4(-f, -f,  f, 0.0f), float4(-f,  f,  f, 0.0f),    //Left

											 float4(-f,  f,  f, 0.0f), float4(-f, -f,  f, 0.0f), float4(f, -f,  f, 0.0f),    //Back
											 float4(f, -f,  f, 0.0f), float4(f,  f,  f, 0.0f), float4(-f,  f,  f, 0.0f)     //Back
					};


					const float2 UV1[CVC] = { float2(0.0f,    0.0f), float2(1.0f,    0.0f), float2(1.0f,    0.0f),         //Esta em uma ordem
											  float2(1.0f,    0.0f), float2(1.0f,    0.0f), float2(1.0f,    0.0f),         //aleatoria qualquer.

											  float2(0.0f,    0.0f), float2(1.0f,    0.0f), float2(1.0f,    0.0f),
											  float2(1.0f,    0.0f), float2(1.0f,    0.0f), float2(1.0f,    0.0f),

											  float2(0.0f,    0.0f), float2(1.0f,    0.0f), float2(1.0f,    0.0f),
											  float2(1.0f,    0.0f), float2(1.0f,    0.0f), float2(1.0f,    0.0f),

											  float2(0.0f,    0.0f), float2(1.0f,    0.0f), float2(1.0f,    0.0f),
											  float2(1.0f,    0.0f), float2(1.0f,    0.0f), float2(1.0f,    0.0f),

											  float2(0.0f,    0.0f), float2(1.0f,    0.0f), float2(1.0f,    0.0f),
											  float2(1.0f,    0.0f), float2(1.0f,    0.0f), float2(1.0f,    0.0f),

											  float2(0.0f,    0.0f), float2(1.0f,    0.0f), float2(1.0f,    0.0f),
											  float2(1.0f,    0.0f), float2(1.0f,    0.0f), float2(1.0f,    0.0f)
					};

					const int TRI_STRIP[CVC] = { 0, 1, 2,  3, 4, 5,
												   6, 7, 8,  9,10,11,
												  12,13,14, 15,16,17,
												  18,19,20, 21,22,23,
												  24,25,26, 27,28,29,
												  30,31,32, 33,34,35
					};

					g2f v[CVC];
					int i;

					// Assign new vertices positions in view space
					for (i = 0; i < CVC; i++) {
						v[i].vertex = UnityObjectToClipPos(p[0].vertex + vc[i]);
						v[i].uv = p[0].uv;
					}

					// Build the cube tile by submitting triangle strip vertices
					for (i = 0; i < CVC / 3; i++)
					{
						triStream.Append(v[TRI_STRIP[i * 3 + 0]]);
						triStream.Append(v[TRI_STRIP[i * 3 + 1]]);
						triStream.Append(v[TRI_STRIP[i * 3 + 2]]);

						triStream.RestartStrip();
					}

				}

				fixed4 frag(g2f i) : SV_Target
				{
					// sample the texture
					/*fixed4 col = tex2D(_MainTex, i.uv);*/

					// To show DEPTH
					//float2 inv_uv = float2(i.uv.x, 1.0 - i.uv.y);
					//float depth = tex2D(_DepthTex, inv_uv).r;
					//fixed4 col = fixed4(depth, depth, depth, 1.0);

					// To show COLOR (alpha component only matters when not using geometry shader)
					fixed4 col = tex2D(_ColorTex, i.uv);
					col.a = _Alpha;

				return col;
				}
			ENDCG
		}
		}
}
