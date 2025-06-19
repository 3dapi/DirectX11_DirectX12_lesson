#pragma once
#ifndef _MainApp_H_
#define _MainApp_H_

#include <vector>
#include <d3d12.h>
#include <DirectxMath.h>

class MainApp
{
protected:
public:
	MainApp();
	virtual ~MainApp();
	virtual int Init();
	virtual int Destroy();
	virtual int Update();
	virtual int Render();
};


#endif
