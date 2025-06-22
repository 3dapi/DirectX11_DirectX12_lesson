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
using Microsoft::WRL::ComPtr;
using namespace DirectX;

struct ConstBufMVP
{
	DirectX::XMMATRIX m{};
	DirectX::XMMATRIX v{};
	DirectX::XMMATRIX p{};
	static const unsigned ALIGNED_SIZE;
};

class MainApp
{
protected:
	ComPtr<ID3D12RootSignature>		m_rootSignature		{};
	ComPtr<ID3D12PipelineState>		m_pipelineState		{};
	ComPtr<ID3D12DescriptorHeap>	m_cbvHeap			{};
	ComPtr<ID3D12Resource>			m_rscVtx			{};
	ComPtr<ID3D12Resource>			m_rscIdx			{};
	D3D12_VERTEX_BUFFER_VIEW		m_viewVtx			{};
	D3D12_INDEX_BUFFER_VIEW			m_viewIdx			{};
	int								m_bufVtxCount		{};
	int								m_bufIdxCount		{};

	ConstBufMVP						m_mtMVP			{};
	uint8_t*						m_csnstPtrMVP	{};
	ComPtr<ID3D12Resource>			m_cnstMVP		{};

public:
	MainApp();
	virtual ~MainApp();

	virtual int Init();
	virtual int Destroy();
	virtual int Update();
	virtual int Render();
};


#endif
