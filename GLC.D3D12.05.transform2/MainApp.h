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

using namespace std;
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

struct CBV_RPL
{
	ID3D12Resource*	rsc	{};
	uint8_t*		ptr	{};		// cbv rsc handle
	UINT			len	{};		// 256-aligned buffer size. per-constant CBV resource per object (e.g., world/view/proj)
};

struct ConstHeap
{
	ID3D12DescriptorHeap*				dscCbvHeap	{};
	D3D12_GPU_DESCRIPTOR_HANDLE			descHandle	{};
	vector<CBV_RPL>						cbvRpl		;

	vector<D3D12_GPU_DESCRIPTOR_HANDLE>	srvTex		{};
	~ConstHeap();
	void setupRpl(const vector<UINT>& rplSize);
};

struct ConstObject
{
	XMMATRIX		tmWld		{};
	XMMATRIX		tmViw		= XMMatrixIdentity();
	XMMATRIX		tmPrj		= XMMatrixIdentity();
};

class MainApp
{
protected:
	// Direct3D resources for cube geometry.
	ComPtr<ID3D12RootSignature>		m_rootSignature		{};
	ComPtr<ID3D12PipelineState>		m_pipelineState		{};

	UINT							m_vtxCount			{};
	ComPtr<ID3D12Resource>			m_vtxGPU			{};			// vertex buffer default heap resource
	D3D12_VERTEX_BUFFER_VIEW		m_vtxView			{};

	UINT							m_idxCount			{};
	ComPtr<ID3D12Resource>			m_idxGPU			{};			// index buffer default heap resource
	D3D12_INDEX_BUFFER_VIEW			m_idxView			{};

	vector<UINT>					m_cbvListSize		;
	vector<ID3D12Resource*>			m_textureLst		;

	vector<ConstHeap>				m_objCbv			{ 5,  ConstHeap{} };
	vector<ConstObject>				m_objValue			{ 5,  ConstObject{} };

	double							m_angle				{};

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
