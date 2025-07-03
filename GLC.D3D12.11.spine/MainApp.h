#pragma once
#ifndef _MainApp_H_
#define _MainApp_H_

#include <string>
#include <vector>
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <wrl.h>
#include <spine/spine.h>
#include "G2Base.h"
#include "G2Util.h"

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

class MainApp : public spine::TextureLoader
{
protected:
	// Direct3D resources for cube geometry.
	ComPtr<ID3D12DescriptorHeap>	m_cbvHeap			{};
	ComPtr<ID3D12RootSignature>		m_rootSignature		{};
	ComPtr<ID3D12PipelineState>		m_pipelineState		{};
	D3D12_VERTEX_BUFFER_VIEW		m_viewVtx			{};
	D3D12_INDEX_BUFFER_VIEW			m_viewIdx			{};
	UINT							m_numVtx			{};
	UINT							m_numIdx			{};
	D3D12_GPU_DESCRIPTOR_HANDLE		m_cbvHandle			{};
	ComPtr<ID3D12Resource>			m_rscVtxGPU			{};
	ComPtr<ID3D12Resource>			m_rscIdxGPU			{};
	ComPtr<ID3D12Resource>			m_textureRsc		{};		// assets/res_checker.png
	D3D12_GPU_DESCRIPTOR_HANDLE		m_textureHandle		{};		// checker SRV GPU 핸들

	ComPtr<ID3D12Resource>			m_cnstTmWld			{};
	uint8_t*						m_ptrWld			{};
	double							m_angle				{};

public:
	MainApp();
	virtual ~MainApp();

	virtual int Init();
	virtual int Destroy();
	virtual int Update();
	virtual int Render();

	int InitResource();
	int InitConstValue();

public:
	void load(spine::AtlasPage& page, const spine::String& path) override;
	void unload(void* texture) override;
};

#endif
