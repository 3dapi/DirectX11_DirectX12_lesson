// Interface for the CD3DApp class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _D3DApp_H_
#define _D3DApp_H_

#include <string>
#include <vector>
#include <Windows.h>
#include <d3d11_1.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;


template<typename T>
inline void SAFE_RELEASE(T*& p)
{
	if (p)
	{
		p->Release();
		p = {};
	}
}
// NOTE: for new T*[count] raw pointer array
template<typename T>
inline void SAFE_RELEASE_ARRAY(T*& pArray, size_t count)
{
	if (pArray)
	{
		for (size_t i = 0; i < count; ++i)
		{
			if (pArray[i])
				pArray[i]->Release();
		}
		delete[] pArray;
		pArray = nullptr;
	}
}
// NOTE: for std::vector 
template<typename T>
inline void SAFE_RELEASE_VECTOR(std::vector<T*>& vec) {
	for (auto& p : vec) {
		if (p) {
			p->Release();
			p = nullptr;
		}
	}
	vec.clear();
}

#define APP_WIN_STYLE (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE)

class D3DApp
{
public:
	static D3DApp*	getInstance();								// sigleton
	int		Create(HINSTANCE hInst);
	int		Run();
	void	Cleanup();
	int		RenderApp();
	static	LRESULT WINAPI WndProc(HWND, UINT, WPARAM, LPARAM);	// window proc function

	// window settings
protected:
	D3DApp();
	static D3DApp*		m_pAppMain;								// sigleton Instance
	const std::string	m_class = "GLC.D3D.Main.Class";
	std::string			m_name;									// windows class name
	HINSTANCE			m_hInst			{};						//
	HWND				m_hWnd			{};
	DWORD				m_dWinStyle		{ APP_WIN_STYLE };
	::SIZE				m_screenSize	{ 1280, 720 };			// HD Screen size width, height
	bool				m_showCusor		{ true };				// Show Cusor
	bool				m_bWindow		{ true };				// Window mode
	LRESULT MsgProc(HWND, UINT, WPARAM, LPARAM);

	// device and context
protected:
	ID3D11Device*			m_d3dDevice				{};
	ID3D11DeviceContext*	m_d3dContext			{};
	IDXGISwapChain*			m_d3dSwapChain			{};
	ID3D11Device1*			m_d3dDevice_1			{};
	ID3D11DeviceContext1*	m_d3dContext_1			{};
	IDXGISwapChain1*		m_d3dSwapChain_1		{};
	ID3D11RenderTargetView*	m_d3dRenderTargetView	{};
	ID3D11Texture2D*		m_d3dDepthStencil		{};
	ID3D11DepthStencilView*	m_d3dDepthStencilView	{};
	D3D_FEATURE_LEVEL		m_featureLevel			{};
	int		InitDevice();
	int		ReleaseDevice();

	// for driven class
public:
	virtual int Init()		{ return S_OK; }
	virtual int Destroy()	{ return S_OK; }
	virtual int Update()	{ return S_OK; }
	virtual int Render()	{ return S_OK; }
};

#endif