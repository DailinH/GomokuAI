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

enum class Direction : char {
    Horizontal, Vertical, LeftDiag, RightDiag
};

// ������ö�ٽ���ɾ����(��x, ��y)ֵ��
constexpr std::pair<int, int> operator*(Direction direction) {
    switch (direction) {
        case Direction::Horizontal: return { 1, 0 };
        case Direction::Vertical:   return { 0, 1 };
        case Direction::LeftDiag:   return { 1, 1 };
        case Direction::RightDiag:  return { -1, 1 };
        default: return { 0, 0 };
    }
}

constexpr Position Shift(Position pose, int offset, Direction dir) {
    auto [x, y] = pose; auto [dx, dy] = *dir;
    return { x + offset * dx, y + offset * dy };
}


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

    using Record = std::tuple<const Pattern&, int>;

    struct generator {
        std::string_view target = "";
        PatternSearch* ref = nullptr;
        size_t offset = 0, state = 0;

        const generator& begin() { return state == 0 ? ++(*this) : *this; }
        const generator& end()   { return generator{}; }

        Record operator*() const;
        const generator& operator++();
        bool operator!=(const generator& other) const { return target != other.target; }
    };

    PatternSearch(std::initializer_list<Pattern> protos);
    
    generator execute(std::string_view target);
    std::vector<Record> matches(std::string_view target);

private:
    std::vector<int> m_base;
    std::vector<int> m_check;
    std::vector<int> m_fail;
    std::vector<Pattern> m_patterns;
};


class BoardMap {
public:
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
        unsigned short field; // 4 White/Black��� * 4 ����λ
        void set(int delta, Player favour, Player perspective, Direction dir);
        bool get(Player favour, Player perspective, Direction dir) const; // ��ȡĳһ���ĳһ����λ�Ƿ�����
        unsigned get(Player favour, Player perspective) const; // �������һ���µ�4������λ
    };

    struct Compound {
        static const Pattern::Type Components[2] = { Pattern::LiveTwo };

        std::function<bool(Evaluator*, Position, Player)> check;
        enum Type { DoubleThree, FourThree, DoubleFour, Size } type;
        int score;
    };

public:
    // ����AC�Զ���ʵ�ֵĶ�ģʽƥ������
    static PatternSearch Patterns;

    // ����lambdaʵ�ֵĸ���ģʽƥ������
    static std::array<Compound, Compound::Size> Compounds;

    // ����Eigen������������Map����ʵ�ֵ����������ܶȼ�������tuple���: { Ȩ�أ� ���� }��
    static std::tuple<Eigen::Array<int, BLOCK_SIZE, BLOCK_SIZE>, int> BlockWeights;

    explicit Evaluator(Board* board = nullptr);

    auto& board() { return *m_boardMap.m_board; }

    Player applyMove(Position move);

    Player revertMove(size_t count = 1);

    // ����Evaluator�����ǿ����������ټ����Ϸ�Ƿ������
    bool checkGameEnd(); 

    // ͬ��Evaluator�������Board״̬��
    void syncWithBoard(const Board& board);

    void reset();

private:
    void updateMove(Position move, Player src_player);

    void updateLine(std::string_view target, int delta, Position move, Direction dir);

    void updateBlock(int delta, Position move, Player src_player);

public:
    BoardMap m_boardMap; // �ڲ�ά����һ��Board, �����ܵ��ⲿ�ĸ���
    std::vector<Record> m_patternDist[Pattern::Size - 1]; // ��ͳ��Pattern::Five�ֲ�
    std::vector<Record> m_compoundDist[Compound::Size];
    std::vector<int> m_density[2];
    std::vector<float> m_scores[2];
};

}

namespace std {

template<typename Enum = std::enable_if_t<false>>
constexpr size_t size() { return 0; }  /* will never get here */ 

template<>
constexpr size_t size<Gomoku::Direction>() { return 4; }

template<>
constexpr size_t size<Gomoku::Pattern::Type>() { return Gomoku::Pattern::Size; }

}

#endif // !GOMOKU_PATTERN_MATCHING_H_
