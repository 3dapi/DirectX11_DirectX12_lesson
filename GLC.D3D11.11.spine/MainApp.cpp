#include <any>
#include <memory>
#include <vector>
#include <Windows.h>
#include <d3d11.h>
#include <DirectxColors.h>
#include "G2Base.h"
#include "MainApp.h"
#include "G2Util.h"
#include "SceneSpine.h"
#include "GameTimer.h"
GameTimer	m_timer;

using namespace DirectX;

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
