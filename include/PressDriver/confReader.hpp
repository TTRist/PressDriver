//***********************************************************************************************************
//*	2021/04/28 作成者：北島
//* 外部ファイルからキーとデータを読み取る
//* データの書き方は，区切り文字を=とした場合は以下のように記述する
//* KEY=value
//***********************************************************************************************************



#ifndef _CONF_READER_H
#define _CONF_READER_H

#include <iostream>
#include <fstream>
#include <regex>
#include <unordered_map>

namespace conf {
    std::unordered_map<std::string, std::string> config_map; // キーと要素を格納

    // （正規表現の）区切り文字設定．
    // 引数は区切り文字に使いたい文字を何個でも入れてOK．使用不可文字（.|）
    template <class... STR> std::string setSeparetor(STR ...sep) {
        std::string regex_separator = "";
        for (std::string i : std::initializer_list<std::string>{ sep... }) {
            regex_separator += i;
            regex_separator += '|';
        }
        regex_separator.erase(--regex_separator.end());
        return regex_separator;
    }

    // 文字列(string)を区切り文字で分割する
    // 引数(分割したい文字列, 文字列リテラル表記の区切り文字)
	// 例えば=と空白が区切り文字なら R"(=|\s)"
    std::vector<std::string> split(const std::string& str, const std::string& regex_separator) {
        std::vector<std::string> splited_str;
        std::regex rx(regex_separator);
        std::sregex_token_iterator it(str.begin(), str.end(), rx, -1);
        std::sregex_token_iterator end;
        while (it != end) splited_str.emplace_back(*it++);
        return splited_str;
    }

    // 設定ファイルから読み込んだキーと要素を保存
    void setMap(std::unordered_map<std::string, std::string>& conf, std::ifstream& input_file, const std::string& regex_separator) {
        std::string row;
		conf.clear();
        while (std::getline(input_file, row)) {
            std::vector<std::string> splited_str = split(row, regex_separator);
            conf.emplace(splited_str[0], splited_str[1]);
        }
    }

    // 引数にキーを入れるとその値が返ってくる．代入先によって返す型が変わる．
    // 対応型:string,char*,short,int,long,float,double,
    class readMap {
    public:
        readMap(std::string key) {
            if (config_map.find(key) == config_map.end()) _key = "DUMMY"; // キーが存在しないならダミーのキーを生成
            else _key = key;
        }
        template<class T> operator T();

    private:
        std::string _key; // 引数の渡し方が不明だっため，一旦こっちに代入する．
    };

    template<> readMap::operator std::string() {
        return config_map[_key];
    }

	template<> readMap::operator short() {
		int num;
		try { num = stoi(config_map[_key]); }
		catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
			num = 0;
		}
		return static_cast<short>(num);
	}

    template<> readMap::operator int() {
        int num;
        try { num = stoi(config_map[_key]); }
        catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            num = 0;
        }
        return num;
    }

    template<> readMap::operator long() {
        long num;
        try { num = stol(config_map[_key]); }
        catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            num = 0;
        }
        return num;
    }

    template<> readMap::operator float() {
        float num;
        try { num = stof(config_map[_key]); }
        catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            num = 0.f;
        }
        return num;
    }

    template<> readMap::operator double() {
        double num;
        try { num = stod(config_map[_key]); }
        catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            num = 0.f;
        }
        return num;
    }

	template<> readMap::operator const char*() {
		return config_map[_key].c_str();
	}
}
#endif