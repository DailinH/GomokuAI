#ifndef GOMOKU_PATTERN_MATCHING_H_
#define GOMOKU_PATTERN_MATCHING_H_
#include "Mapping.h"
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

    // 一条匹配记录包含了{ 匹配到的模式, 相对于起始位置的偏移, 模式在原型里的索引 }
    using Entry = std::tuple<const Pattern&, int, int>;

    // 验证entry是否覆盖了某个点位（以相对原点的偏移表示）。默认为TARGET_LEN/2，即中心点。
    static bool HasCovered(const Entry& entry, size_t pose = TARGET_LEN / 2);

    // 仿照Python生成器模式编写的用于返回匹配结果的工具类。
    struct generator {
        std::string_view target = "";
        PatternSearch* ref = nullptr;
        int offset = -1, state = 0;

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

    // 构造函数中传入的模式原型将经过几层强化，获得完整的模式表。
    PatternSearch(std::vector<Pattern> protos);
    
    // 返回一个生成器，每一次解引用返回当前匹配到的模式，并移动到下一个模式。
    generator execute(std::string_view target);

    // 一次性直接返回所有查找到的记录。
    std::vector<Entry> matches(std::string_view target);

public:
    std::vector<Pattern> m_prototypes; // 可检索模式原型
    std::vector<Pattern> m_patterns;   // 可检索模式集合
    std::vector<int> m_trace; // 可检索模式指向其原型的指针

private:
    std::vector<int> m_base;  // DAT子结点基准数组
    std::vector<int> m_check; // DAT父结点检索数组
    std::vector<int> m_fail;  // AC自动机fail指针数组
    std::vector<int> m_invariants;   // AC自动机「不动点」状态数组
};

/*
<<<<<<< HEAD
class BoardMap {
public:
    static std::tuple<int, int> ParseIndex(Position pose, Direction direction);

    explicit BoardMap(Board* board = nullptr);

    std::string_view lineView(Position pose, Direction direction);

    Player applyMove(Position move);

    Player revertMove(size_t count = 1);

    void reset();

public:
    std::shared_ptr<Board> m_board;
    std::array<std::string, 3*(WIDTH + HEIGHT) - 2> m_lineMap;
    unsigned long long m_hash;
};
=======*/
struct Compound {
    // 复合模式的类型
    enum Type { DoubleThree, FourThree, DoubleFour, Size };
//>>>>>>> tmp

    // 一个复合模式的组件由{ 该组件所在方向, 该组件棋型 }组成。
    using Component = std::tuple<Direction, Pattern::Type>;

    // 检测指定的位置上是否有属于指定玩家的复合模式。前置声明了评估器。
    static bool Test(class Evaluator &ev, Position pose, Player player);

    // 复合模式的分数记录
    static std::array<const int, Compound::Size> Scores;

    // 复合模式的有效性记录
    static std::array<bool, Compound::Size> IsForbidden;

    // 表明该复合模式汇集的位置
    Position position;
    
    // 表明该复合模式对何方有利
    Player favour;

    // 记录该复合模式的类型
    Type type;

    // 记录构成复合模式的各单模式，及它们所在方向
    std::vector<Component> components;

    // 原局面的引用
    Evaluator& ev;

    // 构造函数，表明一个复合模式由评估器、位置与玩家组成。
    Compound(Evaluator& ev, Position pose, Player favour);

    // 定位状态机
    void locate(); 

    // 更新状态机
    void update(int delta);
    
// 用于更新的成员
private: 
    void updateCritical(int delta, Component component);
    void updateAntis(int delta, Component component);
    void updatePose(int delta, Position pose, Component component, Player perspective);

    PatternSearch::generator generator = {}; // 用于标定component的生成器
    Direction gen_dir = Direction(-1); // 当前正在工作的生成器的方向
    int count = 0; // 复合模式的子模式总计数
    int l3_count = 0; // 复合模式中活三的数量
    bool triple_cross = false; // 是否有三个模式汇集于一点
};


class Evaluator {
public:
    struct Record {
        std::uint32_t field; // 4 White-Black组合 * 4 方向组 * 2标记位 || 2 White/Black分割 * 16计数位
        void set(int delta, Player player); // 按玩家类型设置8位计数位。
        void set(int delta, Player favour, Player perspective, Direction dir);
        unsigned get(Player favour, Player perspective, Direction dir) const; // 获取某一组的某一方向的2标记位。
        unsigned get(Player favour, Player perspective) const; // 打包返回一组下的4*2个方向位。
        unsigned get(Player player) const; // 按玩家类型返回16位计数位。
    };

