// Interface for the CD3DApp class.
//
//////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _D3DApp_H_
#define _D3DApp_H_

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


#define APP_WIN_STYLE (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE)

class D3DApp : public IG2GraphicsD3D
{
public:
	int			Create(void* initialist)		override;
	int			Run()							override;
	std::any	GetAttrib(int nCmd)				override;
	std::any	GetDevice()						override;
	std::any	GetContext()					override;
	std::any	GetRootSignature()				override;
	std::any	GetCommandList()				override;
	int			GetCurrentFrameIndex() const	override;

	static D3DApp*	getInstance();								// sigleton
	void			Cleanup();
	int				RenderApp();
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
	::SIZE				m_screenSize	{ 1280, 640 };			// HD Screen size width, height
	bool				m_showCusor		{ true };				// Show Cusor
	bool				m_bFullScreen	{ false };				// Full Screen mode
	LRESULT MsgProc(HWND, UINT, WPARAM, LPARAM);
	void				ToggleFullScreen();						// toggle full screen mode

	// device and context
protected:
	bool								m_useHardware	{true};
	D3D_FEATURE_LEVEL                   m_featureLevel	{};

	// Pipeline objects.
	ID3D12Device*						m_d3dDevice				{};
	ComPtr<IDXGISwapChain3>				m_d3dSwapChain			{};
	ComPtr<ID3D12CommandQueue>			m_d3dCommandQueue		{};
	UINT								m_d3dCurrentFrameIndex	{};
	UINT								m_d3dDescriptorSize		{};
	ComPtr<ID3D12DescriptorHeap>		m_d3dDescTarget			{};
	ComPtr<ID3D12Resource>				m_d3dRenderTarget		[FRAME_BUFFER_COUNT];
	ComPtr<ID3D12DescriptorHeap>		m_d3dDescDepth			{};
	ComPtr<ID3D12Resource>				m_d3dDepthStencil		{};
	ComPtr<ID3D12RootSignature>			m_d3dRootSignature		{};
	ComPtr<ID3D12PipelineState>			m_d3dPipelineState		{};
	ComPtr<ID3D12CommandAllocator>		m_d3dCommandAllocator	{};
	ComPtr<ID3D12GraphicsCommandList>	m_d3dCommandList		{};

	DXGI_FORMAT							m_formatBackBuffer		{DXGI_FORMAT_R8G8B8A8_UNORM};
	DXGI_FORMAT							m_formatDepthBuffer		{DXGI_FORMAT_D32_FLOAT};

	D3D12_VIEWPORT						m_d3dViewport			{};
	D3D12_RECT							m_d3dScissorRect		{};

	// Synchronization objects.
	HANDLE								m_fenceEvent	{};
	ComPtr<ID3D12Fence>					m_fence			{};
	UINT64								m_fenceValue	{};
	int		InitDevice();
	int		ReleaseDevice();
	IDXGIAdapter* GetHardwareAdapter(IDXGIFactory1* pFactory, bool requestHighPerformanceAdapter = false);
	HRESULT PopulateCommandList();
	HRESULT	WaitForPreviousFrame();
	// for driven class
public:
	HWND GetHwnd() const	{ return m_hWnd; }
	virtual int Init()		;
	virtual int Destroy()	;
	virtual int Update()	;
	virtual int Render()	;

	class MainApp* m_pmain{};
};

#endif
