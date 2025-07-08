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
using namespace std;
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

struct ConstHeap
{
	ID3D12DescriptorHeap*	cbvHeap	{};
	ID3D12Resource*			cnstMVP	{};
	uint8_t*				ptrMVP	{};
	XMMATRIX				tmWorld	;
	~ConstHeap();
};

class MainApp
{
protected:
	// Direct3D resources for cube geometry.
	ComPtr<ID3D12RootSignature>		m_rootSignature		{};
	ComPtr<ID3D12PipelineState>		m_pipelineState		{};
	D3D12_VERTEX_BUFFER_VIEW		m_viewVtx			{};
	D3D12_INDEX_BUFFER_VIEW			m_viewIdx			{};
	UINT							m_numVtx			{};
	UINT							m_numIdx			{};

	ComPtr<ID3D12Resource>			m_rscVtx			{};
	ComPtr<ID3D12Resource>			m_rscIdx			{};
	ConstBufMVP						m_bufMVP			{};
	vector< ConstHeap>				m_constHeap			{5,  ConstHeap{} };

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
