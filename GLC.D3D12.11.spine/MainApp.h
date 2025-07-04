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
#include <spine/spine.h>
#include "G2Base.h"
#include "G2Util.h"
#include "SceneSpine.h"

class MainApp
{
protected:
	SceneSpine*		m_spine	{};

public:
	MainApp();
	virtual ~MainApp();

	virtual int Init();
	virtual int Destroy();
	virtual int Update();
	virtual int Render();
};

#endif
