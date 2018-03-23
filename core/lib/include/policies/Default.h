#ifndef GOMOKU_POLICIES_DEFAULT_H_
#define GOMOKU_POLICIES_DEFAULT_H_
#include "MCTS.h"
#include <tuple>
#include <algorithm>

namespace Gomoku::Policies {

// Default��һ�龲̬�����ļ��ϣ������̳�Policy��
// Ϊ��ʹDefault��ʹ�õ�Policy��Container��ÿ������������Policyָ�롣
// C++20��Unified call syntax����������һ�и���ֱ�ۡ�
struct Default {
    
    // �ۺϿ��ǵ�ǰ�����ֵ����ǰAction���ʵ�UCB1��ʽ��
    static double UCB1(const Node* node, double c_puct) {
        const double Q_i = node->state_value;
        const double P_i = node->action_prob;
        const double N = node->parent->node_visits;
        const double n_i = node->node_visits + 1;
        return Q_i + c_puct * P_i * sqrt(log(N) / n_i);
    }

    static Node* select(Policy* policy, const Node* node, double c_puct) {
        //return max_element(node->children.begin(), node->children.end(), [](auto&& lhs, auto&& rhs) {
        //    return lhs->UCB1(sqrt(2)) < rhs->UCB1(sqrt(2));
        //})->get();
        int index = 0;
        double max = 0.0;
        for (int i = 0; i < node->children.size(); ++i) {
            auto cur = UCB1(node->children[i].get(), c_puct);
            if (cur > max) {
                index = i;
                max = cur;
            }
        }
        return node->children[index].get();
    }

    // ���ݴ���ĸ������Ž�㡣����Ϊ0��Action�����������ӽ���С�
    static std::size_t expand(Policy* policy, Node* node, const std::vector<double> action_probs) {
        node->children.reserve(action_probs.size());
        for (int i = 0; i < GameConfig::BOARD_SIZE; ++i) {
            if (action_probs[i] != 0.0) {
                node->children.emplace_back(new Node{ node, {}, i, -node->player, 0, 0.0, action_probs[i] });
            }
        }
        return node->children.size();
    }

    // �������ֱ����Ϸ����
    static std::tuple<double, std::vector<double>> simulate(Policy* policy, Board& board) {
        using Actions = std::vector<Position>;
        if (!policy->container.has_value()) {
            policy->container.emplace<Actions>();
        }   
        auto&& move_stack = std::any_cast<Actions>(policy->container);
        const auto node_player = board.m_curPlayer;

        for (auto result = node_player; result != Player::None; ) {
            auto move = board.getRandomMove();
            result = board.applyMove(move);
            move_stack.push_back(move);
        }

        const auto winner = board.m_winner;
        // ����������MCTS��ǰ���ڵ�״̬��ע��Ӯ�һᱻ������ΪPlayer::None��
        while (!move_stack.empty()) {
            board.revertMove(move_stack.back());
            move_stack.pop_back();
        }

        const auto score = getFinalScore(node_player, winner);
        auto action_probs = std::vector<double>(BOARD_SIZE);
        for (int i = 0; i < BOARD_SIZE; ++i) {
            action_probs[i] = board.moveStates(Player::None)[i] ? 1.0 / board.moveCounts(Player::None) : 0.0;
        }

        return std::make_tuple(score, std::move(action_probs));
    }

    static void backPropogate(Policy* policy, Node* node, Board& board, double value) {
        for (; node != nullptr; node = node->parent, value = -value) {
            node->node_visits += 1;
            node->state_value += (value - node->state_value) / node->node_visits;
            if (node->parent != nullptr) {
                board.revertMove(node->position);
            }
        }
    }
};

}

#endif // !GOMOKU_POLICIES_H_
