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
	// Constant buffers must be a multiple of the minimum hardware
	// allocation size (usually 256 bytes).  So round up to nearest
	// multiple of 256.  We do this by adding 255 and then masking off
	// the lower 2 bytes which store all bits < 256.
	// Example: Suppose byteSize = 300.
	// (300 + 255) & ~255
	// 555 & ~255
	// 0x022B & ~0x00ff
	// 0x022B & 0xff00
	// 0x0200
	// 512
	return (byteSize + 255) & ~255;
}


struct ConstBufMVP
{
	DirectX::XMFLOAT4X4 m{};
	DirectX::XMFLOAT4X4 v{};
	DirectX::XMFLOAT4X4 p{};
	static const unsigned ALIGNED_SIZE;
};

struct Vertex
{
	DirectX::XMFLOAT3 p;
	uint8_t d[4];
};


class MainApp
{
protected:
	UINT												m_d3dDescriptorSize{};


	// Direct3D resources for cube geometry.
	ComPtr<ID3D12DescriptorHeap>			m_cbvHeap;
	ComPtr<ID3D12RootSignature>				m_rootSignature;
	ComPtr<ID3D12PipelineState>				m_pipelineState;
	ComPtr<ID3D12Resource>					m_rscVtx;
	ComPtr<ID3D12Resource>					m_rscIdx;
	ComPtr<ID3D12Resource>					m_cnstMVP;
	ConstBufMVP								m_cnstBufMVP{};
	uint8_t*								m_csnstPtrMVP{};
	D3D12_VERTEX_BUFFER_VIEW				m_viewVtx;
	D3D12_INDEX_BUFFER_VIEW					m_viewIdx;

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
	void Rotate(float radians);
};


#endif
