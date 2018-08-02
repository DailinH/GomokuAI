#ifndef GOMOKU_INTERFACE_H_
#define GOMOKU_INTERFACE_H_
#include <iostream>
#include <iomanip>
#include "Agent.h"

namespace Gomoku::Interface {

inline int BotzoneInterface(Agent& agent) {
    using namespace std;

    Board board;
    json input, output;

    cin >> input;

    int turnID = input["responses"].size();
    for (int i = 0; i < turnID; i++) {
        board.applyMove(input["requests"][i], false);  // requests size: n + 1
        board.applyMove(input["responses"][i], false); // responses size: n
    }
    board.applyMove(input["requests"][turnID], false); // play newest move

    agent.syncWithBoard(board);
    output["response"] = agent.getAction(board); // response to newest move
    output["debug"] = agent.debugMessage();

    cout << output << endl;

    return 0;
}

inline int KeepAliveBotzoneInterface(Agent& agent) {
    using namespace std;

    Board board;

    for (int turn = 0; ; ++turn) {
        json input, output;

        cin >> input;
        if (turn == 0) { // restore board state in first turn
            int turnID = input.count("responses") ? input["responses"].size() : 0;
            for (int i = 0; i < turnID; i++) {
                board.applyMove(input["requests"][i], false);  // requests size: n + 1
                board.applyMove(input["responses"][i], false); // responses size: n
            }
            board.applyMove(input["requests"][turnID], false); // play newest move
        } else {
            board.applyMove(input, false); // play newest move           
        }

        agent.syncWithBoard(board);
        board.applyMove(agent.getAction(board)); // response to newest move
        output["response"] = board.m_moveRecord.back(); 
        output["debug"] = agent.debugMessage();

        cout << output << "\n";
        cout << ">>>BOTZONE_REQUEST_KEEP_RUNNING<<<" << endl; // stdio flushed by endl
    }

    return 0;
}

// 若shuffle为false，则agent0先行。
inline int NewConsoleInterface(Agent& agent0, Agent& agent1, bool RIFRule = false, bool shuffle = true, bool debug = true) {
    using namespace std;

    enum class State {
        Opening, Swap, NProposals, Ongoing, End
    } state = RIFRule ? State::Opening : State::Ongoing;

    Board board;
    Player player = board.m_curPlayer;
    int proposal_count;

    tuple<int, Agent*> agents[2] = { {0, &agent0}, {1, &agent1} };
    auto get_agent = [&](Player p) { 
		auto& [index, ptr] = agents[p == Player::White]; 
		return tie(index, *ptr);
	};

    if (shuffle) {
        srand(time(nullptr));
        if (rand() % 2) {
            swap(agents[0], agents[1]);
        }
    }

    // 输出Agent简介
    for (auto player : { Player::Black, Player::White }) {
        auto [index, agent] = get_agent(player);
        printf("%s: agent%d.%s\n", to_string(player).c_str(), index, agent.name().c_str());
    }
    // 输出初始棋盘
    cout << endl << to_string(board);
    cout << "\n-------------------------\n";

    // 状态机正式工作
    while (state != State::End) {
        auto [index, agent] = get_agent(player);
        agent.syncWithBoard(board);

        switch (state) {
        case State::Opening:
        {
            switch (board.m_moveRecord.size()) {
            case 0:
                proposal_count = agent.queryProposalCount();
                printf("Agent%d announce %d proposals.\n", index, proposal_count);
				break;
            case 3:
                if (player == Player::Black) {
                    player = Player::White; // 改由白方下第4手棋
                    state = State::Swap; // 切换为三手交换状态
                    continue; // 下一次状态机将由假白方Agent响应
                }
				break;
            case 4:
                if (player == Player::White) {
                    player = Player::Black;
                    state = State::NProposals;
                    continue;
                }
				break;
            }
        } // 这里会穿透
        case State::Ongoing:
        {
            auto move = agent.getAction(board);
            if (move == Position(-1, -1)) {
                board.revertMove(2);
                cout << to_string(board);
                continue;
            } else if (!board.checkMove(move)) {
                cout << "Invalid move: " << to_string(move) << endl;
                continue;
            }
            auto result = board.applyMove(move);
            if (result == Player::None) {
                state = State::End;
                continue;
            } else {
                printf("\n%d.%s's move: %s:\n\n", index, agent.name().c_str(), to_string(move).c_str());
                printf("%s\n", to_string(board).c_str());
                printf("Debug Messages: %s", agent.debugMessage().dump().c_str());
                printf("\n-------------------------\n");
            }
            if (state == State::Ongoing) {
                player = board.m_curPlayer;
            }
            break;
        }
        case State::Swap:
        {
            if (agent.querySwap(board)) {
                cout << "Black and white are swapped." << endl;
                swap(agents[0], agents[1]);
			} else {
				cout << "Black and white are not swapped." << endl;
			}
            state = State::Opening; // 仍处于开局阶段
            break;
        }
        case State::NProposals:
        {
            auto proposals = agent.requestProposals(board, proposal_count);
            cout << "\nBlack's proposals: ";
            for (auto move : proposals) {
                cout << to_string(move) << " ";
            }
			cout << endl;
            auto [_, rival] = get_agent(Player::White);
            auto response = rival.respondProposals(board, std::move(proposals));
            board.applyMove(response);
            printf("\nWhite's response: %s\n\n", to_string(response).c_str());
            printf("%s\n", to_string(board).c_str());
            printf("Debug Messages: %s", agent.debugMessage().dump().c_str());
            printf("\n-------------------------\n");
            state = State::Ongoing; // 开局阶段结束
            break;
        }
        }
    }

    // 结果信息输出
    if (board.m_winner != Player::None) {
        auto [index, winner] = get_agent(board.m_winner);
        printf("\nGame end. Winner: %d.%s\n", index, winner.name().c_str());
    } else {
        printf("\nTie.\n");
    }

    // 按赢家的视角输出可被botzone读取的json数据
    json records;
    printf("Record JSON:\n");
    for (int i = 0; i < board.m_moveRecord.size(); ++i) {
        if (i == 0 && board.m_winner == Player::Black) {
            records["requests"].push_back(Position{ -1, -1 });
        }
        if ((i % 2 == 0) == (board.m_winner == Player::Black)) {
            records["responses"].push_back(board.m_moveRecord[i]);
        } else {
            records["requests"].push_back(board.m_moveRecord[i]);
        }
    }
    cout << records;

    return board.m_winner == Player::None;
}

inline int ConsoleInterface(Agent& agent0, Agent& agent1) {
    using namespace std;

    Board board;

    srand(time(nullptr));
    int black_player = rand() % 2;
    Agent* agents[2] = { &agent0, &agent1 };
    int index[2] = { !black_player, black_player };
    auto get_agent = [&](Player player) -> tuple<Agent&, int> {
        auto i = index[((int)player + 1) / 2];
        return { *agents[i], i };
    };

    cout << std::to_string(Player::Black) << ": " << index[1] << "." << agents[index[1]]->name().data() << endl;
    cout << std::to_string(Player::White) << ": " << index[0] << "." << agents[index[0]]->name().data() << endl;
    cout << endl << std::to_string(board) << "\n-------------------------\n";
    for (auto cur_player = Player::Black; ;) {
        auto[agent, index] = get_agent(cur_player);
        agent.syncWithBoard(board);
        auto move = agent.getAction(board);
        if (move == Position{ -1, -1 }) {
            board.revertMove(2);
            cout << std::to_string(board);
        } else if (auto result = board.applyMove(move); result != cur_player) {
            printf("\n%d.%s's move: %s:\n\n", index, agent.name().data(), std::to_string(move).c_str());
            cout << std::to_string(board);
            cout << "\nDebug Messages:" << agent.debugMessage();
            cout << "\n-------------------------\n";
            if (result == Player::None) {
                break;
            } else if (result == -cur_player) {
                cur_player = -cur_player;
                continue;
            }
        } else {
            cout << "Invalid move: " << std::to_string(move) << endl;
            continue;
        }
    }

    if (board.m_winner != Player::None) {
        auto[winner, idx] = get_agent(board.m_winner);
        printf("\nGame end. Winner: %d.%s\nRecord JSON:\n", idx, winner.name().data());
    } else {
        printf("\nTie.\n");
    }


    // 按赢家的视角输出可被botzone读取的json数据
    json records;
    for (int i = 0; i < board.m_moveRecord.size(); ++i) {
        if (i == 0 && board.m_winner == Player::Black) {
            records["requests"].push_back(Position{ -1, -1 });
        }
        if ((i % 2 == 0) == (board.m_winner == Player::Black)) {
            records["responses"].push_back(board.m_moveRecord[i]);
        } else {
            records["requests"].push_back(board.m_moveRecord[i]);
        }
    }
    cout << records;

    return board.m_winner == Player::None;
}

}


#endif // !GOMOKU_INTERFACE_H_
