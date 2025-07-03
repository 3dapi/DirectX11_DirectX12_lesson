
cbuffer MVP0					: register(b0) { matrix tmMVP; }//					cmdList->SetGraphicsRootDescriptorTable(0, handleMVP);  // b0
Texture2D			gTex0		: register(t0);					// diffuse0			cmdList->SetGraphicsRootDescriptorTable(1, m_srvHandle); // t0
SamplerState		gSmpLinear	: register(s0);					// sampler state

//--------------------------------------------------------------------------------------
struct VS_IN
{
    float2 p : POSITION;
    float4 d : COLOR0;
    float2 t : TEXCOORD0;
};
struct PS_IN
{
    float4 p : SV_POSITION;
    float4 d : COLOR0;
    float2 t : TEXCOORD0;
};

// Vertex Shader
PS_IN main_vtx( VS_IN v )
{
    PS_IN o = (PS_IN) 0;
    o.p = mul( tmMVP, float4(v.p, 0.0F, 1.0F) );
	o.d = v.d;
	o.t = v.t;
	return o;
}

// Pixel Shader
float4 main_pxl( PS_IN v) : SV_Target0
{
	float4 t1 = gTex0.Sample(gSmpLinear, v.t);
	float4 o = t1 * v.d * 1.5F;
	return o;
}
