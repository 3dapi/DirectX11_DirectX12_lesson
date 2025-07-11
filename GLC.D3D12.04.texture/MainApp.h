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

	ComPtr<ID3D12DescriptorHeap>	m_dscCbvHeap		{};
	D3D12_GPU_DESCRIPTOR_HANDLE		m_descHandle		{};
	ComPtr<ID3D12Resource>			m_cbv0rsc			{};
	uint8_t*						m_cbv0ptr			{};

	ComPtr<ID3D12Resource>			m_textureRsc		{};		// assets/res_checker.png
	D3D12_GPU_DESCRIPTOR_HANDLE		m_srvTex0			{};		// checker SRV GPU 핸들

	UINT							m_vtxCount			{};
	ComPtr<ID3D12Resource>			m_vtxGPU			{};			// vertex buffer default heap resource
	D3D12_VERTEX_BUFFER_VIEW		m_vtxView			{};

	UINT							m_idxCount			{};
	ComPtr<ID3D12Resource>			m_idxGPU			{};			// index buffer default heap resource
	D3D12_INDEX_BUFFER_VIEW			m_idxView			{};

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
