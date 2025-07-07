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
using namespace DirectX;
using namespace Microsoft::WRL;

constexpr inline unsigned ConstantBufferByteSize(unsigned byteSize)
{
	return (byteSize + 255) & ~255;
}

struct ConstBufMVP
{
	XMMATRIX m{};
	XMMATRIX v{};
	XMMATRIX p{};
	static const unsigned ALIGNED_SIZE;
};

struct Vertex
{
	XMFLOAT3 p;
	uint8_t d[4];
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

	ComPtr<ID3D12Resource>			m_rscVtx			{};
	ComPtr<ID3D12Resource>			m_rscIdx			{};

	ComPtr<ID3D12Resource>			m_cnstMVP			{};
	ConstBufMVP						m_cnstBufMVP		{};
	uint8_t*						m_csnstPtrMVP		{};

	UINT							m_numObject			{2};		// rendering object
	XMMATRIX						m_tmWorld2			= XMMatrixIdentity();

	// Variables used with the rendering loop.
	float	m_radiansPerSecond	{XM_PIDIV4};
	float	m_angle{};
	bool	m_tracking{};
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
