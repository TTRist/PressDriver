// -*- C++ -*-
/*!
 * @file  PressDriver.cpp
 * @brief ModuleDescription
 * @date $Date$
 *
 * $Id$
 */

#include "PressDriver.h"
#include "AZseries.hpp"
#include "confReader.hpp"
#include "GetCOMPortNum.hpp"

#include <iostream>
#include <cmath>
#include <conio.h>
#include <time.h>

 // ESCのキーコード
#define ESC 27 //0x1B

using namespace std;
#define RANGE_CHECK(x,min,max) ((x= (x<min  ? min : x<max ? x : max)))

HANDLE comPort;
DCB dcb; // シリアルポートの構成情報が入る構造体
COMMTIMEOUTS timeout; // COMMTIMEOUTS構造体の変数を宣言

// ハンドラ関数
BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
{
	BOOL ret = FALSE;

	// ウィンドウの終了イベント検出時
	// ※「閉じるボタン」でも、「強制終了」でも同じフラグ
	if (dwCtrlType == CTRL_CLOSE_EVENT)
	{
		try { CloseHandle(comPort); }
		catch (int e) {}
		ret = TRUE; // ハンドラ関数の連続呼び出しを終了する
		exit(1);
	}
	return ret;
}

AZseries mot(&comPort);

// 機械的な固定値
const double LEAD = 2.0;
const double STEP_ANGLE = 0.05;
const int PPR = static_cast<int>(360.0 / STEP_ANGLE);
#define mm2step(mm) (mm * PPR / LEAD)
#define step2mm(step) (step * LEAD / PPR)

// その他変数
int speed, accel;
double pos_max, pos_min, push_speed, touch_speed, back_dist;
double dist_tips, d_load;
int time_pressing, time_release;

enum { MODE, POSITION, LOAD, PORT_LENGTH };
enum {
	NOW_POS,
	ABSOLUTE_POS,
	RELATIVE_POS,
	BASE_RLTPOS,
	BASE_SET,
	BLTOUCH_POS,
	LOAD_POS,
	RELEASE_FORCE,
	NOWPOS_BASE,
	RELOAD_SETTING,
	FULL_AUTO,
	TIME_BLT,
	NOWAIT_BLT,
	SAME_POWER,
	DEPTH_LOAD
};

// Module specification
// <rtc-template block="module_spec">
static const char* pressdriver_spec[] =
{
  "implementation_id", "PressDriver",
  "type_name",         "PressDriver",
  "description",       "ModuleDescription",
  "version",           "1.0.0",
  "vendor",            "VenderName",
  "category",          "Category",
  "activity_type",     "PERIODIC",
  "kind",              "DataFlowComponent",
  "max_instance",      "1",
  "language",          "C++",
  "lang_type",         "compile",
  ""
};
// </rtc-template>

/*!
 * @brief constructor
 * @param manager Maneger Object
 */
PressDriver::PressDriver(RTC::Manager* manager)
// <rtc-template block="initializer">
	: RTC::DataFlowComponentBase(manager),
	m_touchIn("touch", m_touch),
	m_loadIn("load", m_load),
	m_emgIn("emg", m_emg),
	m_posloadOut("posload", m_posload),
	m_BLTonOut("BLTon", m_BLTon),
	m_reoffsetOut("reoffset", m_reoffset)

	// </rtc-template>
{
}

/*!
 * @brief destructor
 */
PressDriver::~PressDriver()
{
}

// ポート接続まとめ。用途によって中身変えて
void connectPort(const char* portName) {
	SetConsoleCtrlHandler(HandlerRoutine, TRUE);
	while (true) // シリアル通信接続
	{
		comPort = CreateFile(fcp::FindCOMPort(portName, 1), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL); // シリアルポートを開く
		if (comPort)break;
		else std::printf("\rCONNECTION RETRY");
	}
	GetCommState(comPort, &dcb); // 現在の設定値を読み込み
	dcb.BaudRate = 38400; // 速度
	dcb.ByteSize = 8; // データ長
	dcb.Parity = EVENPARITY; // パリティ
	dcb.StopBits = ONESTOPBIT; // ストップビット長
	dcb.fOutxCtsFlow = FALSE; // 送信時CTSフロー
	dcb.fRtsControl = RTS_CONTROL_ENABLE; // RTSフロー
	SetCommState(comPort, &dcb); // 変更した設定値を書き込み
	Sleep(50);

	//(受信トータルタイムアウト) = ReadTotal_timeoutMultiplier * (受信予定バイト数) + ReadTotal_timeoutConstant
	timeout.ReadIntervalTimeout = 500; // 文字読込時の2も時間の全体待ち時間（msec）
	timeout.ReadTotalTimeoutMultiplier = 0; //読込の1文字あたりの時間
	timeout.ReadTotalTimeoutConstant = 500; //読込エラー検出用のタイムアウト時間
	//(送信トータルタイムアウト) = WriteTotal_timeoutMultiplier * (送信予定バイト数) + WriteTotal_timeoutConstant
	timeout.WriteTotalTimeoutMultiplier = 0; //書き込み１文字あたりの待ち時間
	timeout.WriteTotalTimeoutConstant = 500;//書き込みエラー検出用のタイムアウト時間

	SetCommTimeouts(comPort, &timeout);
}


