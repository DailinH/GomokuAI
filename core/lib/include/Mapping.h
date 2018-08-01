#ifndef GOMOKU_MAPPING_H_
#define GOMOKU_MAPPING_H_
#include "Game.h"
#include <cstdint>

namespace Gomoku {

// ���̵��ĸ�����ĳ����װ
enum class Direction : char {
    Horizontal, Vertical, LeftDiag, RightDiag
};

// ������ö�ٽ���ɾ����(��x, ��y)ֵ
constexpr std::pair<int, int> operator*(Direction direction) {
    switch (direction) {
        case Direction::Horizontal: return { 1, 0 };
        case Direction::Vertical:   return { 0, 1 };
        case Direction::LeftDiag:   return { 1, 1 };
        case Direction::RightDiag:  return { -1, 1 };
        default: return { 0, 0 };
    }
}

// ��pose����dir�����ƶ�offset����λ
constexpr Position Shift(Position pose, int offset, Direction dir) {
    return { pose.id + offset * Position(*dir) };
}

constexpr Position& SelfShift(Position& pose, int offset, Direction dir) {
    return pose.id += offset * Position(*dir), pose;
}

// �����±���Directionö��ֵһһ��Ӧ
constexpr Direction Directions[] = {
    Direction::Horizontal, Direction::Vertical, Direction::LeftDiag, Direction::RightDiag
};


// ��������������ַ�
constexpr char EncodePlayer(Player player) {
    switch (player) {
        case Player::White: return 'o';
        case Player::None:  return '-';
        case Player::Black: return 'x';
        default: return '?';
    }
}

// ���������ַ�
constexpr int EncodeCharset(char ch) {
    switch (int code = 0; ch) {
        case '-': case '_': case '^': case '~': ++code; // ��λ
        case '?': ++code; // Խ��λ
        case 'o': ++code; // ����
        case 'x': ++code; // ����
        default: return code;
    }
}

// ����״̬���뼯
constexpr int Codeset[] = { 1, 2, 3, 4 };


// �ɱ����ĸ���/��/б�߹��ɵ��ַ�������
class BoardLines {
public:
    BoardLines();

    std::string_view operator()(Position pose, Direction direction);

    void updateMove(Position move, Player player);

    void reset();

private:
    std::array<std::string, 3 * (WIDTH + HEIGHT) - 2> m_lines;
};


// ���������̵Ĺ�ϣ��
struct BoardHash {
    // Zobrist��ϣ���洢��/��/����������״̬
    static const std::array<std::uint64_t[3], BOARD_SIZE> Zorbrist;

    static std::uint64_t HashPose(Position pose, Player player) {
        return Zorbrist[pose.id][static_cast<int>(player) + 1];
    }

    BoardHash();

    void updateMove(Position move, Player player);

    void reset();

    // 64λ��ϣ����ͻ�ĸ��ʺܵ�
    std::uint64_t m_hash;
};


// ����ӳ����ܼ���
class BoardMap {
public:
    Player applyMove(Position move);

    Player revertMove(size_t count = 1);

    void reset();

public:
    Board m_board;
    BoardHash m_hasher;
    BoardLines m_lineView;
};

}

namespace std {

template <> 
struct hash<Gomoku::BoardMap> {
    // WARNING: 64λ���뻷����sizeof(size_t)����Ϊ64λ
    std::size_t operator()(const Gomoku::BoardMap& boardMap) {
        return boardMap.m_hasher.m_hash;
    }
};

}

#endif // !GOMOKU_MAPPING_H_