    // 按 Player::Black | Player::White 构成的二元分组。
    static constexpr int Group(Player player) {
        return player == Player::Black;
    }

    // 按 { Player::Black, Player::White } 构成的2*2列联表分组。
    static constexpr int Group(Player favour, Player perspective) {
        return (favour == Player::Black) << 1 | (perspective == Player::Black);
    }

    // 基于AC自动机实现的多模式匹配器。
    static PatternSearch Searcher;

    // 基于Eigen向量化操作与Map引用实现的区域棋子密度计数器，tuple组成: { 权重， 分数 }。
    static std::tuple<Eigen::Array<int, BLOCK_SIZE, BLOCK_SIZE, Eigen::RowMajor>, int> BlockWeights;

    template<size_t Size>
    using Distribution = std::array<std::array<Record, Size>, BOARD_SIZE + 1>; // 最后一个元素用于总计数

public:
    Evaluator();

    auto& board() { return m_boardMap.m_board; }

    auto& density(Player player) { return m_density[Group(player)]; }

    auto& scores(Player player, Player perspect) { return m_scores[Group(player, perspect)]; }

    auto& patternScores(Player player) { return m_patternScores[Group(player)]; }

    Player applyMove(Position move);

    Player revertMove(size_t count = 1);

    bool checkGameEnd();  // 利用Evaluator，我们可以做到快速检查游戏是否结束。

    void syncWithBoard(const Board& board); // 同步Evaluator至传入的Board状态。

    void reset();

private:
    class Updater {
    public:
        explicit Updater(Evaluator& ev) : ev(ev) { }
        void updateMove(Position move, Player src_player);
    private:
        void reset(int delta, Position move, Player player);
        void matchPatterns(Direction dir);
        void updatePatterns(Direction dir);
        void updateCompound(Direction dir);
        void updateBlock(int delta, Player src_player);
        auto& matchResults(int delta, Direction dir) { return results[delta == 1][int(dir)]; }
        Compound* findCompound(Position pose, Player player);
    private:
        int delta; // 变化量，取值为 { 1, -1 }
        Position move; // 更新的中心位置
        Player player; // 更新的源玩家（Player::None代表悔棋）
        Evaluator& ev; // 原Evaluator的引用
        std::vector<PatternSearch::Entry> results[2][4]; // 存储单模式匹配结果
        std::vector<std::tuple<Position, Player>> compound_keys; // 复合模式索引
        std::vector<Compound> compounds; // 待更新复合模式集合
    } m_updater;

public:
    BoardMap m_boardMap; // 内部维护了一个Board, 避免受到外部的干扰
    Distribution<Pattern::Size - 1> m_patternDist; // 不统计Pattern::Five分布
    Distribution<Compound::Size> m_compoundDist;
    Eigen::ArrayXi m_density[2][2]; // 第一维: { White, Black }, 第二维: { Σ1, Σweight }
    Eigen::VectorXi m_scores[4]; // 按照Group函数分组
    Eigen::VectorXi m_patternScores[2]; // 与Searcher + Compounds同步的向量，存储绝对分数（黑正/白负）
};              
/* ------------------- 数据区 ------------------- */

