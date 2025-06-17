
#include "D3DApp.h"

int main(int argc, char** argv)
{
	HINSTANCE hInst  = GetModuleHandle(nullptr);
	D3DApp appMain;
	D3DApp* pMain = &appMain;
	pMain->Create(hInst);
	pMain->Run();
	return 0;
}
