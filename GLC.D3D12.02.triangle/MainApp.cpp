#include <any>
#include <Windows.h>
#include <d3d12.h>
#include <DirectxColors.h>
#include "G2Base.h"
#include "MainApp.h"
#include "G2Util.h"
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
	return S_OK;
}

int MainApp::Destroy()
{
	return S_OK;
}

int MainApp::Update()
{
	return S_OK;
}

int MainApp::Render()
{
	return S_OK;
}

