
cbuffer MVP0					: register(b0) { matrix mtWld; }//					cmdList->SetGraphicsRootDescriptorTable(0, handleWld);  // b0
cbuffer MVP1					: register(b1) { matrix mtViw; }//					cmdList->SetGraphicsRootDescriptorTable(1, handleViw);  // b1
cbuffer MVP2					: register(b2) { matrix mtPrj; }//					cmdList->SetGraphicsRootDescriptorTable(2, handlePrj);  // b2

Texture2D			gTex0		: register(t0);					// diffuse0			cmdList->SetGraphicsRootDescriptorTable(3, m_srvHandle); // t0
Texture2D			gTex1		: register(t1);					// diffuse1			cmdList->SetGraphicsRootDescriptorTable(4, m_srvHandle); // t1
SamplerState		gSmpLinear	: register(s0);					// sampler state

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
	float4 t1 = gTex0.Sample( gSmpLinear, v.t );
	float4 t2 = gTex1.Sample( gSmpLinear, v.t );
	float4 o  = t1 * t2 * v.d*2.0F;
	return o;
}
