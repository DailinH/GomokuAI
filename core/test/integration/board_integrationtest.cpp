#include "pch.h"
#include "lib/include/Game.h"
#include <algorithm>
#include <numeric>

using namespace Gomoku;
using std::begin;
using std::end;

// Problem found 1: getNextMove() ����ѭ��
// Problem found 2: isCuurentPlayer������������ֱ����ΪPosition����ΪҪ��Խ����
// Problem found 3: std::array::empty()���Ƿ���false ������Ϊ�����ж�ʹ��
// Problem found 4: checkVictory::search�а������������϶�Ϊһ���߼����������
class BoardTest : public ::testing::Test {
protected:
    static const int caseSize = 10;

    // ����һ�в��ظ���Position
    void randomlyFill(Position* first, Position* last) {
        std::iota(first, last, rand() % (GameConfig::BOARD_SIZE / caseSize));
        //std::shuffle(first, last, rand);
    }

    // �����̵�״̬���п��ټ��
    ::testing::AssertionResult trivialCheck(const Board& board) {
        // ������δ�µĸ�������Ҫ����
        if (board.moveCounts(Player::Black) + board.moveCounts(Player::White) + board.moveCounts(Player::None) != GameConfig::BOARD_SIZE) {
            return ::testing::AssertionFailure() << "Moves size sum not compatible.";
        }
        // ��Ϸδ����ʱ��m_curPlayer != Player::None����һ��û��Ӯ�ң�m_winner = Player::None��
        if ((!(board.m_curPlayer != Player::None) || board.m_winner == Player::None) == false) { // �̺���ϵʽ
            return ::testing::AssertionFailure() << "Winner is not none when game does not end.";
        }
        return ::testing::AssertionSuccess();
    }

    Board board;
};

// �Գ��Լ�飺ApplyMove��RevertMove
// RevertMove()�Ļ����������������
TEST_F(BoardTest, MoveSymmetry) {
    Board board_cpy(board);
    Position positions[caseSize];
    randomlyFill(begin(positions), end(positions));
    for (int i = 0; i < caseSize; ++i) {
        int offset = rand() % 3 + 1;
        // ���ѡȡ��������Position����apply��revert
        int j;
        for (j = 0; j < offset && i + j < caseSize; ++j) {
            board.applyMove(positions[i + j]);
            ASSERT_TRUE(trivialCheck(board));
        }
        for (--j; j >= 0; --j) {
            Player curPlayer = board.m_curPlayer;
            Player result = board.revertMove(positions[i + j]);
            ASSERT_TRUE(trivialCheck(board));
            EXPECT_EQ(result, (curPlayer = -curPlayer));
            // ��ԭλ�ٻ�һ���壬�����һ���Ƿ���Ϊ��ǰ��ң�������Ч��
            board.revertMove(positions[i + j]);
            EXPECT_EQ(result, curPlayer);
        }
        EXPECT_EQ(board, board_cpy);
    }
}

// TODO: ��revertMove()����Ϸ�������Ƿ��ܵ��óɹ�
TEST_F(BoardTest, CheckVictory) {
    // ������Ӯ��һ����˳
    Player curPlayer = Player::Black;
    Position blacks[9] = { {3,3}, {3,4}, {4,4}, {3,5}, {5,5}, {3,6}, {6,6}, {3,7}, {7,7} };
    for (int i = 0; i < sizeof(blacks) / sizeof(Position); ++i) {
        EXPECT_NE(curPlayer, board.applyMove(blacks[i])); // �µ�һ������Ч��һ��
        curPlayer = -curPlayer;
    }
    ASSERT_TRUE(board.status().end);
    ASSERT_EQ(board.status().winner, Player::Black);
    for (int i = sizeof(blacks) / sizeof(Position) - 1; i >= 0; --i) {
        EXPECT_NE(curPlayer, board.revertMove(blacks[i])); // һ������ɹ���
        curPlayer = -curPlayer;
    }
    // ������Ӯ��һ����˳
    Position whites[10] = { {3,3}, {3,4}, {4,4}, {3,5}, {5,5}, {3,6}, {6,6}, {3,7}, {8,8}, {3,8} };
    for (int i = 0; i < sizeof(whites) / sizeof(Position); ++i) {
        EXPECT_NE(curPlayer, board.applyMove(whites[i])); // �µ�һ������Ч��һ��
        curPlayer = -curPlayer;
    }
    ASSERT_TRUE(board.status().end);
    ASSERT_EQ(board.status().winner, Player::White);
    for (int i = sizeof(whites) / sizeof(Position) - 1; i >= 0; --i) {
        EXPECT_NE(curPlayer, board.revertMove(whites[i])); // һ������ɹ���
        curPlayer = -curPlayer;
    }
}

