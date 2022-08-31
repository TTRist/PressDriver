//***********************************************************************************************************
//*	探したい機器の名前(の一部でも可)を入れるとconst char*で"COMxx"を返す
//*	COM10以降は未確認，仮想COMポートは不明
//*	https://www.daisukekobayashi.com/blog/getting-com-port-number-of-specified-device-in-device-manager/
//*	より引用・ヘッダー化，検索できるよう改良(2019/11/05)
//* マルチバイト文字セット環境で利用すること
//***********************************************************************************************************

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <cstdio>
#include <iostream>
#include <setupapi.h>
#include <winioctl.h> //追加したらビルド通った

#pragma	comment(lib,"setupapi.lib")


typedef std::basic_string<TCHAR> tstring;
namespace fcp
{
	bool ListDevice(const GUID& guid, std::vector<tstring>& devices, std::vector<tstring>& comnum)
	{
		SP_DEVINFO_DATA devInfoData;
		ZeroMemory(&devInfoData, sizeof(devInfoData));
		devInfoData.cbSize = sizeof(devInfoData);
		

		int nDevice = 0;
		std::vector<tstring> tmp;
		std::vector<tstring> tmpcn;
		HDEVINFO hDeviceInfo = SetupDiGetClassDevs(&guid, 0, 0, DIGCF_PRESENT |	DIGCF_DEVICEINTERFACE);

		while (SetupDiEnumDeviceInfo(hDeviceInfo, nDevice++, &devInfoData)) 
		{

			BYTE friendly_name[300];
			BYTE PNBuffer[256];
			SetupDiGetDeviceRegistryProperty(hDeviceInfo, &devInfoData,	SPDRP_FRIENDLYNAME, NULL, friendly_name, sizeof(friendly_name), NULL);

			tmp.push_back(tstring((TCHAR*)friendly_name));

			//////////
			HKEY tmp_key = SetupDiOpenDevRegKey(hDeviceInfo, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
			DWORD tmp_type = 0;
			DWORD tmp_size = sizeof(PNBuffer);
			RegQueryValueEx(tmp_key, TEXT("PortName"), NULL, &tmp_type, PNBuffer, &tmp_size);
			tmpcn.push_back(tstring((TCHAR*)PNBuffer));
			
			//std::cout << "PortName\t" << PNBuffer << std::endl;
			
			///////////////////
		}

		SetupDiDestroyDeviceInfoList(hDeviceInfo);

		devices.swap(tmp);
		comnum.swap(tmpcn);

		return true;
	}
	/*
	const char* FindCOMPort2(char* str, bool flag = false)
	{

		// 初期化
		DWORD i;
		BYTE DNBuffer[512]; //領域確保
		char PNBuffer[512]; //領域確保
		DWORD Length = 0;
		std::string portname;
		std::vector<tstring> tmp;
		//HWND hwndDlg = this->GetSafeHwnd(); //ダイアログのハンドル取得
		SP_DEVINFO_DATA DeviceInfoData = { sizeof(SP_DEVINFO_DATA) }; // １件デバイス情報
		HDEVINFO hDevInfo = 0; // 列挙デバイス情報

		// COMポートのデバイス情報を取得
		hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, NULL, 0, (DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));


		// 列挙の終わりまでループ
		for (i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData); i++) // １件取得
		{
			SetupDiGetDeviceRegistryProperty(hDevInfo, &DeviceInfoData, SPDRP_DEVICEDESC, NULL, DNBuffer, sizeof(DNBuffer), &Length);
			tmp.push_back(tstring((TCHAR*)DNBuffer));

			std::cout << "SPDRP_DEVICEDESC\t" << DNBuffer << std::endl;


			// COMポート番号を取得
			HKEY tmp_key = SetupDiOpenDevRegKey(hDevInfo, &DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
			if (tmp_key) {
				DWORD tmp_type = 0;
				DWORD tmp_size = sizeof(PNBuffer);
				RegQueryValueEx(tmp_key, TEXT("PortName"), NULL, &tmp_type, reinterpret_cast<LPBYTE>(&PNBuffer), &tmp_size);
				std::cout << "PortName\t" << PNBuffer << std::endl;
				portname = PNBuffer;
			}
		}
			//std::cout << "\n\n" << std::endl;

			return portname.c_str();
	}
	*/
	const char* FindCOMPort(const char* str, bool flag = false)	//探したい機器の名前(の一部でも可)を入れると"COMxx"を返す
	{
		static tstring comnum = "Not Found";
		
	
		std::vector<tstring> dev;
		std::vector<tstring> cpn;
		//tstring buf;
		ListDevice(GUID_DEVINTERFACE_COMPORT, dev, cpn);

		std::cout << dev.size() << std::endl;
		std::cout << cpn.size() << std::endl;

		for (int i = 0; i < dev.size(); i++) {
			//std::cout << dev[i].c_str() << std::endl;		//つながっているCOM機器のリスト
			if (dev[i].find(TEXT(str)) != tstring::npos) 
			{
				comnum = "\\\\.\\" + cpn[i];
				/*
				int start = dev[i].find_last_of(TEXT("(COM"));
				int end = dev[i].find_last_of(TEXT(")"));

				//buf = dev[i].substr(start - 2, end - (start - 2));
				//buf.push_back('\0');
				//comnum = buf;
				comnum = dev[i].substr(start - 2, end - (start - 2));
				comnum.push_back('\0');
				//*/
				if (flag)
				{
					std::cout << "デバイス名:" << dev[i].c_str() << std::endl;
					std::cout << "ポート　　:" << comnum << std::endl;
				}
			}
		}

		if (comnum == "Not Found")std::cout << "デバイス未検出" << comnum << std::endl;;

		return comnum.c_str();
	}

	const char* GetDeviceName(char* str, bool flag = false)	//探したい機器のCOMポート番号を入れるとデバイス名を返す
	{
		static std::string comname = "Not Found";


		std::vector<tstring> dev;
		std::vector<tstring> cpn;
		std::vector<tstring> buf(128);
		ListDevice(GUID_DEVINTERFACE_COMPORT, dev, cpn);

		for (int i = 0; i < dev.size(); i++) {
			//std::cout << dev[i].c_str() << std::endl;		//つながっているCOM機器のリスト
			if (dev[i].find(TEXT(str)) != tstring::npos)
			{
				//int start = dev[i].find_last_of(TEXT("(COM"));
				int end = dev[i].find_last_of(TEXT("(COM"));

				buf[i] = dev[i].substr(0, end - 3);
				buf[i].push_back('\0');
				comname = buf[i];
				
				if (flag)
				{
					std::cout << "デバイス名:" << comname << std::endl;
				}
			}
		}
		return comname.c_str();
	}

	std::vector<tstring> DeviceList(bool flag = false)
	{
		static std::vector<tstring> dev;
		std::vector<tstring> cpn;
		ListDevice(GUID_DEVINTERFACE_COMPORT, dev, cpn);

		for (int i = 0; i < dev.size(); i++) 
		{
			if(flag)std::cout << dev[i].c_str() << std::endl;	//つながっているCOM機器のリスト
		}

		return dev;
	}
}