RTC::ReturnCode_t PressDriver::onInitialize()
{
	cout << "onInitialize [PressDriver]" << endl;
	addInPort("touch", m_touchIn);
	addInPort("load", m_loadIn);
	addInPort("emg", m_emgIn);
	addOutPort("BLTon", m_BLTonOut);
	addOutPort("posload", m_posloadOut);
	addOutPort("reoffset", m_reoffsetOut);

	m_posload.data.length(PORT_LENGTH);
	return RTC::RTC_OK;
}



double base_zero = 0.f;
RTC::ReturnCode_t PressDriver::onActivated(RTC::UniqueId ec_id)
{
	// 設定ファイル読み込み
	std::ifstream infile("./../../../MakedFile/settingOption.ini");
	if (!infile) {
		cout << "読み込み失敗　File notFound\n";
		return RTC::RTC_ERROR;
	}
	conf::setMap(conf::config_map, infile, R"(=| )");
	infile.close();

	// ポート接続
	connectPort(conf::readMap("PORT"));

	mot.id = conf::readMap("ID");
	if (mot.readPulse() == -1000) {
		cout << "connection Error\n";
		return RTC::RTC_ERROR; // 返り値で，接続状況を確認．マイナス値で接続エラー
	}

	mot.init();
	mot.stop();
	mot.driverInputCommand(ALARM_RESET);
	mot.driverInputCommand(FLAG_RESET);

	speed = conf::readMap("SPEED");
	accel = conf::readMap("ACCEL");
	pos_max = conf::readMap("POS_MAX");
	pos_min = conf::readMap("POS_MIN");
	push_speed = conf::readMap("PUSH_SPEED");
	touch_speed = conf::readMap("TOUCH_SPEED");
	back_dist = conf::readMap("BACK_DIST");
	dist_tips = conf::readMap("DIST_TIPS");
	time_pressing = conf::readMap("TIME_PRESSING");
	time_release = conf::readMap("TIME_RELEASE");
	d_load = conf::readMap("D_LOAD");

	base_zero = step2mm(mot.readPulse());

	m_reoffset.data = true;// trueしか送らないのでここで保持させる

	return RTC::RTC_OK;
}


RTC::ReturnCode_t PressDriver::onDeactivated(RTC::UniqueId ec_id)
{
	CloseHandle(comPort);
	return RTC::RTC_OK;
}