static std::vector<Pattern> protos = {
{ "+xxxxx",    Pattern::Five,      9999 },
{ "-_oooo_",   Pattern::LiveFour,  9000 },
{ "-xoooo_",   Pattern::DeadFour,  2500 },
{ "-o_ooo",    Pattern::DeadFour,  3000 },
{ "-oo_oo",    Pattern::DeadFour,  2600 },
{ "-~_ooo_~",  Pattern::LiveThree, 3000 },
{ "-x^ooo_~",  Pattern::LiveThree, 2900 },
{ "-^oo_o^",   Pattern::LiveThree, 2800 },
{ "-xooo__~",  Pattern::DeadThree, 510 },
{ "-xoo_o_~",  Pattern::DeadThree, 520 },
{ "-xoo__o~",  Pattern::DeadThree, 520 },
{ "-xo_oo_~",  Pattern::DeadThree, 530 },
{ "-xo__oo",   Pattern::DeadThree, 530 },
{ "-xooo__x",  Pattern::DeadThree, 500 },
{ "-xoo_o_x",  Pattern::DeadThree, 500 },
{ "-xoo__ox",  Pattern::DeadThree, 500 },
{ "-xo_oo_x",  Pattern::DeadThree, 500 },
{ "-x_ooo_x",  Pattern::DeadThree, 500 },
{ "-~oo__o~",  Pattern::DeadThree, 750 },
{ "-oo__oo",   Pattern::DeadThree, 540 },
{ "-o_o_o",    Pattern::DeadThree, 550 },
{ "-~oo__~",   Pattern::LiveTwo,   650 },
{ "-~_o_o_~",  Pattern::LiveTwo,   600 },
{ "-x^o_o_^",  Pattern::LiveTwo,   550 },
{ "-^o__o^",   Pattern::LiveTwo,   550 },
{ "-xoo___",   Pattern::DeadTwo,   150 },
{ "-xo_o__",   Pattern::DeadTwo,   160 },
{ "-xo__o_",   Pattern::DeadTwo,   170 },
{ "-o___o",    Pattern::DeadTwo,   180 },
{ "-x_oo__x",  Pattern::DeadTwo,   120 },
{ "-x_o_o_x",  Pattern::DeadTwo,   120 },
{ "-~o___~",   Pattern::LiveOne,   150 },
{ "-x~_o__^",  Pattern::LiveOne,   140 },
{ "-x~__o_^",  Pattern::LiveOne,   150 },
{ "-xo___~",   Pattern::DeadOne,   30 },
{ "-x_o___x",  Pattern::DeadOne,   40 },
{ "-x__o__x",  Pattern::DeadOne,   50 },
};
/*
static std::vector<Pattern> protos= {
{ "+xxxxx",    Pattern::Five,      100000 },
{ "-_oooo_",   Pattern::LiveFour,  9000 },
{ "-xoooo_",   Pattern::DeadFour,  2500 },
{ "-o_ooo",    Pattern::DeadFour,  3000 },
{ "-oo_oo",    Pattern::DeadFour,  2600 },
{ "-~_ooo_~",  Pattern::LiveThree, 3000 },
{ "-x^ooo_~",  Pattern::LiveThree, 2900 },
{ "-~o_oo~",   Pattern::LiveThree, 2800 },
{ "-~o~oo_~",  Pattern::DeadThree, 1400 },
{ "-~oo~o_~",  Pattern::DeadThree, 1200 },
{ "-x_o~oo~",  Pattern::DeadThree, 1300 },
{ "-x_oo~o~",  Pattern::DeadThree, 1100 },
{ "-xooo__~",  Pattern::DeadThree, 510 },
{ "-xoo_o_~",  Pattern::DeadThree, 520 },
{ "-xoo__o~",  Pattern::DeadThree, 520 },
{ "-xo_oo_~",  Pattern::DeadThree, 530 },
{ "-xo__oo",   Pattern::DeadThree, 530 },
{ "-xooo__x",  Pattern::DeadThree, 500 },
{ "-xoo_o_x",  Pattern::DeadThree, 500 },
{ "-xoo__ox",  Pattern::DeadThree, 500 },
{ "-xo_oo_x",  Pattern::DeadThree, 500 },
{ "-x_ooo_x",  Pattern::DeadThree, 500 },
{ "-~oo__o~",  Pattern::DeadThree, 750 },
{ "-oo__oo",   Pattern::DeadThree, 540 },
{ "-o_o_o",    Pattern::DeadThree, 550 },
{ "-~oo__~",   Pattern::LiveTwo,   650 },
{ "-~_o_o_~",  Pattern::LiveTwo,   600 },
{ "-x^o_o_^",  Pattern::LiveTwo,   550 },
{ "-^o__o^",   Pattern::LiveTwo,   550 },
{ "-xoo___",   Pattern::DeadTwo,   150 },
{ "-xo_o__",   Pattern::DeadTwo,   160 },
{ "-xo__o_",   Pattern::DeadTwo,   170 },
{ "-o___o",    Pattern::DeadTwo,   180 },
{ "-x_oo__x",  Pattern::DeadTwo,   120 },
{ "-x_o_o_x",  Pattern::DeadTwo,   120 },
{ "-~o___~",   Pattern::LiveOne,   150 },
{ "-x~_o__^",  Pattern::LiveOne,   140 },
{ "-x~__o_^",  Pattern::LiveOne,   150 },
{ "-xo___~",   Pattern::DeadOne,   30 },
{ "-x_o___x",  Pattern::DeadOne,   40 },
{ "-x__o__x",  Pattern::DeadOne,   50 },
};*/
}

#endif // !GOMOKU_PATTERN_MATCHING_H_
