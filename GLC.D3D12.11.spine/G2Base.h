#pragma once
#ifndef _G2Base_H_
#define _G2Base_H_
#include  <any>

enum
{
	FRAME_BUFFER_COUNT = 2,
};

enum ATTRIB_CMD
{
	ATTRIB_NONE  = 0,
	ATTRIB_DEVICE,
	ATTRIB_CONTEXT,
	ATTRIB_SCREEN_SIZE,

	ATTRIB_DEVICE_RENDER_TARGET_FORAT = 101,
	ATTRIB_DEVICE_DEPTH_STENCIL_FORAT,
	ATTRIB_DEVICE_CURRENT_FRAME_INDEX,
	ATTRIB_DEVICE_VIEWPORT,
	ATTRIB_DEVICE_SCISSOR_RECT,
};

typedef struct IG2GraphicsD3D* PENGINED3D;
struct IG2GraphicsD3D
{
	static PENGINED3D getInstance();

	virtual int Create(void* initialist)		= 0;
	virtual int Run()							= 0;
	virtual std::any GetAttrib(int nCmd)		= 0;
	virtual std::any GetDevice()				= 0;
	virtual std::any GetContext()				= 0;
	virtual std::any GetRootSignature()			= 0;
	virtual std::any GetCommandAllocator()		= 0;
	virtual std::any GetCommandQueue()			= 0;
	virtual std::any GetCommandList()			= 0;
	virtual std::any GetRenderTarget()			= 0;
	virtual std::any GetRenderTargetView()		= 0;
	virtual std::any GetDepthStencilView()		= 0;
	virtual int GetCurrentFrameIndex() const	= 0;
	virtual int WaitForGpu()					= 0;
	virtual ~IG2GraphicsD3D() = default;
};


#endif
