#include <unordered_map>
#include <vector>


namespace Gomoku {

using namespace std;

static constexpr int width = 19;
static constexpr int height = 19;
static constexpr int max_renju = 5;

class Player {
public:
    enum : __int8 { White = -1, None = 0, Black = 1 } type;

public:
    Player(decltype(type) type) : type(type) {}

    constexpr Player operator-() {  return Player(-this->type); }
    constexpr operator bool() { return this->type == Player::None; }    
};

namespace Test {
    enum class Player : __int8 { White = -1, None = 0, Black = 1 };

    //constexpr operator bool(const Player& player) { return static_cast<bool>(player); }
    constexpr Player operator-(const Player& player) { return Player(-static_cast<int>(player)); }
}


struct Position {
    int id;

    Position(int id) : id(id) { }
    Position(int x, int y) {}

    operator int() const { return id; }

    int x() const { return 0; }
    int y() const { return 0; }

};

/*
    �������������ܻᱻƵ����������˿����ɱ���Ҫ��Ƶþ����͡�
    ���̵Ĵ洢�м���ѡ��
    1. bitset: ʹ��bitset�洢�������̡�����ÿ��������Ҫ�����������״̬����/��/�ޣ���������Ҫ2bit�洢��
       ���һ��19*19��������Ҫ����19*19*2=722bit=90.25Byte��
    2. pos -> player��map��һ��unordered_map<Position, Player>��MSVC��Ϊ>=32Byte��һ��{pos, player}������Ϊ2�ֽڡ�
       ���ڲ���Hash Table���ƣ� �ռ�����ܿ��ܸ��󡭡�
    3. �洢pair<pos, player>��vector��
*/
class Board {
public:

    /*
        ����ֵ����ΪPlayer��������һ��Ӧ�µ���ҡ�������ͣ�
        - ��PlayerΪ���֣��������һ��Ӧ�����£�����������
        - ��PlayerΪ�������������һ����Ч��Ӧ�����¡�
        - ��PlayerΪNone��������ü������ˣ�����һ���Ѿ���ʤ��
    */
    Player applyMove(const Position& move) {
        if (checkMove(move)) {
            moves[move] = current;
            current = -current;
            return checkVictory(move) ? Player::None : current;
        } else {
            return current;
        }
    }

private:

    bool checkMove(const Position& move) {
        // �����������
        return true;
    }

    // ÿ��һ��������ʤ����飬������ֻ��Ե�ǰ������Χ���б������ɡ�
    bool checkVictory(const Position& move) {
        const int curX = move.x(), curY = move.y();
        
        // �ز�ͬ�����������������
        const auto search = [&](auto direction) {
            // �ж�����ĸ����Ƿ�δԽ���ҹ���Ϊ��ǰ���
            const auto isCurrentPlayer = [&](int x, int y) {
                return (x >= 0 && x < width) && (y >= 0 && y < height) && moves[Position(x, y)] == current;
            };

            int renju = 0; // ��ǰ���ӹ��ɵ����������

            // ��������
            int x = curX, y = curY;
            for (int i = 1; i < 4; ++i) {
                direction(x, y);
                if (isCurrentPlayer(x, y)) ++renju;
                else break;
            }

            // ��������
            x = -curX, y = -curY;        
            for (int i = 1; i < 4; ++i) {
                direction(x, y);
                if (isCurrentPlayer(-x, -y)) ++renju;
                else break;
            }

            return renju == 5;
        };
        
        return search([](int& x, int& y) { x += 1; }) // ��->��
            || search([](int& x, int& y) { y += 1; }) // ��->��
            || search([](int& x, int& y) { x += 1, y -= 1; }) // ����->����
            || search([](int& x, int& y) { x += 1, y += 1; });// ����->����
    }

public:
    
    unordered_map<Position, Player> moves;
    Player current;
};

}