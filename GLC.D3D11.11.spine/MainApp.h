#pragma once
#ifndef _MainApp_H_
#define _MainApp_H_

#include <vector>
#include <string>
#include <d3d11.h>
#include <DirectxMath.h>

using namespace std;
using namespace DirectX;

class MainApp
{
protected:
	class SceneSpine*	m_spine	{};
public:
	MainApp();
	virtual ~MainApp();
	virtual int Init();
	virtual int Destroy();
	virtual int Update();
	virtual int Render();

	::SIZE	StringSize(const string& text, HFONT hFont);
	int InitTexture();
};


#endif
