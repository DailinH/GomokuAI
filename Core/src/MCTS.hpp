#pragma once
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <stack>
#include <memory>
#include "Game.hpp"

namespace Gomoku {

using namespace std;

struct Node {
    Node* parent = nullptr;
    vector<unique_ptr<Node>> children = {};
    Position position = -1;
    size_t visits = 0;
    float victory_value = 0.0;

    bool isLeaf() {
        return children.empty();
    }

    float UCB1(float expl_param) {
        return 1.0f * visits / victory_value + expl_param * sqrt(logf(parent->visits) / this->visits);
    }
};

class MCTS {
public:
    MCTS(Player player) : m_root(new Node{}), m_player(player) {
        srand(time(nullptr));
    }

    Position getNextMove(Board& board) {
        for (int i = 0; i < 10000; ++i) {
            playout(board); // ���һ�����ؿ�������ѭ��
        }
        stepForward(); // �������ؿ���������ѡ����������ƽ�һ��
        return m_root->position; // ����ѡ�������һ��
    }

private:
    // ���ؿ�������һ��ѭ��
    void playout(Board& board) {
        Node* node = m_root.get(); // ��ָ�������۲�ָ�룬���������ӵ������Ȩ
        while (!node->isLeaf()) {
            node = select(node);
            board.applyMove(node->position);
            m_moveStack.push(node->position);
        }
        float leaf_value;
        if (!board.status().end) {
            node = expand(node, board);
            leaf_value = simulate(node, board); // ����ģ��սԤ����ǰ����ֵ����
        } else {
            leaf_value = getFinalScore(m_player, board.m_winner); // ���ݶԾֽ����ȡ���Լ�ֵ����
        }
        backPropogate(node, leaf_value);
        reset(board); // ����Board����ʼ�����״̬
    }
    
    // AlphaZero�������У���MCTS�������ò���
    // �μ�https://stackoverflow.com/questions/47389700/why-does-monte-carlo-tree-search-reset-tree
    void stepForward() {
        // ѡ����õ�һ��
        auto&& nextNode = std::move(*max_element(m_root->children.begin(), m_root->children.end(), [](auto&& lhs, auto&& rhs) {
            return lhs->visits < rhs->visits;
        }));
        // �������ؿ������������ڵ��ƽ���ѡ���������ֵĶ�Ӧ���
        // ���º�ԭ���ڵ���unique_ptr�Զ��ͷţ�����ķ��������Ҳ�ᱻ��ʽ�Զ����١�
        m_root = std::move(nextNode);
        m_root->parent = nullptr;
    }

private:
    Node* select(const Node* node) const {
        return max_element(node->children.begin(), node->children.end(), [](auto&& lhs, auto&& rhs) {
            return lhs->UCB1(sqrtf(2)) < rhs->UCB1(sqrtf(2));
        })->get();
    }

    Node* expand(Node* node, Board& board) {
        // ���ѡȡһ��Ԫ����Ϊ�½��
        auto newPos = board.getRandomMove();
        node->children.emplace_back(new Node{ node, {}, newPos });
        return node->children.back().get(); // ���������Ľ��
    }

    float simulate(Node* node, Board& board) {
        while (true) {
            auto move = board.getRandomMove();
            auto result = board.applyMove(move);
            m_moveStack.push(move);
            if (result == Player::None) {
                return getFinalScore(m_player, board.m_winner);
            }
        }
    }

    void backPropogate(Node* node, float value) {
        do {
            node->visits += 1;
            node->victory_value += value >= 0 ? value : 0;
            node = node->parent;
            value = -value;
        } while (node != nullptr);
    }

    // TODO: ͳһ���õĹ���Ŀǰ��ջ��parent���������ַ�ʽ������
    void reset(Board& board) {
        while (!m_moveStack.empty()) {
            board.revertMove(m_moveStack.top());
            m_moveStack.pop();
        }
    }

private:
    unique_ptr<Node> m_root;
    Player m_player;
    stack<Position> m_moveStack;
};

}