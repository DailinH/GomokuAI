#include "Game.h"
#include <string>
#include <sstream>
#include <random>

using namespace std;
using namespace Gomoku;

static uniform_int_distribution<unsigned> rnd(0, BOARD_SIZE - 1); // 注意区间是[a, b]!
static mt19937 rnd_eng((random_device())());
static ostringstream oss;

// 由于是内联使用，不暴露成外部接口，因此无需进行额外参数检查，下同
inline void setState(Board* board, Player player, Position position) {
    board->moveStates(player)[position] = true;
    board->moveCounts(player) += 1;
}

inline void unsetState(Board* board, Player player, Position position) {
    board->moveStates(player)[position] = false;
    board->moveCounts(player) -= 1;
}

Board::Board() {
    this->m_moveRecord.reserve(GameConfig::BOARD_SIZE / 3);
    this->reset();
}

Player Board::applyMove(Position move, bool checkVictory) {
    // 检查游戏是否未结束且为有效的一步
    if (m_curPlayer != Player::None && checkMove(move)) {
        setState(this, m_curPlayer, move);
        unsetState(this, Player::None, move);
        m_moveRecord.push_back(move);
        m_curPlayer = checkVictory && checkGameEnd() ? Player::None : -m_curPlayer; // 若checkVictory为false，则checkGameEnd()被短路。
    }
    return m_curPlayer;
}

Player Board::revertMove(size_t count) {
    // 需要保证游戏结束后，也能调用成功
    if (m_curPlayer == Player::None && count != 0) {
        m_curPlayer = moveCounts(Player::Black) == moveCounts(Player::White) ? Player::Black : Player::White;
        m_winner = Player::None; // 由于悔了一步，游戏回到没有赢家的状态
    }
    for (size_t i = 0; !m_moveRecord.empty() && i < count; ++i) {
        unsetState(this, -m_curPlayer, m_moveRecord.back());
        setState(this, Player::None, m_moveRecord.back());
        m_moveRecord.pop_back();
        m_curPlayer = -m_curPlayer;
    }
    return m_curPlayer;
}

Position Board::getRandomMove() const {
    if (moveCounts(Player::None) == 0) {
        throw overflow_error("board is already full");
    }
    int id = rnd(rnd_eng);
    while (!moveStates(Player::None)[id]) {
        id = (id + 1) % moveStates(Player::None).size();
    }
    return Position(id);
}

bool Board::checkMove(Position move) {
    return move.id >= 0 && move.id < BOARD_SIZE && moveStates(Player::None)[move];
}

bool Board::checkGameEnd() {
    // 若已结束则不必再检测
    if (m_curPlayer == Player::None) {
        return true;
    }

    // 获取最后一手的坐标
    const auto [curX, curY] = m_moveRecord.back();
        
    // 沿不同方向的搜索方法复用
    const auto search = [curX, curY, this](int dx, int dy) {
        int renju = 1; // 当前落子构成的最大连珠数

        // 正向与反向搜索
        for (int sgn : {1, -1}) {
            int x = curX, y = curY;
            for (int i = 1; i <= MAX_RENJU; ++i) {
                x += sgn * dx, y += sgn * dy;
                // 判断坐标的格子是否未越界且归属为当前棋子
                if ((x >= 0 && x < WIDTH) && (y >= 0 && y < HEIGHT)
                    && moveStates(m_curPlayer)[y*WIDTH + x]) ++renju;
                else break;
            }
        }

        return renju >= MAX_RENJU;
    };
        
    // 从 左上->右下 || 左下->右上 || 左->右 || 下->上  进行搜索
    if (search(1, -1) || search(1, 1) || search(1, 0) || search(0, 1)) {
        m_winner = m_curPlayer; // 赢家为当前玩家
        m_curPlayer = Player::None;
        return true;
    } else if (moveCounts(Player::None) == 0) {
        m_winner = Player::None; // 若未赢，之后也没有可下之地，则为和局
        m_curPlayer = Player::None;
        return true;
    } else {
        return false;
    }
}

void Board::reset() {
    for (auto player : { Player::Black, Player::None, Player::White }) {
        moveStates(player).fill(player == Player::None ? true : false);
        moveCounts(player) = (player == Player::None ? GameConfig::BOARD_SIZE : 0);
    }
    m_moveRecord.clear();
    m_curPlayer = Player::Black;
    m_winner = Player::None;
}

string std::to_string(Player player) {
    switch (player) {
    case Player::Black:
        return "Player Black";
    case Player::None:
        return "No Player";
    case Player::White:
        return "Player White";
    default:
        return "Player ???";
    }
}

string std::to_string(Position position) {
    oss.str("");
    oss << "(" << (int)position.x() << ", " << (int)position.y() << ")";
    return oss.str();
}

string std::to_string(const Board& board) {
    static Player positions[WIDTH * HEIGHT];

    oss.str("");
    for (int i : {0, 1, 2}) {
        for (int j = 0; j < WIDTH * HEIGHT; ++j) {
            positions[j] = board.m_moveStates[i][j] ? Player(i - 1) : positions[j];
        }
    }
    oss << hex << "  ";
    for (int i = 0; i < WIDTH; ++i) {
        oss << i + 1 << " ";
    }
    oss << "\n";
    for (int y = 0; y < HEIGHT; ++y) {
        oss << y + 1 << " ";
        for (int x = 0; x < WIDTH; ++x) {
            switch (positions[y * WIDTH + x]) {
            case Player::Black: oss << "x"; break;
            case Player::White: oss << "o"; break;
            case Player::None:  oss << "_"; break;
            }
            oss << " ";
        }
        oss << "\n";
    }
    oss << dec;

    return oss.str();
}