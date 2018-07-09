#ifndef GOMOKU_PATTERN_MATCHING_H_
#define GOMOKU_PATTERN_MATCHING_H_
#include "Game.h"
#include <utility>
#include <string_view>

namespace Gomoku {

inline namespace Config {
// 模式匹配模块的相关配置
enum PatternConfig {
    MAX_PATTERN_LEN = 7,
    BLOCK_SIZE = 2*3 + 1,
    TARGET_LEN = 2 * MAX_PATTERN_LEN - 1
};
}

// 棋盘的四个方向的抽象封装
enum class Direction : char {
    Horizontal, Vertical, LeftDiag, RightDiag
};

// 将方向枚举解包成具体的(Δx, Δy)值
constexpr std::pair<int, int> operator*(Direction direction) {
    switch (direction) {
        case Direction::Horizontal: return { 1, 0 };
        case Direction::Vertical:   return { 0, 1 };
        case Direction::LeftDiag:   return { 1, 1 };
        case Direction::RightDiag:  return { -1, 1 };
        default: return { 0, 0 };
    }
}

// 将pose按照dir方向移动offset个单位
constexpr Position Shift(Position pose, int offset, Direction dir) {
    auto [x, y] = pose; auto [dx, dy] = *dir;
    return { x + offset * dx, y + offset * dy };
}

// 数组下标与Direction枚举值一一对应
constexpr Direction Directions[] = {
    Direction::Horizontal, Direction::Vertical, Direction::LeftDiag, Direction::RightDiag
};


struct Pattern {
    /*
        该棋型的富信息字符串表示，具体为：
          'x': 黑棋，'o': 白棋
          '_': 对该棋型所属玩家来说有利的空位
          '^': 该棋型敌对玩家可用于反击的空位
          '~': 对双方玩家均无价值，但对该棋型而言必须存在的空位
    */
    std::string str;

    // 表明该模式对何方有利
    Player favour;
    
    // 表明该模式所属的棋型
    enum Type {
        DeadOne, LiveOne,
        DeadTwo, LiveTwo,
        DeadThree, LiveThree,
        DeadFour, LiveFour,
        Five, Size
    } type;
    
    // 对当前模式的空位的评分
    int score;

    // proto中的第一个字符为'+'或'-'，分别代表对黑棋与白棋有利。
    Pattern(std::string_view proto, Type type, int score);
};


class PatternSearch {
public:
    // 利用friend指明类实现里使用了AC自动机。
    friend class AhoCorasickBuilder;

    // 一条匹配记录包含了{ 匹配到的模式, 相对于起始位置的偏移 }
    using Entry = std::tuple<const Pattern&, int>;

    // 仿照Python生成器模式编写的用于返回匹配结果的工具类。
    struct generator {
        std::string_view target = "";
        PatternSearch* ref = nullptr;
        short offset = -1, state = 0;

        generator begin() { return state == 0 ? ++(*this) : *this; } // ++是为了保证从begin开始就有匹配结果。
        generator end()   { return generator{}; } // 利用空生成器的空目标串("")代表匹配结束。

        Entry operator*() const; // 返回当前状态对应的记录。
        const generator& operator++(); // 将自动机状态移至下一个有匹配模式的位置。
        bool operator!=(const generator& other) const { // target与state完整标记了匹配状态。
            return std::tie(target, state) != std::tie(other.target, other.state);
        }
    };

    // 构造一个未初始化的搜索器。
    PatternSearch() = default;

    // 构造函数中传入的模式原型将经过几层强化，获得完整的模式表
    PatternSearch(std::initializer_list<Pattern> protos);
    
    // 返回一个生成器，每一次解引用返回当前匹配到的模式，并移动到下一个模式
    generator execute(std::string_view target);

    // 一次性直接返回所有查找到的记录
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
        unsigned short field; // 4 White-Black组合 * 4 方向位 || 2 White/Black分割 * 8计数位
        void set(int delta, Player player); // 按玩家类型设置8位计数位。
        void set(int delta, Player favour, Player perspective, Direction dir);
        bool get(Player favour, Player perspective, Direction dir) const; // 获取某一组的某一方向位是否设置。
        unsigned get(Player favour, Player perspective) const; // 打包返回一组下的4个方向位。
        unsigned get(Player player) const; // 按玩家类型返回8位计数位。
    };

    struct Compound {
        enum Type { DoubleThree, FourThree, DoubleFour, Size };
        static bool Test(Evaluator* ev, Position pose, Player player);
        static void Update(Evaluator* ev, int delta, Position pose, Player player);
        static const Pattern::Type Components[3]; // 被用于组成复合模式的单模式类型。
        static const int BaseScore; // 双三/四三/双四共用一个分数。
    };

    // 按 { Player::Black, Player::White } 构成的2*2列联表分组。
    static constexpr int Group(Player favour, Player perspective) {
        return (favour == Player::Black) << 1 | (perspective == Player::Black);
    }

    // 基于AC自动机实现的多模式匹配器。
    static PatternSearch Patterns;

    // 基于Eigen向量化操作与Map引用实现的区域棋子密度计数器，tuple组成: { 权重， 分数 }。
    static std::tuple<Eigen::Array<int, BLOCK_SIZE, BLOCK_SIZE>, int> BlockWeights;

    template<size_t Size>
    using Distribution = std::array<std::array<Record, Size>, BOARD_SIZE + 1>; // 最后一个元素用于总计数

public:
    explicit Evaluator(Board* board = nullptr);

    auto& board() { return *m_boardMap.m_board; }

    auto& scores(Player player, Player perspect) { return m_scores[Group(player, perspect)]; }

    auto& density(Player player) { return m_density[player == Player::Black]; }

    Player applyMove(Position move);

    Player revertMove(size_t count = 1);

    bool checkGameEnd();  // 利用Evaluator，我们可以做到快速检查游戏是否结束。

    void syncWithBoard(const Board& board); // 同步Evaluator至传入的Board状态。

    void reset();

private:
    void updateMove(Position move, Player src_player);

    void updateLine(std::string_view target, int delta, Position move, Direction dir);

    void updateBlock(int delta, Position move, Player src_player);

public:
    BoardMap m_boardMap; // 内部维护了一个Board, 避免受到外部的干扰
    Distribution<Pattern::Size - 1> m_patternDist; // 不统计Pattern::Five分布
    Distribution<Compound::Size> m_compoundDist;
    Eigen::VectorXi m_density[2];
    Eigen::VectorXi m_scores[4];
};

}

#endif // !GOMOKU_PATTERN_MATCHING_H_
