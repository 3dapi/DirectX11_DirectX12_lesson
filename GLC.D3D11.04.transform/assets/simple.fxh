//--------------------------------------------------------------------------------------
// Constant Buffer Variables

cbuffer MVP0 : register( b0 ) { matrix mtWld;} // world matrix
cbuffer MVP1 : register( b1 ) { matrix mtViw;} // view matrix
cbuffer MVP2 : register( b2 ) { matrix mtPrj;} // projection matrix

//--------------------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float4 dif : COLOR0;
};

//--------------------------------------------------------------------------------------
// main vertex shader

VS_OUTPUT main_vtx( float4 pos : POSITION0, float4 dif : COLOR0 )
{
    VS_OUTPUT vso = (VS_OUTPUT)0;
    vso.pos = mul( mtWld, pos);
    vso.pos = mul( mtViw, vso.pos);
    vso.pos = mul( mtPrj, vso.pos);
    vso.dif = dif;
    return vso;
}

//--------------------------------------------------------------------------------------
// main pixel shader

float4 main_pxl( VS_OUTPUT vsi) : SV_Target0
{
    return vsi.dif;
}
