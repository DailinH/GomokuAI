#include <string>
#include <iostream>
#include <iomanip>
#include "Game.hpp"
#include "MCTS.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

using namespace std;

//namespace Gomoku {
//void to_json(json& j, const Position& p) {
//    j = { { "x", p.x() }, { "y", p.y() } };
//}
//
//void from_json(const json& j, Position& p) {
//    p.id = static_cast<int>(j["y"]) * width + static_cast<int>(j["x"]);
//}
//}
//
//int main(int argc, const char* argv[]) {
//    json input, output;
//    cin >> input;
//
//    Gomoku::Position firstMove = input["requests"][0];
//    Gomoku::Player player = firstMove.x() == -1 && firstMove.y() == -1 ? Gomoku::Player::Black : Gomoku::Player::White;
//   
//    Gomoku::Board board;
//    Gomoku::MCTS mcts(player);
//
//    int turnID = input["responses"].size();
//
//    for (int i = 0; i < turnID; i++) {
//        if (i != 0) {
//            board.applyMove(input["requests"][i]);
//        }
//        board.applyMove(input["responses"][i]);
//    }
//    board.applyMove(input["requests"][turnID]);
//
//
//    Gomoku::Position nextMove = mcts.getNextMove(board);
//    output["response"] = { {"x", nextMove.x() }, { "y", nextMove.y() } };
//    cout << output << endl;
//
//    return 0;
//}

using namespace Gomoku;

void print(const Board& board, ostream& os = cout) {
    Player positions[width * height];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < width * height; ++j) {
            positions[j] = board.m_moveStates[i][j] ? Player(i - 1) : positions[j];
        }
    }
    os << hex << "  ";
    for (int i = 0; i < width; ++i) {
        os << i + 1 << " ";
    }
    os << "\n";
    for (int y = 0; y < height; ++y) {
        os << y + 1 << " ";
        for (int x = 0; x < width; ++x) {
            switch (positions[y * width + x]) {
            case Player::Black: os << "x"; break;
            case Player::White: os << "o"; break;
            case Player::None:  os << "."; break;
            }
            os << " ";
        }
        os << "\n";
    }
}

int main() {
    std::ios::sync_with_stdio(false);

    Board board;
    MCTS mcts(Player::White);

    print(board);
    for (int x, y; board.m_curPlayer != Player::None;) {
        cout << "\nYour  (x, y): ";
        cin >> hex >> x >> y;
        if (board.applyMove({ x - 1, y - 1 }) == Player::Black) {
            continue;
        }

        auto move = board.getRandomMove();
        //auto move = mcts.getNextMove(board);
        cout << "CPU's (x, y): " << move.x() + 1 << " " << move.y() + 1 << endl;
        board.applyMove(move);
        //cout << string(width - 3, ' ') << "Board\n";
        cout << endl;
        print(board);
    }

    cout << "\nWinner: " << (board.m_winner == Player::Black ? "You" : "CPU") << "!\n\n";

    return 0;
}