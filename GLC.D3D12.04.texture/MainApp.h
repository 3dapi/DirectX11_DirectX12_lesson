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
	XMFLOAT3 p;
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
	ComPtr<ID3D12Resource>			m_rscVtx			{};
	ComPtr<ID3D12Resource>			m_rscIdx			{};

	XMMATRIX						m_tmWld				= XMMatrixIdentity();
	ComPtr<ID3D12Resource>			m_cnstTmWld			{};
	uint8_t*						m_ptrWld			{};
	XMMATRIX						m_tmViw				= XMMatrixIdentity();
	ComPtr<ID3D12Resource>			m_cnstTmViw			{};
	uint8_t*						m_ptrViw			{};
	XMMATRIX						m_tmPrj				= XMMatrixIdentity();
	ComPtr<ID3D12Resource>			m_cnstTmPrj			{};
	uint8_t*						m_ptrPrj			{};

	double							m_angle				{};

	ComPtr<ID3D12Resource>			m_textureChecker	{};		// assets/res_checker.png
	ComPtr<ID3D12Resource>			m_textureXlogo		{};		// assets/xlogo.png
	ComPtr<ID3D12DescriptorHeap>	m_srvHeap2x			{};		// 텍스처용 힙
	D3D12_GPU_DESCRIPTOR_HANDLE		m_srvHandleChecker	{};		// checker SRV GPU 핸들
	D3D12_GPU_DESCRIPTOR_HANDLE		m_srvHandleXlogo	{};		// checker SRV GPU 핸들
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
