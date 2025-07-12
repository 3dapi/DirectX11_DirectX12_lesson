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

class MainApp
{
protected:
	// Direct3D resources for cube geometry.
	ComPtr<ID3D12RootSignature>		m_rootSignature		{};
	ComPtr<ID3D12PipelineState>		m_pipelineState		{};

	ComPtr<ID3D12DescriptorHeap>	m_cbvHeap			{};
	ComPtr<ID3D12Resource>			m_cbv0rsc			{};
	uint8_t*						m_cbv0ptr			{};

	UINT							m_vtxCount			{};
	ComPtr<ID3D12Resource>			m_vtxGPU			{};			// vertex buffer default heap resource
	D3D12_VERTEX_BUFFER_VIEW		m_vtxView			{};

	UINT							m_idxCount			{};
	ComPtr<ID3D12Resource>			m_idxGPU			{};			// index buffer default heap resource
	D3D12_INDEX_BUFFER_VIEW			m_idxView			{};

	const UINT NUM_CBV = 1;								// 셰이더 상수 레지스터 숫자
	const UINT NUM_SRV = 1;								// 셰이더 텍스처 텍스처 레지스터
	const UINT NUM_CB_TX = NUM_CBV + NUM_SRV;

	ComPtr<ID3D12Resource>			m_textureRsc		{};		// assets/res_checker.png

public:
	MainApp();
	virtual ~MainApp();

	virtual int Init();
	virtual int Destroy();
	virtual int Update();
	virtual int Render();

	int InitDeviceResource();
	int SetupResource();
	int InitDefaultConstant();
};

#endif
