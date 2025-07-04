#include <any>
#include <memory>
#include <Windows.h>
#include <d3d12.h>
#include <DirectxColors.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include <wrl.h>
#include <shellapi.h>
#include <WICTextureLoader.h>
#include <ResourceUploadBatch.h>

#include "d3dx12.h"
#include "G2Base.h"
#include "MainApp.h"
#include "G2Util.h"
#include "D3DApp.h"
#include "GameTimer.h"
GameTimer	m_timer;

MainApp::MainApp()
{
}

MainApp::~MainApp()
{
	Destroy();
}

int MainApp::Init()
{
	HRESULT hr = S_OK;
	m_timer.Reset();
	m_spine = new SceneSpine;
	hr = m_spine->Init();
	if(FAILED(hr))
		return hr;
	return S_OK;
}

int MainApp::Destroy()
{
	if(m_spine)
	{
		delete m_spine;
		m_spine = {};
	}
	return S_OK;
}

int MainApp::Update()
{
	m_timer.Tick();
	auto deltaTime = m_timer.DeltaTime();
	auto d3dDevice = std::any_cast<ID3D12Device*>(IG2GraphicsD3D::getInstance()->GetDevice());
	if(m_spine)
		m_spine->Update(deltaTime);

	return S_OK;
}

int MainApp::Render()
{
	HRESULT hr = S_OK;
	if(m_spine)
		m_spine->Render();
	return S_OK;
}

