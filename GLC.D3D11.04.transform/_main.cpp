
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "comctl32")

#include <Windows.h>
#include "G2Base.h"

int main(int argc, char** argv)
{
	auto pMain = IG2GraphicsD3D::getInstance();
	if (FAILED(pMain->Create({})))
		return 0;
	pMain->Run();
	return 0;
}
