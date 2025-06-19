#pragma once
#ifndef _G2Base_H_
#define _G2Base_H_
#include  <any>

enum ATTRIB_CMD
{
	ATTRIB_NONE  = 0,
	ATTRIB_DEVICE,
	ATTRIB_CONTEXT,
	ATTRIB_SCREEN_SIZE,
};

typedef struct IG2GraphicsD3D* PENGINED3D;
struct IG2GraphicsD3D
{
	static PENGINED3D getInstance();

	virtual int Create(void* initialist) = 0;
	virtual int Run() = 0;
	virtual std::any GetAttrib(int nCmd) = 0;
	virtual std::any GetDevice() = 0;
	virtual std::any GetContext() = 0;
	virtual ~IG2GraphicsD3D() = default;
};


#endif
