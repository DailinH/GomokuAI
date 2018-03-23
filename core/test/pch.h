//
// pch.h
// Header for standard system include files.
//

#pragma once
#define GTEST_HAS_TR1_TUPLE 0 // It seems that MSVC C++17 does not provide ::std::tr1 namespace.

#include "gtest/gtest.h"
#include <array>
#include <cstdlib>
#include "lib/include/Game.h"

namespace Gomoku {
inline bool operator==(const Board& lhs, const Board& rhs) {
    // Ϊ�ӿ��ٶȣ�ֻ���moveStatesԪ�ظ����Ƿ���ȣ��Ͳ����Ԫ���Ƿ�һһ��Ӧ��
    // ע��m_moveStates��m_moveCounts������std::array������ԭ�����飬��˲���ֱ�������Ƚ�
    auto make_tied = [](const Board& b) {
        return std::tie(b.m_curPlayer, b.m_winner, b.m_moveCounts[0], b.m_moveCounts[1], b.m_moveCounts[2]);
    };
    return make_tied(lhs) == make_tied(rhs);
}
}