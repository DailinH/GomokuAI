#include "pch.h"
#include "../../Core/src/Game.hpp"

using namespace Gomoku;

// Problem 1 found: x����һ�������Ǹ���
// Problem 2 found: ��ʽֻ����ͬ��ͬ���²���������
class PositionTest : public ::testing::Test {
protected:
    static const int caseSize = 10;

    virtual void SetUp() override {
        for (int i = 0; i < caseSize; ++i) {
            int sgn = randSgn(); // ÿ��ѭ����֤����һ��
            xCoords[i] = sgn * (rand() % width);
            yCoords[i] = sgn * (rand() % height);
            ids[i] = sgn * (rand() % (width * height));
        }
    }

    int randSgn() {
        return 2 * (rand() % 2) - 1;
    }
    
    int xCoords[caseSize];
    int yCoords[caseSize];
    int ids[caseSize];
};

TEST_F(PositionTest, CtorSymmetry) {
    for (auto id : ids) {
        Position pos(id);
        ASSERT_EQ(Position(pos.x(), pos.y()), pos);
    }
}

TEST_F(PositionTest, CheckFormula) {
    // �����˸������������(a/b)*b + a%b = a�Ĺ���
    // ����x��y�Ǹ�������ʽҲ��ȻӦ��������
    for (auto id : ids) {
        Position pos(id);
        int offset = rand() % 500;
        ASSERT_EQ(Position(pos.x(), pos.y() + offset).id, pos.id + width * offset);
        ASSERT_EQ(Position(pos.x() + offset, pos.y()).id, pos.id + offset);
    }
}

TEST_F(PositionTest, CheckXY) {
    for (int i = 0; i < caseSize; ++i) {
        Position pos(xCoords[i], yCoords[i]);
        ASSERT_EQ(pos.x(), xCoords[i]);
        ASSERT_EQ(pos.y(), yCoords[i]);
    }
}