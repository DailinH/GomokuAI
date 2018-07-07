#ifndef GOMOKU_PATTERN_MATCHING_H_
#define GOMOKU_PATTERN_MATCHING_H_
#include "Game.h"
#include <utility>
#include <string_view>

namespace Gomoku {

inline namespace Config {
// ģʽƥ��ģ����������
enum PatternConfig {
    MAX_PATTERN_LEN = 7,
    BLOCK_SIZE = 2*3 + 1,
    TARGET_LEN = 2 * MAX_PATTERN_LEN - 1
};
}

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
    auto [x, y] = pose; auto [dx, dy] = *dir;
    return { x + offset * dx, y + offset * dy };
}

// �����±���Directionö��ֵһһ��Ӧ
constexpr Direction Directions[] = {
    Direction::Horizontal, Direction::Vertical, Direction::LeftDiag, Direction::RightDiag
};


struct Pattern {
    enum Type {
        DeadOne, LiveOne, 
        DeadTwo, LiveTwo, 
        DeadThree, LiveThree, 
        DeadFour, LiveFour, 
        Five, Size
    };

    std::string str;
    Player favour;
    Type type;
    int score;

    Pattern(std::string_view proto, Type type, int score);
    Pattern(const Pattern&) = default;
    Pattern(Pattern&&) = default;
};


class PatternSearch {
public:
    // ����friendָ����ʵ����ʹ����AC�Զ�����
    friend class AhoCorasickBuilder;

    using Entry = std::tuple<const Pattern&, int>;

    struct generator {
        std::string_view target = "";
        PatternSearch* ref = nullptr;
        size_t offset = 0, state = 0;

        generator begin() { return state == 0 ? ++(*this) : *this; }
        generator end()   { return generator{}; }

        Entry operator*() const;
        const generator& operator++();
        bool operator!=(const generator& other) const { return target != other.target; }
    };

    PatternSearch(std::initializer_list<Pattern> protos);
    
    generator execute(std::string_view target);
    std::vector<Entry> matches(std::string_view target);

private:
    std::vector<int> m_base;
    std::vector<int> m_check;
    std::vector<int> m_fail;
    std::vector<Pattern> m_patterns;
};


class BoardMap {
public:
    static std::tuple<int, int> ParseIndex(Position pose, Direction direction);

    explicit BoardMap(Board* board = nullptr);

    std::string_view lineView(Position pose, Direction direction);

    Player applyMove(Position move);

    Player revertMove(size_t count = 1);

    void reset();

public:
    std::unique_ptr<Board> m_board;
    std::array<std::string, 3*(WIDTH + HEIGHT) - 2> m_lineMap;
    unsigned long long m_hash;
};


class Evaluator {
public:
    struct Record {
        unsigned short field; // 4 White-Black��� * 4 ����λ || 2 White/Black�ָ� * 8����λ
        void set(int delta, Player player); // �������������8λ����λ��
        void set(int delta, Player favour, Player perspective, Direction dir);
        bool get(Player favour, Player perspective, Direction dir) const; // ��ȡĳһ���ĳһ����λ�Ƿ����á�
        unsigned get(Player favour, Player perspective) const; // �������һ���µ�4������λ��
        unsigned get(Player player) const; // ��������ͷ���8λ����λ��
    };

    struct Compound {
        enum Type { DoubleThree, FourThree, DoubleFour, Size };
        static bool Test(Evaluator* ev, Position pose, Player player);
        static void Update(Evaluator* ev, int delta, Position pose, Player player);
        static const Pattern::Type Components[3]; // ��������ɸ���ģʽ�ĵ�ģʽ���͡�
        static const int BaseScore; // ˫��/����/˫�Ĺ���һ��������
    };

    // �� { Player::Black, Player::White } ���ɵ�2*2���������顣
    static constexpr int Group(Player favour, Player perspective) {
        return (favour == Player::Black) << 1 | (perspective == Player::Black);
    }

    // ����AC�Զ���ʵ�ֵĶ�ģʽƥ������
    static PatternSearch Patterns;

    // ����Eigen������������Map����ʵ�ֵ����������ܶȼ�������tuple���: { Ȩ�أ� ���� }��
    static std::tuple<Eigen::Array<int, BLOCK_SIZE, BLOCK_SIZE>, int> BlockWeights;

    template<size_t Size>
    using Distribution = std::array<std::array<Record, Size>, BOARD_SIZE + 1>; // ���һ��Ԫ�������ܼ���

public:
    explicit Evaluator(Board* board = nullptr);

    auto& board() { return *m_boardMap.m_board; }

    auto& scores(Player player, Player perspect) { return m_scores[Group(player, perspect)]; }

    auto& density(Player player) { return m_density[player == Player::Black]; }

    Player applyMove(Position move);

    Player revertMove(size_t count = 1);

    bool checkGameEnd();  // ����Evaluator�����ǿ����������ټ����Ϸ�Ƿ������

    void syncWithBoard(const Board& board); // ͬ��Evaluator�������Board״̬��

    void reset();

private:
    void updateMove(Position move, Player src_player);

    void updateLine(std::string_view target, int delta, Position move, Direction dir);

    void updateBlock(int delta, Position move, Player src_player);

public:
    BoardMap m_boardMap; // �ڲ�ά����һ��Board, �����ܵ��ⲿ�ĸ���
    Distribution<Pattern::Size - 1> m_patternDist; // ��ͳ��Pattern::Five�ֲ�
    Distribution<Compound::Size> m_compoundDist;
    Eigen::VectorXi m_density[2];
    Eigen::VectorXi m_scores[4];
};

}

#endif // !GOMOKU_PATTERN_MATCHING_H_