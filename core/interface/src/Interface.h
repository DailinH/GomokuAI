#ifndef GOMOKU_INTERFACE_H_
#define GOMOKU_INTERFACE_H_
#include <iostream>
#include <fstream>
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

inline std::string get_name(std::string info_type, std::string default_info = "") {
    char flag;
    std::string info;
    while (true) {
        std::cout << "������-" << info_type << ":";
        if (default_info != "") {
            std::cout << default_info << std::endl;

            return default_info;

        }

        std::cin >> info;

        std::cout << "ȷ��?/y :";

        std::cin >> flag;

        if (flag == 'y' || flag == 'Y')

            return info;

    }

}

// ��shuffleΪfalse����agent0���С�
inline int NewConsoleInterface(Agent& agent0, Agent& agent1, bool RIFRule = false, bool shuffle = true, bool debug = true) {
    using namespace std;

        /////////////////////

	string first_player = get_name("����"),

		second_player = get_name("����");

	char start_time[300];

	string header_info;

	string game = "C5";

	string location = "2017 CCGC";

    time_t timep;

    time (&timep);

    strftime(start_time, sizeof(start_time), "%Y.%m.%d %H:%M ����",localtime(&timep) );

	string title = game + "-" + first_player + "-" + second_player + "-WINNER.txt";

	header_info = "[" + game + "][" + first_player + "][" + second_player + "][" + "WINNER" + "][" + start_time + "][" + location+"]";

    //sprintf(title,"%s-%s-%s-%s-%s.txt",game,first_player,second_player,"WINNER",start_time );

    //sprintf(header_info,"[%s][%s][%s][%s][%s][%s]",game,first_player,second_player,"WINNER",start_time ,location);

	//header_info = 

	ofstream game_records(title);

	game_records << "{" << header_info;

    ////////////////

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

    // ���Agent���
    for (auto player : { Player::Black, Player::White }) {
        auto [index, agent] = get_agent(player);
        printf("%s: agent%d.%s\n", to_string(player).c_str(), index, agent.name().c_str());
    }
    // �����ʼ����
    cout << endl << to_string(board);
    cout << "\n-------------------------\n";

    // ״̬����ʽ����
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
                    player = Player::White; // ���ɰ׷��µ�4����
                    state = State::Swap; // �л�Ϊ���ֽ���״̬
                    continue; // ��һ��״̬�����ɼٰ׷�Agent��Ӧ
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
        } // ����ᴩ͸
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
                game_records << (player == Player::Black ? ";B(" : ";W(") << move.x() << "," << move.y() << ")";
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
            state = State::Opening; // �Դ��ڿ��ֽ׶�
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
            player = board.applyMove(response);
            printf("\nWhite's response: %s\n\n", to_string(response).c_str());
            printf("%s\n", to_string(board).c_str());
            printf("Debug Messages: %s", agent.debugMessage().dump().c_str());
            printf("\n-------------------------\n");
            state = State::Ongoing; // ���ֽ׶ν���
            break;
        }
        }
    }

    // �����Ϣ���
    if (board.m_winner != Player::None) {
        auto [index, winner] = get_agent(board.m_winner);
        printf("\nGame end. Winner: %d.%s\n", index, winner.name().c_str());
    } else {
        printf("\nTie.\n");
    }

    // ��Ӯ�ҵ��ӽ�����ɱ�botzone��ȡ��json����
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
    game_records << "}" << endl;
    game_records.close();

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


    // ��Ӯ�ҵ��ӽ�����ɱ�botzone��ȡ��json����
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
