//--------------------------------------------------------------------------------------
// Constant Buffer Variables

cbuffer MVP0 : register( b0 ) { matrix mtWld;} // world matrix
cbuffer MVP1 : register( b1 ) { matrix mtViw;} // view matrix
cbuffer MVP2 : register( b2 ) { matrix mtPrj;} // projection matrix
cbuffer LIGHTING : register( b3 ) {             //lighting
	float4 vLightDir[2];
	float4 vLightColor[2];
	float4 vOutputColor;
}

//--------------------------------------------------------------------------------------
struct VS_IN
{
    float4 pos : POSITION;
    float3 nor : NORMAL;
};
struct VS_OUT
{
    float4 pos : SV_POSITION;
    float3 nor : TEXCOORD0;
};


//--------------------------------------------------------------------------------------
// Vertex Shader

VS_OUT main_vtx( VS_IN vsi )
{
    VS_OUT vso = (VS_OUT)0;
    vso.pos = mul( vsi.pos, World );
    vso.pos = mul( vso.pos, View );
    vso.pos = mul( vso.pos, Projection );
    vso.nor = mul( float4( vsi.nor, 1 ), World ).xyz;
    
    return vso;
}


//--------------------------------------------------------------------------------------
// Pixel Shader

float4 main_pxl( VS_OUT vsi) : SV_Target0
{
    float4 finalColor = 0;
    
    //do NdotL lighting for 2 lights
    for(int i=0; i<2; i++)
    {
        finalColor += saturate( dot( (float3)vLightDir[i],vsi.nor) * vLightColor[i] );
    }
    finalColor.a = 1;
    return finalColor;
}

//--------------------------------------------------------------------------------------
// PSSolid - render a solid color

float4 main_pxl_solid( VS_OUT vsi) : SV_Target0
{
    return vOutputColor;
}