// ����һ�ֿ��Ժ�����·����м��
TEST_F(BoardTest, CheckTie) {
    for (int j = 0; j < HEIGHT; ++j) {
        // y��ӳ�䷽ʽΪ��
        // ��HEIGHT/2λ����0λ��ʼ��ÿλӳ��Ϊ0,2,4,6,8...
        // ��HEIGHT/2λ����(HEIGHT+1)/2λ��ʼ��ÿλӳ��Ϊ1,3,5,7,9...
        int y = j <= HEIGHT/2 ? 2*j : 2*(j - HEIGHT/2) - 1;
        for (int i = 0; i < WIDTH; ++i) {
            // x��ӳ�䷽ʽΪ��i -> x
            int x = i;
            // ���嵽{x, y}��������ؼ��
            Player result = board.applyMove({x, y});
            ASSERT_TRUE(trivialCheck(board));
            if (j * WIDTH + i == GameConfig::BOARD_SIZE - 1) {
                EXPECT_EQ(result, Player::None);
                EXPECT_EQ(board.m_curPlayer, Player::None);
                EXPECT_EQ(board.m_winner, Player::None);
            } else {
                // ��Ϸһ��û�н���
                ASSERT_NE(result, Player::None);
                ASSERT_NE(board.m_curPlayer, Player::None);
            }
        }
    }
    // ������������������ȡ���Ƿ���׳��쳣
    ASSERT_THROW(board.getRandomMove(), std::exception) << "Random move does not throw exception when board is full.";
}

// �������һ����������
// getRandomMove()��applyMove()�������������������
TEST_F(BoardTest, RandomRollout) {
    Board board_cpy(board);
    while (true) {
        Position move = board.getRandomMove();
        Player curPlayer = board.m_curPlayer;
        Player result = board.applyMove(move);
        auto status = board.status();
        ASSERT_TRUE(trivialCheck(board));
        EXPECT_NE(result, curPlayer); // getRandomMove()��Ҫ��֤һ�������µ���Чλ�ã������н���Ҳ��һ����
        if (board.m_curPlayer != Player::None) {
            // ����Ϸδ����
            EXPECT_EQ(result, -curPlayer); // ��һ����Ϊ������
            EXPECT_EQ(status.end, false);
            EXPECT_EQ(status.winner, Player::None);
        } else {
            // ����Ϸ�ѽ���
            EXPECT_EQ(result, Player::None);
            EXPECT_EQ(status.end, true);
            EXPECT_NE(status.winner, -curPlayer); // Ӯ��һ�������Ƕ���
            break;
        }
        // ��ʼ����Ч�ֽ��м��
        curPlayer = result; // ����ǰ���ת����ԭboard����һ�ֺ�Ӧ�µ����
        board_cpy.applyMove(move); // ������board����һ��
        result = board.applyMove(move); // ��ԭboardԭλ������һ��
        EXPECT_EQ(board, board_cpy) << "board does not remain the same after applied an invalid move"; // ��Ϊ������Ч������ԭboardӦ���ޱ仯
        ASSERT_EQ(result, curPlayer); // resultһ����Ϊ��ǰ��ң���ǰ�����Ҫ���£�
    }
}
