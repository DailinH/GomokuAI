#ifndef GOMOKU_AGENT_H_
#define GOMOKU_AGENT_H_
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>
#include "Game.h"
#include "MCTS.h"
#include "Minimax.h"
#include "Pattern.h"
#include "algorithms/Heuristic.hpp"

namespace Gomoku {

using json = nlohmann::json;
inline void to_json(json& j, const Position& p) { j = { { "x", p.x() },{ "y", p.y() } }; }
inline void from_json(const json& j, Position& p) { p = Position{ j["x"], j["y"] }; }

namespace Interface {

class Agent {
public:
    virtual std::string name() = 0;

    virtual void syncWithBoard(Board& board) { };

    virtual Position getAction(Board& board) = 0;

    virtual int queryProposalCount() { return 2; }

    virtual bool querySwap(Board& board) { return false; }

	virtual std::vector<Position> requestProposals(Board& board, int n) { return std::vector<Position>(Position::npos, n); };

	virtual Position respondProposals(Board& board, std::vector<Position> proposal) { return Position::npos; };

    virtual json debugMessage() { return json(); };

    virtual void reset() { }
};


class HumanAgent : public Agent {
public:
    using Heuristic = Algorithms::Heuristic;

    HumanAgent(bool output_probs = false) : c_output(output_probs) { }

    virtual std::string name() {
        return "HumanAgent";
    }

	virtual void syncWithBoard(Board& board) { 
		m_evaluator.syncWithBoard(board);
	};

    virtual Position getAction(Board& board) {
        std::cout << "\nInput your move({-1 -1} to revert): ";
        std::cin >> std::hex >> m_move.first >> m_move.second;
        return m_move;
    }

	virtual int queryProposalCount() {
		int count;
		std::cout << "\nInput proposal count: ";
		std::cin >> count;
		return count; 
	}

	virtual bool querySwap(Board& board) {
		char do_swap;
		std::cout << "\nSwap?(y/n): ";
		std::cin >> do_swap;
		switch (do_swap) {
		case 'Y': case 'y':
			return true;
		case 'N': case 'n':
			return false;
		default:
			return false;
		}
	}

	virtual std::vector<Position> requestProposals(Board& board, int n) {
		std::cout << "\nInput " << n << " proposals:\n";
		std::vector<Position> proposals;
		for (int i = 0; i < n; ++i) {
			int x, y;
			std::cout << i + 1 << ": ";
			std::cin >> x >> y;
			proposals.emplace_back(x, y);
		}
		return proposals;
	};

	virtual Position respondProposals(Board& board, std::vector<Position> proposals) {
		Position choice;
		while (std::find(proposals.begin(), proposals.end(), choice) == proposals.end()) {
			int x, y;
			std::cout << "\nYour choice: ";
			std::cin >> x >> y;
			choice = Position(x, y);
		}
		return choice;
	};

    virtual json debugMessage() {
        if (c_output) {
            auto reshaped_probs = [this] {
                return Eigen::Map<const Eigen::Array<float, 15, 15, Eigen::RowMajor>>(m_probs.data());
            };

            m_probs = Heuristic::EvaluationProbs(m_evaluator, m_evaluator.board().m_curPlayer);
            Heuristic::DecisiveFilter(m_evaluator, m_probs);
            std::cout << "before:\n" << reshaped_probs() << std::endl;

            m_evaluator.applyMove(m_move);

            m_probs = Heuristic::EvaluationProbs(m_evaluator, m_evaluator.board().m_curPlayer);
            Heuristic::DecisiveFilter(m_evaluator, m_probs);
            std::cout << "after:\n" << reshaped_probs() << std::endl;
        }
		json message;
		message["before"] = patternMessage();
		m_evaluator.applyMove(m_move);
		message["current"] = patternMessage();
		return message;
    }

	json patternMessage() {
		json message;
		for (auto player : { Player::Black, Player::White })
			for (int i = 0; i < Pattern::Size - 1; ++i) {
				message[std::to_string(player)][0][i] = m_evaluator.m_patternDist.back()[i].get(player);
			}
		for (auto player : { Player::Black, Player::White })
			for (int i = 0; i < Compound::Size; ++i) {
				message[std::to_string(player)][1][i] = m_evaluator.m_compoundDist.back()[i].get(player);
			}
		return message;
	}

    Evaluator m_evaluator;
    std::pair<int, int> m_move;
    Eigen::VectorXf m_probs;
    bool c_output;
};


class RandomAgent : public Agent {
public:
    virtual std::string name() {
        return "RandomAgent";
    }

    virtual Position getAction(Board& board) {
        return board.getRandomMove();
    }
};


class MCTSAgent : public Agent {
public:
    MCTSAgent(milliseconds durations, Policy* policy) : c_duration(durations), m_policy(policy) { }

