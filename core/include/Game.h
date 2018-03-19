#ifndef GOMOKU_GAME_H_
#define GOMOKU_GAME_H_
#include <utility> // std::pair, std::size_t
#include <array>   // std::array, 

namespace Gomoku {

// ��Ϸ�Ļ�������
enum GameConfig {
    WIDTH = 15, HEIGHT = 15, MAX_RENJU = 5, BOARD_SIZE = WIDTH*HEIGHT
};


// ��Ҹ���ĳ����װ
enum class Player : char { 
    White = -1, None = 0, Black = 1 
};

// ���ض��ֵ�Playerֵ��Player::None�Ķ�������Player::None��
constexpr Player operator-(Player player) { 
    return Player(-static_cast<char>(player)); 
}

// ������Ϸ������ĵ÷֡�ͬ�ţ�winner��player��ͬ��Ϊ�������Ϊ����ƽ��Ϊ�㡣
constexpr double getFinalScore(Player player, Player winner) { 
    return static_cast<double>(player) * static_cast<double>(winner); 
}


// ��������ʾ��һ/�������޵����ꡣҲ����˵��x/y�������ͬ����ͬ����
struct Position {
    int id;

    Position(int id = -1) : id(id) {}
    Position(int x, int y) : id(y * WIDTH + x) {}
    Position(std::pair<int, int> pose) : id(pose.second * WIDTH + pose.first) {}

    operator int() const { return id; }
    constexpr int x() const { return id % WIDTH; }
    constexpr int y() const { return id / WIDTH; }

    // ��֤Position������Ϊ��ϣ���keyʹ��
    struct Hasher {
        std::size_t operator()(const Position& position) const {
            return position.id;
        }
    };
};

/*
    ʱ�俪��������
    ������Ҫ�ڵ��̵߳Ļ����Ͻ������Ż������£���˸��ֲ����Ŀ�����Ҫ�����͡�
    * ����������֪��Ϊ�˷����������ѵ�������ÿһ������״̬����/��/�ޣ��ֱ���һ�������洢��
    ������̿��������ƣ� Array[3] = { ״̬����(����), ״̬����(����), ״̬����(����) }��
    �ڴ�֮�ϣ��Դ洢��״̬��ʹ�õ��������з�����
        1. unordered_set<Position>: �ص��ǿ��Բ��ô洢�������̣�ֻ�����϶�Ӧ״̬��λ�ü��ɡ�
        ���ǣ�����profiler��⣬ʵ���϶������ò����Ŀ��������˵�Եù����˵㡣
        2. std::array<bool>: �ص��Ǵ洢���������̣�index����λ�ã�value��ʾ��λ���Ƿ������Ӧ״̬��
        �ŵ����ڵ�ǰ����£�ÿһ�ֲ��������������ǳ��졣����Ҫ����洢һ��size_t��������¼ÿ��״̬��������Ŀ��
        3. bitset: ʹ��bitset,��������std::arrayһ����Ч������Ϊ3��2�Ľ��հ汾����
        ����Ч������д�profiler��⡣

    �ռ俪��������
    �������������ܻᱻƵ����������Ҫ�������Ļ��������ɱ�����Ƶþ����͡�
    ���̵Ĵ洢�м���ѡ��
        1. bitset: ʹ��bitset�洢�������̡�����ÿ��������Ҫ�����������״̬����/��/�ޣ���������Ҫ2bit�洢��
        ���һ��19*19��������Ҫ����19*19*2=722bit=90.25Byte��
        2. pos -> player��map��һ��unordered_map<Position, Player>��MSVC��Ϊ>=32Byte��һ��{pos, player}������Ϊ3�ֽڡ�
        ���ڲ���Hash Table���ƣ� �ռ�����ܿ��ܸ��󡭡�
        3. ͬʱ�俪��������2, 3������2��ջ�����飬3��ջ��bit�У�������ǵĿ������������˵���ϵ�һЩ��
*/
class Board {
// �����ӿڲ���
public:
    Board();

    /*
        ���塣����ֵ����ΪPlayer��������һ��Ӧ�µ���ҡ�������ͣ�
        - ��PlayerΪ���֣��������һ��Ӧ�����£�����������
        - ��PlayerΪ�������������һ����Ч��Ӧ�����¡�
        - ��PlayerΪNone���������һ������Ϸ�ѽ�������ʱ������ͨ��m_winner��ȡ��Ϸ�����
    */
    Player applyMove(Position move, bool checkVictory = true);

    /*
        ���塣����ֵ����ΪPlayer��������һ��Ӧ�µ���ҡ�������ͣ�
        - ��PlayerΪ���֣���������ɹ�����Ϊ����������塣
        - ��PlayerΪ��������������ʧ�ܣ�����״̬���䡣
    */
    Player revertMove(Position move);

    // ÿ��һ���������վּ�飬������ֻ��Ե�ǰ������Χ���б������ɡ�
    Position getRandomMove() const;

    // Խ�������Ӽ�顣���޽��ֹ���
    bool checkMove(Position move);

    // ÿ��һ���������վּ�飬������ֻ��Ե�ǰ������Χ���б������ɡ�
    bool checkGameEnd(Position move);
    
// ���ݳ�Ա��װ����
public: 
    // ���ص�ǰ��Ϸ״̬
    const auto status() const {
        struct { bool end; Player curPlayer; Player winner; } status {
            m_curPlayer == Player::None, m_curPlayer, m_winner 
        };
        return status;
    }

    // ͨ��Playerö�ٻ�ȡ��Ӧ����״̬
    std::array<bool, BOARD_SIZE>&       moveStates(Player player) { return m_moveStates[static_cast<int>(player) + 1]; }
    const std::array<bool, BOARD_SIZE>& moveStates(Player player) const { return m_moveStates[static_cast<int>(player) + 1]; }

    // ͨ��Playerö�ٻ�ȡ������/δ��������
    std::size_t& moveCounts(Player player) { return m_moveCounts[static_cast<int>(player) + 1]; }
    std::size_t  moveCounts(Player player) const { return m_moveCounts[static_cast<int>(player) + 1]; }

// ���ݳ�Ա����
public:
    /*
        ��ʾ�ڵ�ǰ����״̬��Ӧ�µ���ҡ�
        ��ֵΪPlayer::Noneʱ��������Ϸ������
        Ĭ��Ϊ�������¡�
    */
    Player m_curPlayer = Player::Black;

    /*
        ����ֽ�������ֵ����������Ϸ���:
        - Player::Black: ��Ӯ
        - Player::White: ��Ӯ
        - Player::None:  �;�
        ����ֻ�δ����ʱ��m_curPlayer != Player::None����ֵʼ�ձ���ΪNone������û��Ӯ�ҡ�
    */
    Player m_winner = Player::None;

    /*
        ���������ʾ�������ϵ�״̬�������Ӹ��������±��Ӧ��ϵΪ��
        0 - Player::White - ���ӷ������
        1 - Player::None  - ������λ�����
        2 - Player::Black - ���ӷ������
        ����Ϊʲô��ʹ��std::vector�أ���Ϊvector<bool> :)��
    */
    std::array<bool, BOARD_SIZE> m_moveStates[3] = {};
    std::size_t m_moveCounts[3] = {};
};

}

#endif // !GAME_H_
