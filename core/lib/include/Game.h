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