    virtual std::string name() {
        using namespace std::chrono;
        return "MCTSAgent:" + std::to_string(c_duration.count()) + "ms";
    }

	auto popCandidates(Board& board) {
		auto[state_value, action_probs] = m_mcts->evalState(board);
		Eigen::Map<const Eigen::Array<float, 15, 15, Eigen::RowMajor>> probs_2d(action_probs.data());
		//std::cout << state_value << std::endl;
		std::cout << probs_2d << std::endl;
		std::vector<Position> candidates(BOARD_SIZE);
		std::iota(candidates.begin(), candidates.end(), 0);
		std::sort(candidates.begin(), candidates.end(), [&](auto lhs, auto rhs) {
			return action_probs[lhs] > action_probs[rhs];
		});
		for (int i = 0; i < 10; ++i) {
			std::cout << std::to_string(candidates[i]) << " ";
		}
		return candidates;
	}

    virtual Position getAction(Board& board) {
		auto candidates = popCandidates(board);
		return candidates.front();
    }

	virtual bool querySwap(Board& board) { 
		return -m_mcts->m_root->state_value < 0;
	}

	virtual std::vector<Position> requestProposals(Board& board, int n) {
		auto candidates = popCandidates(board);
		return std::vector<Position>(candidates.begin(), candidates.begin() + n);
	};

	virtual Position respondProposals(Board& board, std::vector<Position> proposals) { 
		auto [state_value, action_probs] = m_mcts->evalState(board);
		return *std::min_element(proposals.begin(), proposals.end(), [&](auto lhs, auto rhs) {
			return action_probs[lhs] < action_probs[rhs];
		});
	};

    virtual json debugMessage() {
        return {
            { "iterations", m_mcts->m_iterations },
            { "duration",   std::to_string(m_mcts->m_duration.count()) + "ms" }
        };
    };

    virtual void syncWithBoard(Board& board) {
        if (m_mcts == nullptr) {
            auto last_action = board.m_moveRecord.empty() ? Position::npos : board.m_moveRecord.back();
            m_mcts = std::make_unique<MCTS>(c_duration, last_action, -board.m_curPlayer, m_policy);
        } else {
            m_mcts->syncWithBoard(board);
        }
    };

    virtual void reset() {
        m_mcts->reset();
    }

protected:
    std::unique_ptr<MCTS> m_mcts;
    std::shared_ptr<Policy> m_policy;
    std::chrono::milliseconds c_duration;
};


class PatternEvalAgent : public Agent {
public:
    using Heuristic = Algorithms::Heuristic;

    virtual std::string name() {
        return "PatternEvalAgent";
    }

    virtual Position getAction(Board& board) {
        if (board.m_moveRecord.empty()) {
            m_thisMove = { WIDTH / 2, HEIGHT / 2 };
        } else {
            auto action_probs = Heuristic::EvaluationProbs(m_evaluator, m_evaluator.board().m_curPlayer);
            Heuristic::DecisiveFilter(m_evaluator, action_probs);
            Heuristic::EvaluationValue(m_evaluator, m_evaluator.board().m_curPlayer);
            action_probs.maxCoeff(&m_thisMove.id);
            //m_thisMove = board.getRandomMove(action_probs);
        }
        return m_thisMove;
    }

    virtual json debugMessage() {
        json message;
        message["before"] = patternMessage();
        m_evaluator.applyMove(m_thisMove);
        message["current"] = patternMessage();
        return message;
    };

    virtual void syncWithBoard(Board& board) {
        m_evaluator.syncWithBoard(board);
    };

    virtual void reset() {
        m_evaluator.reset();
    }

    json patternMessage() {
        json message;
        for (auto player : { Player::Black, Player::White })
        for (int i = 0; i < Pattern::Size - 1; ++i) {
            message[std::to_string(player)][0][i] = m_evaluator.m_patternDist.back()[i].get(player);
        }
        for (auto player : { Player::Black, Player::White })
        for (int i = 0; i < Compound::Size; ++i) {
            message[std::to_string(player)][1][i] = m_evaluator.m_compoundDist.back()[i].get(player);
        }
        return message;
    }

private:
    Evaluator m_evaluator;
    Position m_thisMove;
};


class MinimaxAgent : public Agent {
public:
	MinimaxAgent(int depth) : m_minimax(depth) {
        c_depth = depth;
	}

    virtual std::string name() {
        return "MinimaxAgent: Depth="+std::to_string(c_depth);
    }
     
    virtual Position getAction(Board& board) { 
        return m_minimax.getAction(board);
    }

 //   virtual json debugMessage() {
	//	return { "depth", m_minimax->m_depth };
	//};

    virtual void syncWithBoard(Board& board) {
        m_minimax.syncWithBoard(board);
    };

    virtual void reset() {//valid
        //m_minimax->reset();
	};

private:
    Minimax m_minimax;
    int c_depth;
};


}

}

#endif // !GOMOKU_AGENT_H_