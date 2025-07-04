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
	ComPtr<ID3D12DescriptorHeap>	m_cbvHeap			{};
	ComPtr<ID3D12RootSignature>		m_rootSignature		{};
	ComPtr<ID3D12PipelineState>		m_pipelineState		{};
	D3D12_VERTEX_BUFFER_VIEW		m_viewVtx			{};
	D3D12_INDEX_BUFFER_VIEW			m_viewIdx			{};
	UINT							m_numVtx			{};
	UINT							m_numIdx			{};
	D3D12_GPU_DESCRIPTOR_HANDLE		m_cbvHandle			{};

	ComPtr<ID3D12Resource>			m_rscVtxGPU			{};			// vertex buffer default heap resource
	ComPtr<ID3D12Resource>			m_rscVtxCPU			{};			// vertex buffer upload heap resource
	ComPtr<ID3D12Resource>			m_rscIdxGPU			{};			// index buffer default heap resource
	ComPtr<ID3D12Resource>			m_rscIdxCPU			{};			// index buffer upload heap resource

	ComPtr<ID3D12Resource>			m_spineTextureRsc	{};		// assets/res_checker.png
	D3D12_GPU_DESCRIPTOR_HANDLE		m_spineTexture		{};		// checker SRV GPU 핸들

	ComPtr<ID3D12Resource>			m_cnstMVP			{};
	uint8_t*						m_ptrMVP			{};

public:
	MainApp();
	virtual ~MainApp();

	virtual int Init();
	virtual int Destroy();
	virtual int Update();
	virtual int Render();

	int InitResource();
	int InitConstValue();
};

#endif
