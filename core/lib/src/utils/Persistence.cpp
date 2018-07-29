#include "Persistence.h"
#include <fstream>
#include <memory>

using namespace std;
using namespace Gomoku;

// ע��data�ļ���һ��Ҫ�ȴ��ڣ�
constexpr auto data_path = L"./data/persistence.json";

struct Handle {
	// ������ڴ�����
	json data;

	static Handle& Inst() {
		static Handle handle;
		return handle;
	}

	Handle() {
		ifstream ifs(data_path);
		if (ifs.is_open()) {
			ifs >> data;
		}
	}

	~Handle() {
		ofstream ofs(data_path);
		if (!ofs.is_open()) {
			throw data_path; // ����filesystem...����botzone�ϴ���ܲ�����
		}
		ofs << data;
	}
};

json Gomoku::Persistence::Load(std::string_view name) {
	if (Handle::Inst().data.count(name.data())) {
		return Handle::Inst().data[name.data()];
	} else {
		return json();
	}
}

void Gomoku::Persistence::Save(std::string_view name, json value) {
	Handle::Inst().data[name.data()] = std::move(value);
}
