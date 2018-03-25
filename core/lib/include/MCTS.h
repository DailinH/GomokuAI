#ifndef GOMOKU_MCTS_H_
#define GOMOKU_MCTS_H_
#include "Game.h" // Gomoku::Player, Gomoku::Position, Gomoku::Board
#include <vector> // std::vector
#include <memory> // std::unique_ptr
#include <functional> // std::function
#include <any> // std::any

namespace Gomoku {

struct Node {
    /* ���ṹ���� */
    Node* parent = nullptr;
    std::vector<std::unique_ptr<Node>> children = {};

    /* ����״̬���� */
    Position position;
    Player player;

    /* ����ֵ���� */
    std::size_t node_visits = 0;
    double state_value = 0.0;     // ��ʾ�˽���Ӧ�ĵ�ǰ���������
    double action_prob = 0.0;     // ��ʾ������Ӧ�����£�ѡ��ö����ĸ���

    /* �����жϺ��� */
    bool isLeaf() const { return children.empty(); }
    bool isFull(const Board& board) const { return children.size() == board.moveCounts(Player::None); }
};


class Policy {
public:
    /*
        �� simple wrapper of uct
    */
    using SelectFunc = std::function<Node*(const Node*, double)>;

    /*
        �� return self
    */
    using ExpandFunc = std::function<std::size_t(Node*, std::vector<double>)>;

    /*
        �� Board State Invariant
    */
    using EvaluateFunc = std::function<std::tuple<double, std::vector<double>>(Board&)>;

    /*
        �� Board State Reset (Symmetry with select)
    */
    using UpdateFunc = std::function<void(Node*, Board&, double)>;

    // ������ĳһ���nullptrʱ�����ʹ��һ��Ĭ�ϲ��Գ�ʼ����
    Policy(SelectFunc = nullptr, ExpandFunc = nullptr, EvaluateFunc = nullptr, UpdateFunc = nullptr);

public:
    SelectFunc   select;
    ExpandFunc   expand;
    EvaluateFunc simulate;
    UpdateFunc   backPropogate;

    // ���Դ��Actions�������������ڲ�ͬ��Policy�б���Ϊ�κ���ʽ��
    std::any container;
};


class MCTS {
public:
    MCTS(
        Position last_move = -1,
        Player last_player = Player::White,
        Policy* policy = nullptr,
        std::size_t c_iterations = 20000,
        double c_puct = sqrt(2)
    );

    Position getNextMove(Board& board);

    // get root value
    // get root probabilities

    // ���ؿ�������һ�ֵ���
    void playout(Board& board);

    // ѡ������֣�ʹ���ؿ�����������һ��
    Node* stepForward(); 

    // �����ṩ�Ķ���������
    Node* stepForward(Position); 

public:
    std::unique_ptr<Node> m_root;
    std::unique_ptr<Policy> m_policy;
    std::size_t m_size;
    std::size_t c_iterations;
    double      c_puct;
};

}

#endif // !GOMOKU_MCTS_H_