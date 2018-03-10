#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace Gomoku {

using namespace std; // FIXME����ϰ�ߣ���δ������Ǩ�Ƶ�cpp��

static constexpr int width = 15;
static constexpr int height = 15;
static constexpr int max_renju = 5;

enum class Player : char { White = -1, None = 0, Black = 1 };

constexpr Player operator-(Player player) {
    // ���ض���Playerֵ��Player::None�Ķ�������Player::None
    return Player(-static_cast<char>(player));
}

constexpr float getFinalScore(Player player, Player winner) {
    // ͬ�ţ�winner��player��ͬ��Ϊ�������Ϊ����ƽ��Ϊ��
    return static_cast<float>(player) * static_cast<float>(winner);
}

// ��������ʾ��һ/�������޵����ꡣҲ����˵��x/y�������ͬ����ͬ����
struct Position {
    int id;

    Position(int id = -1) : id(id) { }
    Position(int x, int y) : id(y * width + x) {}
    Position(std::pair<int, int> pose) : id(pose.second * width + pose.first) {}

    operator int() const { return id; }

    constexpr int x() const { return id % width; }
    constexpr int y() const { return id / width; }

    // ��֤Position������Ϊ��ϣ���keyʹ��
    struct Hasher {
        std::size_t operator()(const Position& position) const {
            return position.id;
        }
    };
};


/*
    �������������ܻᱻƵ����������˿����ɱ���Ҫ��Ƶþ����͡�
    ���̵Ĵ洢�м���ѡ��
    1. bitset: ʹ��bitset�洢�������̡�����ÿ��������Ҫ�����������״̬����/��/�ޣ���������Ҫ2bit�洢��
       ���һ��19*19��������Ҫ����19*19*2=722bit=90.25Byte��
    2. pos -> player��map��һ��unordered_map<Position, Player>��MSVC��Ϊ>=32Byte��һ��{pos, player}������Ϊ3�ֽڡ�
       ���ڲ���Hash Table���ƣ� �ռ�����ܿ��ܸ��󡭡�
    3. �洢pair<pos, player>��vector��
*/
class Board {
public:
    static Board load(wstring boardPath) {
        
    }

public:
    Board() : 
        m_curPlayer(Player::Black), 
        m_winner(Player::None),
        m_appliedMoves(width * height),
        m_availableMoves(width * height) {
        for (int i = 0; i < width * height; ++i) {
            m_availableMoves.emplace(i);
        }
    }

    /*
        ����ֵ����ΪPlayer��������һ��Ӧ�µ���ҡ�������ͣ�
        - ��PlayerΪ���֣��������һ��Ӧ�����£�����������
        - ��PlayerΪ�������������һ����Ч��Ӧ�����¡�
        - ��PlayerΪNone���������һ������Ϸ�ѽ�������ʱ������ͨ��m_winner��ȡ��Ϸ�����
    */
    Player applyMove(Position move) {
        // �����Ϸ�Ƿ�δ������Ϊ��Ч��һ��
        if (m_curPlayer != Player::None && checkMove(move)) {
            m_appliedMoves[move] = m_curPlayer;
            m_availableMoves.erase(move);
            m_curPlayer = checkVictory(move) ? Player::None : -m_curPlayer;
        }
        return m_curPlayer;
    }

    Player revertMove(Position move) {
        if (m_appliedMoves.count(move) && m_appliedMoves[move] != m_curPlayer) {
            m_curPlayer = m_appliedMoves[move];
            m_availableMoves.insert(move);
            m_appliedMoves.erase(move);
        }
        return m_curPlayer;
    }

    const auto status() const {
        struct { bool end; Player winner; } status { 
            m_curPlayer == Player::None, 
            m_winner 
        };
        return status;
    }

    // a hack way to get a random move
    // referto: https://stackoverflow.com/questions/12761315/random-element-from-unordered-set-in-o1/31522686#31522686
    Position getRandomMove() const {
        if (m_availableMoves.empty()) {
            throw overflow_error("board is already full");
        }
        int divisor = (RAND_MAX + 1) / (width * height);
        auto rnd = Position(rand() % divisor);
        while (!m_availableMoves.count(rnd)) {
            rnd.id = rand() % divisor;
            //board.m_availableMoves.insert(rnd);
            //auto iter = board.m_availableMoves.find(rnd);
            //rnd = *(next(iter) == board.m_availableMoves.end() ? board.m_availableMoves.begin() : next(iter));
            //board.m_availableMoves.erase(iter);
        }
        return rnd;
    }

private:
    bool checkMove(Position move) {
        // �����������
        return true;
    }

    // ÿ��һ���������վּ�飬������ֻ��Ե�ǰ������Χ���б������ɡ�
    bool checkVictory(Position move) {
        const int curX = move.x(), curY = move.y();
        
        // �ز�ͬ�����������������
        const auto search = [&](int dx, int dy) {
            // �ж�����ĸ����Ƿ�δԽ���ҹ���Ϊ��ǰ����
            // TODO: Profiler test
            const auto isCurrentPlayer = [&](Position pose) {
                return (pose.x() >= 0 && pose.x() < width) && (pose.y() >= 0 && pose.y() < height) 
                    && (m_appliedMoves.count(pose) && m_appliedMoves[pose] == m_curPlayer);
            };

            int renju = 1; // ��ǰ���ӹ��ɵ����������

            // �����뷴������
            for (int sgn = 1; sgn != -1; sgn = -1) {
                int x = curX, y = curY;
                for (int i = 1; i < 4; ++i) {
                    x += sgn*dx, y += sgn*dy;
                    if (isCurrentPlayer({ x, y })) ++renju;
                    else break;
                }
            }

            return renju >= 5;
        };
        
        // �� ��->�� || ��->�� || ����->���� || ����->���� ��������
        if (search(1, 0) || search(0, 1) || search(1, -1) || search(1, 1)) {
            m_winner = m_curPlayer; // Ӯ��Ϊ��ǰ���
            return true;
        } else if (m_availableMoves.empty()) {
            m_winner = Player::None; // ��δӮ��֮��Ҳû�п���֮�أ���Ϊ�;�
            return true;
        } else {
            return false;
        }
    }

public:
    // ��ֵΪPlayer::Noneʱ��������Ϸ������
    Player m_curPlayer;

    /*
        ����ֽ�������ֵ����������Ϸ���:
        - Player::Black: ��Ӯ
        - Player::White: ��Ӯ
        - Player::None:  �;�
        ����ֻ�δ����ʱ��m_curPlayer != Player::None����ֵʼ�ձ���ΪNone������û��Ӯ�ҡ�
    */
    Player m_winner;

    // ����������λ�á��洢 { λ�ã�������ɫ } �ļ�ֵ�ԡ�
    unordered_map<Position, Player, Position::Hasher> m_appliedMoves;

    // ���п�����λ�á�����ֵ��ΪPlayer::None������set�洢���ɡ�
    unordered_set<Position, Position::Hasher> m_availableMoves;
};

}