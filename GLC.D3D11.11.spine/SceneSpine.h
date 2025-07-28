#pragma once
#ifndef _SceneSpine_H_
#define _SceneSpine_H_

#include <string>
#include <vector>
#include <Windows.h>
#include <d3d11.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <wrl.h>
#include <spine/spine.h>
#include "G2Base.h"
#include "G2Util.h"

using namespace std;
using namespace DirectX;
using Microsoft::WRL::ComPtr;

struct Vertex
{
	XMFLOAT2 p;
	union
	{
		uint8_t d[4];
		uint32_t c;
	};
	XMFLOAT2 t;
};

struct DRAW_BUFFER {
	UINT			numVb		{};		// vertex count
	UINT			numIb		{};		// index count
	ID3D11Buffer*	vbvPos		{};		// position buffer
	ID3D11Buffer*	vbvDif		{};		// diffuse buffer
	ID3D11Buffer*	vbvTex		{};		// texture buffer
	ID3D11Buffer*	ibv			{};		// index buffer

	~DRAW_BUFFER();
	int Setup(ID3D11Device* d3dDevice, UINT countVtx, UINT countIdx);
};


class SceneSpine : public spine::TextureLoader
{
protected:
	ID3D11VertexShader*			m_shaderVtx		{};
	ID3D11PixelShader*			m_shaderPxl		{};
	ID3D11InputLayout*			m_vtxLayout		{};

	UINT						m_maxVtxCount	{};
	UINT						m_maxIdxCount	{};
	vector<DRAW_BUFFER>			m_drawBuf		;
	int							m_drawCount		{};

	XMMATRIX					m_mtMVP			= XMMatrixIdentity();
	ID3D11Buffer*				m_cnstMVP		{};

	ID3D11ShaderResourceView*	m_srvTexture	{};		// spine texture resource
	ID3D11SamplerState*			m_sampLinear	{};

	ID3D11BlendState*			m_rsAlpha		{};
	ID3D11DepthStencilState*	m_rsZwrite		{};
	ID3D11DepthStencilState*	m_rsZtest		{};

	// spine instance
	vector<string>				m_spineAnimations	;
	string						m_spineTextureName	;
	spine::Skeleton*			m_spineSkeleton			{};
	spine::AnimationStateData*	m_spineAniStateData		{};
	spine::AnimationState*		m_spineAniState			{};
	spine::SkeletonData*		m_spineSkeletonData		{};
	spine::Atlas*				m_spineAtlas			{};

public:
	SceneSpine();
	virtual ~SceneSpine();

	virtual int Init();
	virtual int Destroy();
	virtual int Update(float deltaTime);
	virtual int Render();
protected:
	int		InitSpine(const string& str_atlas, const string& str_skel);
	int		InitForDevice();
	int		UpdateDrawBuffer();
	void*	TextureLoad(const string& fileName);
	void	TextureUnload(void* texture);
	void	load(spine::AtlasPage& page, const spine::String& path) override;
	void	unload(void* texture) override;
};

#endif
