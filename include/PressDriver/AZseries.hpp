#ifndef __AZSERIES_H
#define __AZSERIES_H

#include <vector>
#define _TOTAL_NUM_OF_ELEMENTS_OFtxdata 32

#define BROADCAST	0
#define FLAG_RESET	0
#define ZHOME	0b10000
#define FREE		0b1000000
#define ALARM_RESET	0b10000000

typedef enum CONTROL_MODE {
	ALL_STOP,
	ABSOLUTE_POSITIONING = 1,
	SPEED_CONTROL = 16,
	CONTROL_MODE_NUM,
};

class AZseries {
public:
	AZseries(HANDLE* comPort) {
		_comPort = comPort;
		init();
	}

	void init() {
		txdata.reserve(32);
		txdata.clear();
		rxdata.reserve(9);
	}

	// アドレス(007D)への入力信号の割り付について
	// ・入力信号への割り付(機能編P.174)
	// ・ドライバ入力指令(機能編P.270)
	// ・入力信号一覧(機能編P.160)
	void driverInputCommand(const short& _id, int option) {
		txdata.clear();
		txdata.resize(6);
		txdata[0] = _id;		// ID(スレーブアドレス)
		txdata[1] = 0x06;	// ファンクションコード(特定の保持レジスタへの書き込み)
		txdata[2] = 0x00;	// 書き込まれるレジスタのアドレス(0x007D)
		txdata[3] = 0x7D;	// 書き込まれるレジスタのアドレス(0x007D)
		txdata[4] = (option >> 8) & 0xFF;
		txdata[5] = option & 0xFF;
		int crc = crc16(txdata, txdata.size());
		txdata.emplace_back(crc & 0xFF);
		txdata.emplace_back((crc >> 8) & 0xFF);

		sendPort(txdata.data(), txdata.size());
		Sleep(20);
	}

	// ドライバに書き込んだ指令値を実行 (機能編pp.272-275)
	void rotate(const short& _id, int driveMode, int driveNo) {
		if (driveMode == ABSOLUTE_POSITIONING)	driverInputCommand(_id, 0x0008 + driveNo & 0xFFFF);
		else if (driveMode == SPEED_CONTROL)	driverInputCommand(_id, 0x4000 + driveNo & 0xFFFF);
	}

	//モータ即停止コマンド。例）位置決め運転中、目標位置以外でも停止可能。
	void stop(const short& _id) {
		driverInputCommand(_id, 0x0020);
	}
	

	// 検出位置(現在パルス)を読み込む関数
	int readPulse(const short& _id) {
		txdata.clear();
		txdata.emplace_back(_id);
		txdata.emplace_back(0x03); // ファンクションコード(特定の保持レジスタの読み出し p254)
		txdata.emplace_back(0x00); // 読み出しの起点となるレジスタアドレス（上位の上位）p354参照
		txdata.emplace_back(0xCC); // 読み出しの起点となるレジスタアドレス（上位の下位）速度Hzなら00D0といった具合
		txdata.emplace_back(0x00); // 読み出しレジスタ数（上位）
		txdata.emplace_back(0x02); // 読み出しレジスタ数（下位）
		int crc = crc16(txdata, txdata.size());
		txdata.emplace_back(crc & 0xFF);
		txdata.emplace_back((crc >> 8) & 0xFF);

		bool ret = sendPort(txdata.data(), txdata.size());
		if (ret == 0)// 送信エラー
		{
			std::printf("write error x\n");
			return -1000;
		}
		Sleep(20);

		// バッファから取り込み
		rxdata.clear();
		rxdata.resize(9); // 5 + 4 * 取得データ種類数
		ret = ReadFile(*_comPort, rxdata.data(), rxdata.size(), &numberOfPut, NULL);
		if (ret == 0)// 受信エラー
		{
			std::printf("read error\n");
			return -2000;
		}
		return (rxdata[3] << 24) + (rxdata[4] << 16) + (rxdata[5] << 8) + rxdata[6];
		// レスポンスについて
		// rxdata[0]はID、[1]はファンクションコード、[2]は読み込みバイト数、後ろ二つはCRCである。
	}


