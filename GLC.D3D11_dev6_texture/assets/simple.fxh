//--------------------------------------------------------------------------------------
// Constant Buffer Variables

cbuffer MVP0 : register( b0 ) { matrix mtWld;}      // world matrix
cbuffer MVP1 : register( b1 ) { matrix mtViw;}      // view matrix
cbuffer MVP2 : register( b2 ) { matrix mtPrj;}      // projection matrix
cbuffer DIF0 : register( b3 ) { float4 diffMesh;}   // projection matrix

Texture2D    texDiff   : register(t0);              // diffuse texture
SamplerState smpLinear : register(s0);              // sampler state

//--------------------------------------------------------------------------------------
struct VS_IN
{
    float4 p : POSITION;
    float2 t : TEXCOORD0;
};
struct VS_OUT
{
    float4 p : SV_POSITION;
    float2 t : TEXCOORD0;
};


//--------------------------------------------------------------------------------------
// Vertex Shader

VS_OUT main_vtx( VS_IN v )
{
    VS_OUT o = (VS_OUT)0;
    o.p = mul( mtWld, v.p );
    o.p = mul( mtViw, o.p );
    o.p = mul( mtPrj, o.p );
    o.t = v.t;    
    return o;
}


//--------------------------------------------------------------------------------------
// Pixel Shader

float4 main_pxl( VS_OUT v) : SV_Target0
{
    float4 finalColor = texDiff.Sample( smpLinear, v.t ) * diffMesh;
    return finalColor;
}
