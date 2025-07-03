
Texture2D			gTexDif   : register(t0);              // diffuse texture
SamplerState		gSmpLinear : register(s0);              // sampler state

cbuffer MVP0 : register(b0)
{
	matrix mtWld;
}

cbuffer MVP0 : register(b1)
{
	matrix mtViw;
	matrix mtPrj;
}

struct VS_IN
{
    float4 p : POSITION;
    float4 d : COLOR;
    float2 t : TEXCOORD0;
};
struct PS_IN
{
	float4 p : SV_POSITION;
	float4 d : COLOR;
	float2 t : TEXCOORD;
};

// main vertex shader
PS_IN main_vs(VS_IN v)
{
	PS_IN o = (PS_IN) 0;
	o.p = mul(mtWld, v.p);
	o.p = mul(mtViw, o.p);
	o.p = mul(mtPrj, o.p);
	o.d = v.d;
	o.t = v.t;
	return o;
}

// main pixel shader
float4 main_ps(PS_IN v) : SV_Target0
{
	float4 finalColor;
	finalColor = gTexDif.Sample( gSmpLinear, v.t );
    finalColor *= v.d;
    return finalColor;
}