	// 直接参照 (機能編P.366-P.370)
	// 連続運転(速度制御)の注意
	// 運転速度を切り替える時は，現在とは異なる運転データNo.で送信すると適用される(機能編P.128)
	// 速度は加速度同じ数値であれば、１秒でその速度になる。単位は[Hz/s]なはず
	void directReference(const short& _id, int step, int speed, int boot_rate, int stop_rate, int mode, int driveNo) {
		txdata.clear();
		txdata.emplace_back(_id);	//ID(スレーブアドレス)
		txdata.emplace_back(0x10);	//ファンクションコード(複数の保持レジスタへの書き込み)
		short baseAddress = 0x1800 + 0x40 * driveNo;
		txdata.emplace_back((baseAddress >> 8) & 0xFF);	//書き込み基点のレジスタアドレス(0x1800)
		txdata.emplace_back(baseAddress & 0xFF);		//書き込み基点のレジスタアドレス(0x1800)
		txdata.emplace_back(0x00);	//レジスタ数(0x000A)
		txdata.emplace_back(0x0A);	//レジスタ数(0x000A)
		txdata.emplace_back(0x14);	//バイト数(レジスタ数*2)

		//方式
		txdata.emplace_back(mode >> 24);
		txdata.emplace_back(mode >> 16 & 0xFF);
		txdata.emplace_back(mode >> 8 & 0xFF);
		txdata.emplace_back(mode & 0xFF);

		// ステップ送信
		txdata.emplace_back(step >> 24);
		txdata.emplace_back(step >> 16 & 0xFF);
		txdata.emplace_back(step >> 8 & 0xFF);
		txdata.emplace_back(step & 0xFF);

		//速度
		speed;
		txdata.emplace_back(speed >> 24);
		txdata.emplace_back(speed >> 16 & 0xFF);
		txdata.emplace_back(speed >> 8 & 0xFF);
		txdata.emplace_back(speed & 0xFF);

		//起動レート
		boot_rate;
		txdata.emplace_back(boot_rate >> 24);
		txdata.emplace_back(boot_rate >> 16 & 0xFF);
		txdata.emplace_back(boot_rate >> 8 & 0xFF);
		txdata.emplace_back(boot_rate & 0xFF);

		//停止レート
		stop_rate;
		txdata.emplace_back(stop_rate >> 24);
		txdata.emplace_back(stop_rate >> 16 & 0xFF);
		txdata.emplace_back(stop_rate >> 8 & 0xFF);
		txdata.emplace_back(stop_rate & 0xFF);
		int crc = crc16(txdata, txdata.size());
		txdata.emplace_back(crc & 0xFF);
		txdata.emplace_back((crc >> 8) & 0xFF);

		sendPort(txdata.data(), txdata.size());
		Sleep(20); //重要(先輩のメモは50だった)
	}

	

	short id;
	// 設定したID（m_id）を使用する場合の上記の関数たち（引数のidを省略して呼び出す）
	int readPulse() {
		return readPulse(id);
	}
	void rotate(int driveMode, int driveNo) {
		rotate(id, driveMode, driveNo);
	}
	void stop() {
		stop(id);
	}
	void driverInputCommand(int option) {
		driverInputCommand(id, option);
	}
	void directReference(int step, int speed, int boot_rate, int stop_rate, int mode, int driveNo) {
		directReference(id, step, speed, boot_rate, stop_rate, mode, driveNo);
	}

private:

	//通信パケットエラーチェック関数
	int crc16(const std::vector<BYTE>& data, const int& len) {
		int crc = 0xFFFF;
		for (int i = 0; i < len; i++) {
			crc = crc ^ data[i];
			for (int j = 0; j < 8; j++) {
				if (crc % 2) crc = (crc >> 1) ^ 0xA001;
				else		 crc >>= 1;
			}
		}
		return crc;
	}


	// ポートへ送信。戻り値は成功・失敗判定
	bool sendPort(BYTE* container_ptr, const int& container_size) {
		PurgeComm(*_comPort, PURGE_RXCLEAR);
		PurgeComm(*_comPort, PURGE_TXCLEAR);
		return WriteFile(*_comPort, container_ptr, container_size, &numberOfPut, NULL);
	}

	HANDLE* _comPort;
	DWORD numberOfPut;
	std::vector<BYTE> txdata,  rxdata;
};


#endif