RTC::ReturnCode_t PressDriver::onExecute(RTC::UniqueId ec_id)
{
	static bool slcted_mode = false;
	static double pos = step2mm(mot.readPulse());
	static int mode = 404;
	static double orderval = 0.f;

	// モード選択
	if (!slcted_mode) {
		cout << "\n\n\ninput Command..." << endl;
		cout << NOW_POS << ":現在位置取得 [mm]" << endl;
		cout << ABSOLUTE_POS << ":機械絶対位置座標 [mm] (0が離れた位置)" << endl;
		cout << RELATIVE_POS << ":現在相対位置座標 [mm](圧縮する方向が正)" << endl;
		cout << BASE_RLTPOS << ":基準相対位置座標 [mm](圧縮する方向が正)" << endl;
		cout << BASE_SET << ":物体基準点調整 [mm]" << endl;
		cout << BLTOUCH_POS << ":BLTocuh接触モード" << endl;
		cout << LOAD_POS << ":負荷荷重モード [g](入力値:圧迫力ｎ[g])" << endl;
		cout << RELEASE_FORCE << ":圧縮解放モード" << endl;
		cout << NOWPOS_BASE << ":現在地を基準点に変更" << endl;
		cout << RELOAD_SETTING << ":設定ファイル再読み込み" << endl;
		cout << FULL_AUTO << ":フルオート(入力値:最終荷重ｎ[g])" << endl;
		cout << TIME_BLT << ":BLTouch時間毎測定（欠陥）(入力値:ｎ[s]待機)" << endl;
		cout << NOWAIT_BLT << ":BLTouch連続" << endl;
		cout << SAME_POWER << ":同じ力での圧迫・計測を繰り返す(入力値:n[g])" << endl;
		cout << DEPTH_LOAD << ":深さ指定し、動作中の最大圧迫力を測定。(入力値:[mm])" << endl;

		cout << ">> モード選択：";
		cin >> mode;

		if (mode == NOW_POS || mode == BASE_SET || mode == BLTOUCH_POS || mode == RELEASE_FORCE ||
			mode == NOWPOS_BASE || mode == RELOAD_SETTING || mode == NOWAIT_BLT) {
			slcted_mode = true;
		}
		else if (mode == ABSOLUTE_POS || mode == RELATIVE_POS || mode == BASE_RLTPOS || mode == LOAD_POS ||
				 mode == FULL_AUTO || mode == TIME_BLT || mode == SAME_POWER || mode == DEPTH_LOAD) {
			cout << "指令値(位置か重さ)：";
			cin >> orderval;
			slcted_mode = true;
		}
	}
	// 動作時ループ
	else {
		cout << "選択モード：" << mode << endl;
		//位置取得
		if (mode == NOW_POS) {
			pos = step2mm(mot.readPulse());
			cout << "現在位置：" << pos << " [mm]" << endl;
		}

		// 絶対位置制御
		if (mode == ABSOLUTE_POS) {
			RANGE_CHECK(orderval, pos_min, pos_max);
			cout << "orderval = " << orderval << endl;
			mot.directReference(mm2step(orderval), mm2step(speed), mm2step(accel), mm2step(accel), ABSOLUTE_POSITIONING, 0);
			mot.rotate(BROADCAST, ABSOLUTE_POSITIONING, 0);
			mot.driverInputCommand(FLAG_RESET);
			while (abs(mot.readPulse() - mm2step(orderval)) > 3); // 目標位置と10ステップ以内になるまで待機
			{
				if (m_emgIn.isNew()) {
					while (!m_emgIn.isEmpty()) m_emgIn.read();
					if (m_emg.data) mot.stop();

					slcted_mode = false;
				}
			}
			m_posload.data[MODE] = ABSOLUTE_POS;
			m_posload.data[POSITION] = step2mm(mot.readPulse());
			m_posload.data[LOAD] = -404;
			m_posloadOut.write();
		}

		// 相対位置制御
		if (mode == RELATIVE_POS) {
			orderval = step2mm(mot.readPulse()) + orderval;
			RANGE_CHECK(orderval, pos_min, pos_max);
			mot.directReference(mm2step(orderval), mm2step(speed), mm2step(accel), mm2step(accel), ABSOLUTE_POSITIONING, 0);
			mot.rotate(BROADCAST, ABSOLUTE_POSITIONING, 0);
			mot.driverInputCommand(FLAG_RESET);
			while (abs(mot.readPulse() - mm2step(orderval)) > 3); // 目標位置と10ステップ以内になるまで待機
			{
				if (m_emgIn.isNew()) {
					while (!m_emgIn.isEmpty()) m_emgIn.read();
					if (m_emg.data) mot.stop();
					slcted_mode = false;
				}
			}
			m_posload.data[MODE] = RELATIVE_POS;
			m_posload.data[POSITION] = step2mm(mot.readPulse());
			m_posload.data[LOAD] = -404;
			m_posloadOut.write();
		}

		// 基準点相対制御
		if (mode == BASE_RLTPOS) {
			orderval = base_zero + orderval;
			RANGE_CHECK(orderval, pos_min, pos_max);
			mot.directReference(mm2step(orderval), mm2step(speed), mm2step(accel), mm2step(accel), ABSOLUTE_POSITIONING, 0);
			mot.rotate(BROADCAST, ABSOLUTE_POSITIONING, 0);
			mot.driverInputCommand(FLAG_RESET);
			while (abs(mot.readPulse() - mm2step(orderval)) > 3) // 目標位置と10ステップ以内になるまで待機
			{
				if (m_emgIn.isNew()) {
					while (!m_emgIn.isEmpty()) m_emgIn.read();
					if (m_emg.data) mot.stop();
					slcted_mode = false;
				}
			}
			m_posload.data[MODE] = BASE_RLTPOS;
			m_posload.data[POSITION] = step2mm(mot.readPulse());
			m_posload.data[LOAD] = -404;
			m_posloadOut.write();
		}

		// 基準点調整
		if (mode == BASE_SET) {
			// BLTouch起動
			m_BLTon.data = true;
			m_BLTonOut.write();
			Sleep(1500);
			// モータ移動
			mot.directReference(0, mm2step(touch_speed), mm2step(touch_speed) * 100000, mm2step(touch_speed) * 100000000, SPEED_CONTROL, 0);
			mot.rotate(SPEED_CONTROL, 0);
			// BLTouch接触待機
			m_touch.data = false;
			while (!m_touch.data) {
				if (m_touchIn.isNew())m_touchIn.read();
				if (m_emgIn.isNew()) {
					while (!m_emgIn.isEmpty()) m_emgIn.read();
					if (m_emg.data) mot.stop();
					slcted_mode = false;
				}
			}
			mot.driverInputCommand(FLAG_RESET);
			mot.stop();
			Sleep(1);
			base_zero = step2mm(mot.readPulse());
			m_posload.data[MODE] = BASE_SET;
			m_posload.data[POSITION] = base_zero;
			m_posload.data[LOAD] = -404;
			m_posloadOut.write();
		}

		// BLTouch接触モード
		if (mode == BLTOUCH_POS) {
			// BLTouch起動
			m_BLTon.data = true;
			m_BLTonOut.write();
			Sleep(1500);
			double tmp = base_zero - 1;
			RANGE_CHECK(tmp, pos_min, pos_max);
			mot.directReference(mm2step(tmp), mm2step(speed), mm2step(accel), mm2step(accel), ABSOLUTE_POSITIONING, 0);
			mot.rotate(BROADCAST, ABSOLUTE_POSITIONING, 0);
			mot.driverInputCommand(FLAG_RESET);
			while (fabs(step2mm(mot.readPulse()) - tmp) > 0.05f) {
				if (m_emgIn.isNew()) {
					while (!m_emgIn.isEmpty()) m_emgIn.read();
					if (m_emg.data) mot.stop();
					slcted_mode = false;
				}
			}
			Sleep(500);
			// モータ移動
			mot.directReference(0, mm2step(touch_speed), mm2step(touch_speed) * 100000, mm2step(touch_speed) * 100000000, SPEED_CONTROL, 0);
			mot.rotate(SPEED_CONTROL, 0);
			// BLTouch接触待機
			m_touch.data = false;
			while (!m_touch.data) {
				if (m_touchIn.isNew())m_touchIn.read();
				if (m_emgIn.isNew()) {
					while (!m_emgIn.isEmpty()) m_emgIn.read();
					if (m_emg.data) mot.stop();
					slcted_mode = false;
				}
			}
			mot.driverInputCommand(FLAG_RESET);
			mot.stop();
			Sleep(1);
			m_posload.data[MODE] = BLTOUCH_POS;
			m_posload.data[POSITION] = step2mm(mot.readPulse());
			m_posload.data[LOAD] = -404;
			m_posloadOut.write();
		}

		// 荷重制御
		if (mode == LOAD_POS) {
			while (!m_loadIn.isEmpty()) m_loadIn.read();
			double g = round(m_load.data); // 1g単位にする
			if (g < orderval) {
				double tmp = base_zero - 1;
				RANGE_CHECK(tmp, pos_min, pos_max);
				mot.directReference(mm2step(tmp), mm2step(speed), mm2step(accel), mm2step(accel), ABSOLUTE_POSITIONING, 0);
				mot.rotate(BROADCAST, ABSOLUTE_POSITIONING, 0);
				mot.driverInputCommand(FLAG_RESET);
				while (fabs(step2mm(mot.readPulse()) - tmp) > 0.05f) {
					if (m_emgIn.isNew()) {
						while (!m_emgIn.isEmpty()) m_emgIn.read();
						if (m_emg.data) mot.stop();
						slcted_mode = false;
					}
				}
				Sleep(500);
				mot.directReference(0, mm2step(push_speed), mm2step(push_speed) * 100000, mm2step(push_speed) * 100000000, SPEED_CONTROL, 0);
				mot.rotate(SPEED_CONTROL, 0);
			}
			while (g < orderval) // 荷重待機
			{
				while (!m_loadIn.isEmpty()) m_loadIn.read();
				g = m_load.data;
				std::printf("\rLOAD:%.1f	", g);
				if (m_emgIn.isNew()) {
					while (!m_emgIn.isEmpty()) m_emgIn.read();
					if (m_emg.data) mot.stop();
					slcted_mode = false;
				}
			}
			mot.driverInputCommand(FLAG_RESET);
			mot.stop();
			Sleep(1);
			while (!m_loadIn.isEmpty()) m_loadIn.read();
			m_posload.data[MODE] = LOAD_POS;
			m_posload.data[POSITION] = step2mm(mot.readPulse()) - dist_tips;
			m_posload.data[LOAD] = round(g);
			m_posloadOut.write();
			std::printf("\nPOS:%.1f		LOAD:%.1f\n", step2mm(mot.readPulse()) - dist_tips, g);
		}

		// 解放モード
		if (mode == RELEASE_FORCE) {
			orderval = step2mm(mot.readPulse()) - back_dist;
			RANGE_CHECK(orderval, pos_min, pos_max);
			mot.directReference(mm2step(orderval), mm2step(speed), mm2step(accel), mm2step(accel), ABSOLUTE_POSITIONING, 0);
			mot.rotate(BROADCAST, ABSOLUTE_POSITIONING, 0);
			mot.driverInputCommand(FLAG_RESET);
			while (abs(mot.readPulse() - mm2step(orderval)) > 3); // 目標位置と10ステップ以内になるまで待機
			{if (m_emgIn.isNew()) {
				while (!m_emgIn.isEmpty()) m_emgIn.read();
				if (m_emg.data) mot.stop();
				slcted_mode = false;
			}}
		}

		// 現在地を基準点にする
		if (mode == NOWPOS_BASE) {
			base_zero = step2mm(mot.readPulse());
			cout << "基準位置 " << base_zero << endl;
		}

		// 設定ファイル再読み込み
		if (mode == RELOAD_SETTING) {
			// 設定ファイル読み込み
			std::ifstream infile("./../../../MakedFile/settingOption.ini");
			if (!infile) {
				cout << "読み込み失敗　File notFound\n";
				return RTC::RTC_ERROR;
			}
			conf::setMap(conf::config_map, infile, R"(=| )");
			infile.close();

			speed = conf::readMap("SPEED");
			accel = conf::readMap("ACCEL");
			pos_max = conf::readMap("POS_MAX");
			pos_min = conf::readMap("POS_MIN");
			push_speed = conf::readMap("PUSH_SPEED");
			touch_speed = conf::readMap("TOUCH_SPEED");
			back_dist = conf::readMap("BACK_DIST");
		}

		// 放置しても動いてくれるやつ.
		// 指令値としては重さｇを指定
		// このモードは基準位置を設定（モード4）したのちに使用することを想定している
		// 潰しー＞解放ー＞深さ測定ー＞潰しー＞・・・って感じで、最後は深さ測定で追わr
		if (mode == FULL_AUTO) {

			// ここから潰しと深さ測定のループ
			bool a = false; // T:BLTouch, F:Press
			int middle_g = 5;
			double middle_tmp = base_zero;
			m_loadIn.read();

			while (orderval > round(m_load.data)) {
				// 潰し
				if (!a) {
					// 基準点まで移動
					while (!m_loadIn.isEmpty()) m_loadIn.read();
					double g = round(m_load.data); // 1g単位にする
					if (g < orderval) {
						double tmp = middle_tmp;
						RANGE_CHECK(tmp, pos_min, pos_max);
						mot.directReference(mm2step(tmp), mm2step(speed), mm2step(accel), mm2step(accel), ABSOLUTE_POSITIONING, 0);
						mot.rotate(BROADCAST, ABSOLUTE_POSITIONING, 0);
						mot.driverInputCommand(FLAG_RESET);
						while (fabs(step2mm(mot.readPulse()) - tmp) > 0.05f) {
							if (m_emgIn.isNew()) {
								while (!m_emgIn.isEmpty()) m_emgIn.read();
								if (m_emg.data) mot.stop();
								slcted_mode = false;
							}
						}

						Sleep(50);
						// 速度制御
						mot.directReference(0, mm2step(push_speed), mm2step(push_speed) * 100000, mm2step(push_speed) * 100000000, SPEED_CONTROL, 0);
						mot.rotate(SPEED_CONTROL, 0);
						
						// 潰し監視
						while (middle_g > round(m_load.data))
							while (!m_loadIn.isEmpty()) m_loadIn.read();

						mot.stop();
						a = true;
						mot.driverInputCommand(FLAG_RESET);
						m_posload.data[MODE] = LOAD_POS;
						m_posload.data[POSITION] = step2mm(mot.readPulse()) - dist_tips;
						m_posload.data[LOAD] = round(m_load.data);
						m_posloadOut.write();
						std::printf("\nPOS:%.1f		LOAD:%.1f\n", step2mm(mot.readPulse()) - dist_tips, m_load.data);
						Sleep(time_pressing * 1000);
					}
					else break;
				}

				// 距離
				if (a) {
					
					double tmp = base_zero - 1;
					RANGE_CHECK(tmp, pos_min, pos_max);
					mot.directReference(mm2step(tmp), mm2step(speed), mm2step(accel), mm2step(accel), ABSOLUTE_POSITIONING, 0);
					mot.rotate(BROADCAST, ABSOLUTE_POSITIONING, 0);
					mot.driverInputCommand(FLAG_RESET);
					while (fabs(step2mm(mot.readPulse()) - tmp) > 0.05f) {
						if (m_emgIn.isNew()) {
							while (!m_emgIn.isEmpty()) m_emgIn.read();
							if (m_emg.data) mot.stop();
							slcted_mode = false;
						}
					}
					// BLTouch起動
					m_BLTon.data = true;
					m_BLTonOut.write();
					m_reoffsetOut.write();
					Sleep(time_release * 1000);
					// モータ移動
					mot.directReference(0, mm2step(touch_speed), mm2step(touch_speed) * 100000, mm2step(touch_speed) * 100000000, SPEED_CONTROL, 0);
					mot.rotate(SPEED_CONTROL, 0);
					// BLTouch接触待機
					m_touch.data = false;
					while (!m_touch.data) {
						if (m_touchIn.isNew())m_touchIn.read();
						if (m_emgIn.isNew()) {
							while (!m_emgIn.isEmpty()) m_emgIn.read();
							if (m_emg.data) mot.stop();
							slcted_mode = false;
						}
					}
					mot.driverInputCommand(FLAG_RESET);
					mot.stop();
					Sleep(1);
					m_posload.data[MODE] = BLTOUCH_POS;
					m_posload.data[POSITION] = step2mm(mot.readPulse());
					m_posload.data[LOAD] = -404;
					m_posloadOut.write();
					cout << "Touch:" << step2mm(mot.readPulse()) << endl;
					a = false;
					if (orderval - middle_g) middle_g += d_load;
					else middle_g = orderval;
					middle_tmp = step2mm(mot.readPulse());
				}
			}
			slcted_mode = false;
		}

		// BLTouchで指定した時間[s]毎に深さを測定するやつ
		if (mode == TIME_BLT) {
			while (!kbhit() || (getch() != ESC)) {
				double tmp = base_zero - 1;
				RANGE_CHECK(tmp, pos_min, pos_max);
				mot.directReference(mm2step(tmp), mm2step(speed), mm2step(accel), mm2step(accel), ABSOLUTE_POSITIONING, 0);
				mot.rotate(BROADCAST, ABSOLUTE_POSITIONING, 0);
				mot.driverInputCommand(FLAG_RESET);
				while (fabs(step2mm(mot.readPulse()) - tmp) > 0.05f) {
					if (m_emgIn.isNew()) {
						while (!m_emgIn.isEmpty()) m_emgIn.read();
						if (m_emg.data) mot.stop();
						slcted_mode = false;
					}
				}



				m_BLTon.data = true;
				m_BLTonOut.write();
				Sleep(orderval * 1000);
				// モータ移動
				mot.directReference(0, mm2step(touch_speed), mm2step(touch_speed) * 100000, mm2step(touch_speed) * 100000000, SPEED_CONTROL, 0);
				mot.rotate(SPEED_CONTROL, 0);
				// BLTouch接触待機
				m_touch.data = false;
				while (!m_touch.data) {
					if (m_touchIn.isNew())m_touchIn.read();
					if (m_emgIn.isNew()) {
						while (!m_emgIn.isEmpty()) m_emgIn.read();
						if (m_emg.data) mot.stop();
						slcted_mode = false;
					}
				}
				mot.driverInputCommand(FLAG_RESET);
				mot.stop();
				Sleep(1);
				m_posload.data[MODE] = BLTOUCH_POS;
				m_posload.data[POSITION] = step2mm(mot.readPulse());
				m_posload.data[LOAD] = -404;
				m_posloadOut.write();
			}
		}

		if (mode == NOWAIT_BLT) {
			double start_t = clock();
			// 基準位置のちょい前まで移動
			double back = base_zero - 1;
			RANGE_CHECK(back, pos_min, pos_max);
			mot.directReference(mm2step(back), mm2step(speed), mm2step(accel), mm2step(accel), ABSOLUTE_POSITIONING, 0);
			mot.rotate(BROADCAST, ABSOLUTE_POSITIONING, 0);
			mot.driverInputCommand(FLAG_RESET);
			while (fabs(step2mm(mot.readPulse()) - back) > 0.05f) {
				if (m_emgIn.isNew()) {
					while (!m_emgIn.isEmpty()) m_emgIn.read();
					if (m_emg.data) mot.stop();
					slcted_mode = false;
				}
			}
			Sleep(100);
			// ここからループ
			double tgt_blt_pos = base_zero;
			while (!kbhit() || (getch() != ESC)) 
			{
				
				double tmp = tgt_blt_pos - 5;
				RANGE_CHECK(tmp, pos_min, pos_max);
				mot.directReference(mm2step(tmp), mm2step(speed), mm2step(accel), mm2step(accel), ABSOLUTE_POSITIONING, 0);
				mot.rotate(BROADCAST, ABSOLUTE_POSITIONING, 0);
				mot.driverInputCommand(FLAG_RESET);
				while (fabs(step2mm(mot.readPulse()) - tmp) > 0.05f) {
					if (m_emgIn.isNew()) {
						while (!m_emgIn.isEmpty()) m_emgIn.read();
						if (m_emg.data) mot.stop();
						slcted_mode = false;
					}
				}
				m_BLTon.data = true;
				m_BLTonOut.write();
				Sleep(1500);
				// モータ移動
				mot.directReference(0, mm2step(touch_speed), mm2step(touch_speed) * 100000, mm2step(touch_speed) * 100000000, SPEED_CONTROL, 0);
				mot.rotate(SPEED_CONTROL, 0);
				// BLTouch接触待機
				m_touch.data = false;
				while (!m_touch.data) {
					if (m_touchIn.isNew())m_touchIn.read();
					if (m_emgIn.isNew()) {
						while (!m_emgIn.isEmpty()) m_emgIn.read();
						if (m_emg.data) mot.stop();
						slcted_mode = false;
					}
				}
				mot.driverInputCommand(FLAG_RESET);
				mot.stop();
				double passed_t = (clock() - start_t) / 1000;
				Sleep(1);
				tgt_blt_pos = step2mm(mot.readPulse());
				m_posload.data[MODE] = NOWAIT_BLT;
				m_posload.data[POSITION] = tgt_blt_pos;
				m_posload.data[LOAD] = passed_t;
				m_posloadOut.write();
				Sleep(100);
			}
		}

		if (mode == SAME_POWER) {
			// 開始時はロードセルのオフセットをしておく
			m_reoffsetOut.write();
			Sleep(5000);

			while (!kbhit() || (getch() != ESC)) {
				cout << "圧迫開始" << endl;
				// LOAD_POSからのコピペ
				while (!m_loadIn.isEmpty()) m_loadIn.read();
				double g = round(m_load.data); // 1g単位にする
				if (g < orderval) {
					double tmp = base_zero - 1;
					RANGE_CHECK(tmp, pos_min, pos_max);
					mot.directReference(mm2step(tmp), mm2step(speed), mm2step(accel), mm2step(accel), ABSOLUTE_POSITIONING, 0);
					mot.rotate(BROADCAST, ABSOLUTE_POSITIONING, 0);
					mot.driverInputCommand(FLAG_RESET);
					while (fabs(step2mm(mot.readPulse()) - tmp) > 0.05f) {
						if (m_emgIn.isNew()) {
							while (!m_emgIn.isEmpty()) m_emgIn.read();
							if (m_emg.data) mot.stop();
							slcted_mode = false;
						}
					}
					Sleep(500);
					mot.directReference(0, mm2step(push_speed), mm2step(push_speed) * 100000, mm2step(push_speed) * 100000000, SPEED_CONTROL, 0);
					mot.rotate(SPEED_CONTROL, 0);
				}
				while (g < orderval) // 荷重待機
				{
					while (!m_loadIn.isEmpty()) m_loadIn.read();
					g = m_load.data;
					std::printf("\rLOAD:%.1f	", g);
					if (m_emgIn.isNew()) {
						while (!m_emgIn.isEmpty()) m_emgIn.read();
						if (m_emg.data) mot.stop();
						slcted_mode = false;
					}
				}
				mot.driverInputCommand(FLAG_RESET);
				mot.stop();
				Sleep(1);
				while (!m_loadIn.isEmpty()) m_loadIn.read();
				m_posload.data[MODE] = LOAD_POS;
				m_posload.data[POSITION] = step2mm(mot.readPulse()) - dist_tips;
				m_posload.data[LOAD] = round(g);
				m_posloadOut.write();
				std::printf("\nPOS:%.1f		LOAD:%.1f\n", step2mm(mot.readPulse()) - dist_tips, g);
				cout << "圧迫状態維持中" << endl;
				Sleep(time_pressing * 1000);// 潰し維持

				// 膨らみ計測
				cout << "膨らみ待機" << endl;
				double tmp = base_zero - 2;
				RANGE_CHECK(tmp, pos_min, pos_max);
				mot.directReference(mm2step(tmp), mm2step(speed), mm2step(accel), mm2step(accel), ABSOLUTE_POSITIONING, 0);
				mot.rotate(BROADCAST, ABSOLUTE_POSITIONING, 0);
				mot.driverInputCommand(FLAG_RESET);
				while (fabs(step2mm(mot.readPulse()) - tmp) > 0.05f) {
					if (m_emgIn.isNew()) {
						while (!m_emgIn.isEmpty()) m_emgIn.read();
						if (m_emg.data) mot.stop();
						slcted_mode = false;
					}
				}
				// BLTouch起動
				m_BLTon.data = true;
				m_BLTonOut.write();
				m_reoffsetOut.write();
				Sleep(time_release * 1000);
				cout << "膨らみ計測開始" << endl;
				// モータ移動
				mot.directReference(0, mm2step(touch_speed), mm2step(touch_speed) * 100000, mm2step(touch_speed) * 100000000, SPEED_CONTROL, 0);
				mot.rotate(SPEED_CONTROL, 0);
				// BLTouch接触待機
				m_touch.data = false;
				while (!m_touch.data) {
					if (m_touchIn.isNew())m_touchIn.read();
					if (m_emgIn.isNew()) {
						while (!m_emgIn.isEmpty()) m_emgIn.read();
						if (m_emg.data) mot.stop();
						slcted_mode = false;
					}
				}
				mot.driverInputCommand(FLAG_RESET);
				mot.stop();
				Sleep(1);
				m_posload.data[MODE] = BLTOUCH_POS;
				m_posload.data[POSITION] = step2mm(mot.readPulse());
				m_posload.data[LOAD] = -404;
				m_posloadOut.write();
				cout << "Touch:" << step2mm(mot.readPulse()) << endl;
				Sleep(100);
			}
		}

		if (mode == DEPTH_LOAD) {
			orderval = base_zero + orderval;
			RANGE_CHECK(orderval, pos_min, pos_max);
			mot.directReference(mm2step(orderval), mm2step(speed), mm2step(accel), mm2step(accel), ABSOLUTE_POSITIONING, 0);
			mot.rotate(BROADCAST, ABSOLUTE_POSITIONING, 0);
			mot.driverInputCommand(FLAG_RESET);
			double g = 0.f;
			while (abs(mot.readPulse() - mm2step(orderval)) > 3); // 目標位置と10ステップ以内になるまで待機
			{
				if (m_emgIn.isNew()) {
					while (!m_emgIn.isEmpty()) m_emgIn.read();
					if (m_emg.data) mot.stop();
					slcted_mode = false;
				}

				if (m_loadIn.isNew()) {// 動作中の負荷計測
					while (!m_loadIn.isEmpty()) m_loadIn.read();
					if (m_load.data > g)
						g = m_load.data;
					std::printf("\r最大負荷 : %.1f	", g);
				}
			}
			// ｎ秒そのままでの負荷計測
			int start_t = clock();
			while (clock() - start_t < time_pressing * 1000) {
				while (!m_loadIn.isEmpty()) m_loadIn.read();
				if (m_load.data > g)
					g = m_load.data;
				std::printf("\r最大負荷 : %.1f	", g);
			}
			m_posload.data[MODE] = LOAD_POS;
			m_posload.data[POSITION] = step2mm(mot.readPulse());
			m_posload.data[LOAD] = g;
			m_posloadOut.write();

			cout << "\n解放" << endl;
			orderval = base_zero - 5;
			RANGE_CHECK(orderval, pos_min, pos_max);
			mot.directReference(mm2step(orderval), mm2step(speed), mm2step(accel), mm2step(accel), ABSOLUTE_POSITIONING, 0);
			mot.rotate(BROADCAST, ABSOLUTE_POSITIONING, 0);
			mot.driverInputCommand(FLAG_RESET);
			while (abs(mot.readPulse() - mm2step(orderval)) > 3); // 目標位置と10ステップ以内になるまで待機
			{
				if (m_emgIn.isNew()) {
					while (!m_emgIn.isEmpty()) m_emgIn.read();
					if (m_emg.data) mot.stop();
					slcted_mode = false;
				}
			}

			// 膨らみ計測
			cout << "膨らみ待機" << endl;
			double tmp = base_zero - 2;
			RANGE_CHECK(tmp, pos_min, pos_max);
			mot.directReference(mm2step(tmp), mm2step(speed), mm2step(accel), mm2step(accel), ABSOLUTE_POSITIONING, 0);
			mot.rotate(BROADCAST, ABSOLUTE_POSITIONING, 0);
			mot.driverInputCommand(FLAG_RESET);
			while (fabs(step2mm(mot.readPulse()) - tmp) > 0.05f) {
				if (m_emgIn.isNew()) {
					while (!m_emgIn.isEmpty()) m_emgIn.read();
					if (m_emg.data) mot.stop();
					slcted_mode = false;
				}
			}
			// BLTouch起動
			m_BLTon.data = true;
			m_BLTonOut.write();
			m_reoffsetOut.write();
			Sleep(time_release * 1000);
			cout << "膨らみ計測開始" << endl;
			// モータ移動
			mot.directReference(0, mm2step(touch_speed), mm2step(touch_speed) * 100000, mm2step(touch_speed) * 100000000, SPEED_CONTROL, 0);
			mot.rotate(SPEED_CONTROL, 0);
			// BLTouch接触待機
			m_touch.data = false;
			while (!m_touch.data) {
				if (m_touchIn.isNew())m_touchIn.read();
				if (m_emgIn.isNew()) {
					while (!m_emgIn.isEmpty()) m_emgIn.read();
					if (m_emg.data) mot.stop();
					slcted_mode = false;
				}
			}
			mot.driverInputCommand(FLAG_RESET);
			mot.stop();
			Sleep(1);
			m_posload.data[MODE] = BLTOUCH_POS;
			m_posload.data[POSITION] = step2mm(mot.readPulse());
			m_posload.data[LOAD] = -404;
			m_posloadOut.write();
			cout << "Touch:" << step2mm(mot.readPulse()) << endl;
			Sleep(100);

		}


		slcted_mode = false;
	}
	return RTC::RTC_OK;
}




extern "C"
{

	void PressDriverInit(RTC::Manager* manager)
	{
		coil::Properties profile(pressdriver_spec);
		manager->registerFactory(profile,
			RTC::Create<PressDriver>,
			RTC::Delete<PressDriver>);
	}

};


