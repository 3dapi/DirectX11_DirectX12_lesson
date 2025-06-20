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

class MainApp
{
protected:
	ComPtr<ID3D12PipelineState>				m_pipelineState;
	ComPtr<ID3D12Resource>                  m_rscVtx		{};
	D3D12_VERTEX_BUFFER_VIEW                m_vtxView		{};

public:
	MainApp();
	virtual ~MainApp();

	virtual int Init();
	virtual int Destroy();
	virtual int Update();
	virtual int Render();
};


#endif
