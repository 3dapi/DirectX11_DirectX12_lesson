//--------------------------------------------------------------------------------------
// main vertex shader

float4 main_vtx( float4 Pos : POSITION ) : SV_POSITION
{
    return Pos;
}


//--------------------------------------------------------------------------------------
// main pixel shader

float4 main_pxl( float4 Pos : SV_POSITION ) : SV_Target0
{
    return float4( 1.0f, 0.0f, 1.0f, 1.0f );
}
