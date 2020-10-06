// Upgrade NOTE: replaced 'mul(UNITY_MATRIX_MVP,*)' with 'UnityObjectToClipPos(*)'

// Upgrade NOTE: replaced '_World2Object' with 'unity_WorldToObject'
// Upgrade NOTE: replaced 'mul(UNITY_MATRIX_MVP,*)' with 'UnityObjectToClipPos(*)'

// Upgrade NOTE: replaced 'mul(UNITY_MATRIX_MVP,*)' with 'UnityObjectToClipPos(*)'

Shader "PointCloudViewer/PointCloudShader_720p"
{
	Properties
	{
		_MainTex("Texture", 2D) = "white" {}
		_DepthTex("Depth Channel", 2D) = "black" {}
		_ColorTex("Color Channel", 2D) = "black" {}
		_ProjectionTex("Point Projection Channel", 2D) = "black" {}
		_PointSize("Point Size", Float) = 0.002
	
		//_Alpha("Alpha", Range(0.00, 1.00)) = 0.

		[Toggle] _PinholeCamera("Assume pinhole camera?", Float) = 0
		_FocalLengthX("Focal Length X", Range(0.01, 1920.00)) = 603.584
		_FocalLengthY("Focal Length Y", Range(0.01, 1080.00)) = 603.401
		_PrincipalPointX("Principal Point X", Float) = 640.951
		_PrincipalPointY("Principal Point Y", Float) = 366.111
		_Width("Width", Float) = 1280
		_Height("Height", Float) = 720

			// can convert from meters to another unit (defaults to 1 m)
			_MetricMultiplier("MetricMultiplier", Float) = 1
	}
		SubShader
		{
			Tags { "RenderType" = "Opaque" }
			//Blend SrcAlpha OneMinusSrcAlpha
			//Tags { "Queue" = "Transparent" "IgnoreProjector" = "True"  "RenderType" = "Transparent" }

			Pass
			{
				CGPROGRAM
				#pragma vertex vert
				#pragma fragment frag alpha
				//#pragma geometry geom
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
				half _PointSize;
				//float _Alpha;

				float _MetricMultiplier;
				float _PrincipalPointX, _PrincipalPointY;
				float _FocalLengthX, _FocalLengthY;
				float _Width, _Height;
				float _PinholeCamera;

				v2g vert(appdata v)
				{
					v2g o;

					float2 inv_uv = float2(v.uv.x, 1.0 - v.uv.y);
					half depth = tex2Dlod(_DepthTex, float4(inv_uv, 0, 0)).r * 65.536 * _MetricMultiplier;

					if (_PinholeCamera > 0.5)
					{

						const float invfocalLengthx = 1.f / _FocalLengthX;
						const float invfocalLengthy = 1.f / _FocalLengthY;


						v.vertex.x = (inv_uv.x * _Width - _PrincipalPointX) * depth * invfocalLengthx;
						v.vertex.y = (inv_uv.y * _Height - _PrincipalPointY) * depth * invfocalLengthy;
						v.vertex.z = depth;
					}
					else {
						float4 projV = tex2Dlod(_ProjectionTex, float4(inv_uv, 0, 0));
						float4 a = float4(projV.x * depth, projV.y * depth, 1.0 * depth, 0.0);
						v.vertex.x = projV.x * depth;// *depth;
						v.vertex.y = projV.y * depth;
						v.vertex.z = depth;
					}
					o.uv = TRANSFORM_TEX(inv_uv, _MainTex);
					v.vertex = UnityObjectToClipPos(v.vertex);
					o.vertex = v.vertex;
					o.psize = _PointSize;

					return o;
				}


				fixed4 frag(v2g i) : SV_Target
				{
					// sample the texture
					/*fixed4 col = tex2D(_MainTex, i.uv);*/

					// To show DEPTH

					float2 inv_uv = float2(i.uv.x, 1-i.uv.y);

					//float4 projV = tex2Dlod(_ProjectionTex, float4(i.uv, 0, 0));
					//float2 inv_uv = float2(i.vertex.x, i.vertex.y);
					//half depth = tex2Dlod(_DepthTex, float4(i.uv, 0, 0)).r;
					//depth =  (depth * 255) / 1000.0f;
					//depth *= 65.536;
					//fixed4 col = fixed4(depth/255, depth/255, depth/255, 1.0);
					

					//if (depth > 1.5)
					//{
					//	col.r = 1.0f;
					//}
					
					
					// To show COLOR (alpha component only matters when not using geometry shader)
					//col =  tex2D(_ColorTex, i.uv);
					
					fixed4 col = tex2D(_ColorTex, inv_uv);
					//col.a = 255;

				return col;
				}
			ENDCG
		}
		}
}