#include "Pattern.h"
#include <numeric>
#include <queue>
#include <set>

using namespace std;
using Eigen::Matrix;

namespace Gomoku {

constexpr Direction Directions[] = {
    Direction::Vertical, Direction::Horizontal, Direction::LeftDiag, Direction::RightDiag
};

/* ------------------- Pattern��ʵ�� ------------------- */

constexpr int Codeset[] = { 1, 2, 3, 4 };

constexpr int EncodeCharset(char ch) {
    switch (int code = 0; ch) {
        case '-': case '_': case '^': case '~': ++code;
        case '?': ++code;
        case 'o': ++code;
        case 'x': ++code;
        default: return code;
    }
}

Pattern::Pattern(std::string_view proto, Type type, int score) 
    : str(proto.begin() + 1, proto.end()), score(score), type(type),
      belonging(proto[0] == '+' ? Player::Black : Player::White) {

}

/* ------------------- PatternSearch��ʵ�� ------------------- */

class AhoCorasickBuilder {
public:
    // ���ڹ���˫�������ʱ���
    struct Node {
        int code = 0, depth = 0, first = 0; mutable int last = first + 1; // Ĭ�Ϲ���ʱ���ɸ��ڵ�
        bool operator<(Node other) const { // ����depth->first�ֵ���Ƚϡ�last������Ƚϣ��ʿ�����Ϊmutable��
            return std::tie(depth, first) < std::tie(other.depth, other.first); 
        }
    };

    AhoCorasickBuilder(initializer_list<Pattern> protos) 
        : m_tree{ Node{} }, m_patterns(protos) {

    }

    void build(PatternSearch* searcher) {
        this->reverseAugment()->flipAugment()->boundaryAugment()->sortPatterns()
            ->buildNodeBasedTrie()->buildDAT(searcher)->buildACGraph(searcher);
    }

private:
    // ���ԳƵ�pattern����������ԭpattern�ȼ�
    AhoCorasickBuilder* reverseAugment() {
        for (int i = 0, size = m_patterns.size(); i < size; ++i) {
            Pattern reversed(m_patterns[i]);
            std::reverse(reversed.str.begin(), reversed.str.end());
            if (reversed.str != m_patterns[i].str) {
                m_patterns.push_back(std::move(reversed));
            }
        }
        return this;
    }

    // pattern�ĺڰ���ɫ�Ե�����ԭpattern�ȼ�
    AhoCorasickBuilder* flipAugment() {
        for (int i = 0, size = m_patterns.size(); i < size; ++i) {
            Pattern flipped(m_patterns[i]);
            flipped.belonging = -flipped.belonging;
            for (auto& piece : flipped.str) {
                if (piece == 'x') piece = 'o';
                else if (piece == 'o') piece = 'x';
            }
            m_patterns.push_back(std::move(flipped));
        }
        return this;
    }

    // ���߽��ס��pattern�뱻���ֶ�סһ�ǵ�pattern�ȼ�
    AhoCorasickBuilder* boundaryAugment() {
        for (int i = 0, size = m_patterns.size(); i < size; ++i) {
            char enemy = m_patterns[i].belonging == Player::Black ? 'x' : 'o';
            auto first = m_patterns[i].str.find_first_of(enemy);
            auto last  = m_patterns[i].str.find_last_of(enemy);
            if (first != string::npos) { // ���ֻ���������
                Pattern bounded(m_patterns[i]);
                bounded.str[first] = '?';
                m_patterns.push_back(std::move(bounded));
                if (last != first) {
                    bounded.str[last] = '?';
                    m_patterns.push_back(std::move(bounded));
                    bounded.str[first] = enemy;
                    m_patterns.push_back(std::move(bounded));
                }
            }
        }
        return this;
    }

    // ���ݱ����ֵ�������patterns
    AhoCorasickBuilder* sortPatterns() {
        // �������յ�patterns����Ԥ���ռ�
        vector<int> codes(m_patterns.size());
        vector<int> indices(m_patterns.size());
        
        // ���롢���������
        std::transform(m_patterns.begin(), m_patterns.end(), codes.begin(), [](const Pattern& p) {
            return std::reduce(p.str.begin(), p.str.end(), 0, [](int sum, char ch) { 
                return sum *= std::size(Codeset), sum += EncodeCharset(ch); 
            }); // ͨ������������ʵ�ֶ�patterns���ֵ�������
        }); 
        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(), [&codes](int lhs, int rhs) { 
            return codes[lhs] < codes[rhs]; 
        });

        // ����indices����m_patterns
        vector<Pattern> medium;
        medium.reserve(m_patterns.size());
        for (auto index : indices) {
            medium.push_back(std::move(m_patterns[index]));
        }
        m_patterns.swap(medium);
        return this;
    }

    // DFS�������ɻ��ڽ���Trie��
    AhoCorasickBuilder* buildNodeBasedTrie() {
        using NodeIter = decltype(m_tree)::iterator;
        // suffixΪ��parent���ӽ��Ϊ��㣬ֱ������pattern�����ĺ�׺
        function<void(string_view, NodeIter)> insert_pattern = [&](string_view suffix, NodeIter parent) {
            if (!suffix.empty()) {
                Node key = { // ׼���ӽ������
                    EncodeCharset(suffix[0]), 
                    parent->depth + 1, 
                    parent->first, parent->last 
                }; 
                auto child = m_tree.find(key); // ����ƥ���ǰ׺���
                if (child == m_tree.end()) { // ��ǰ׺ƥ��ʧ�ܣ���������һ����֧���
                    // ���ڲ���ʱ�Ѱ��ֵ������򣬹��������first���ϸ������last��������
                    key.first = parent->last;
                    key.last = key.first + 1;
                    child = m_tree.insert(key).first;
                }
                insert_pattern(suffix.substr(1), child); // �ݹ齫patternʣ�µĲ��ֲ���trie����
                parent->last = child->last; // ���򴫲������������ұ߽�ֵ������last��Ӱ��Node�����򣬹��޸��ǿ��еġ�
            }
        };
        auto root = m_tree.find({});
        for (auto pattern : m_patterns) {
            insert_pattern(pattern.str, root); // ���ֵ������pattern
        }
        return this;
    }

    // DFS��������˫����Trie��
    AhoCorasickBuilder* buildDAT(PatternSearch* ps) {
        using NodeIter = decltype(m_tree)::iterator;
        // indexΪnode��˫�����е�������֮ǰ�ĵݹ�����ȷ����
        function<void(int, NodeIter)> build_recursive = [&](int index, NodeIter node) {
            if (node->last - node->first <= 1) {
                // Base case: �ѵִ�Ҷ��㣬����index��baseΪ(-��Ӧpattern����±�)
                ps->m_base[index] = -node->first;
            } else {
                // ׼���ӽڵ㣨����[first, last)����������򶨣���begin����
                auto first = m_tree.lower_bound({ 0, node->depth + 1, node->first }); // �ӽڵ��½磨no less than��
                auto last = m_tree.upper_bound({ 0, node->depth + 1, node->last - 1 }); // �ӽڵ��Ͻ磨greater than��
                int begin; // ��Ѱ�ҵĵ�ǰ����baseֵ

                /*
                    ��ʼ������begin�Ӹ��ڵ�0��ʼ
                    �˳��������ҵ�һ����������index���������ӽ���beginֵ
                    ״̬ת�ƣ���ǰ�������ҵ���һ����λ
                    ע�⣺���ڵ�����û�и���㣬�����ޱ���checkֵ����λ��������Ϊǰ���������㡣
                    �ο���http://www.aclweb.org/anthology/D13-1023  
                */
                for (begin = 0; ; begin = -ps->m_check[begin]) {
                    // �ռ䲻��ʱ����m_base��m_check���顣��ֵ��Ϊsize-1�Ա�֤���һλΪ��
                    if (begin + std::size(Codeset) >= ps->m_check.size() - 1) {
                        // ����ռ�
                        auto pre_size = ps->m_base.size();
                        ps->m_base.resize(2*pre_size + 1);
                        ps->m_check.resize(2*pre_size + 1);
                        // ����±겹ȫ˫����
                        for (int i = pre_size; i < ps->m_base.size(); ++i) {
                            ps->m_base[i] = -(i-1); // ��������
                            ps->m_check[i] = -(i+1); // ǰ������
                        }
                    }

                    if (std::all_of(first, last, [&](const Node& node) {
                        return ps->m_check[begin + node.code] <= 0;
                    })) break;
                }

                // �����ӽ�㣬�������״̬����ӽ��ݹ鹹��
                for (auto cur = first; cur != last; ++cur) {
                    // ȡ�õ�ǰ�ӽڵ��±�
                    int c_i = begin + cur->code;

                    // ����ǰ�±��Ƴ����нڵ㣨����Dancing Links��
                    ps->m_check[-ps->m_base[c_i]] = ps->m_check[c_i];
                    ps->m_base[-ps->m_check[c_i]] = ps->m_base[c_i];

                    // ���ӽ��checkֵ�븸���󶨣����ӽڵ�ݹ�
                    ps->m_check[c_i] = index;
                    build_recursive(c_i, cur);
                }
                ps->m_base[index] = begin;
            }
        };
        auto root = m_tree.find({});
        build_recursive(0, root);
        ps->m_patterns.swap(m_patterns);
        return this;
    }

    // BFS������ΪDAT����AC�Զ�����failָ������
    AhoCorasickBuilder* buildACGraph(PatternSearch* ps) {
        // ��ʼ�����н���failָ�붼ָ����ڵ�
        ps->m_fail.resize(ps->m_base.size(), 0);

        // ׼���ý����У�������ڵ���Ϊ��ʼֵ
        queue<int> node_queue;
        node_queue.push(0); 
        while (!node_queue.empty()) {
            // ȡ����ǰ��� 
            int cur_node = node_queue.front();
            node_queue.pop();

            // ׼���µĽ��
            for (auto code : Codeset) {
                int child_node = ps->m_base[cur_node] + code;
                if (ps->m_check[child_node] == cur_node) {
                    node_queue.push(child_node);
                }
            }

            // Ϊ��ǰ�������failָ��
            int code = cur_node - ps->m_base[ps->m_check[cur_node]]; // ȡ��ת����cur���ı���
            int pre_fail_node = ps->m_check[cur_node]; // ��ʼpre_fail�������Ϊcur���ĸ����
            while (pre_fail_node != 0) { // ��ƥ���׺���ȴӳ�->�̲�����תfail��㣬ֱ������Ϊ0���ִ���ڵ㣩
                // ÿһ��failָ�����ת�����ƥ���׺�ĳ������ټ�����1�����ѭ�������޵�
                pre_fail_node = ps->m_fail[pre_fail_node]; 
                int fail_node = ps->m_base[pre_fail_node] + code;
                if (ps->m_check[fail_node] == pre_fail_node) { // ����pre_fail�����ͨ��code�ִ�ĳ�ӽ�㣨��fail_node���ڣ�
                    ps->m_fail[cur_node] = fail_node; // ����ӽ�㼴Ϊcur����failָ���ָ��
                    break;
                }
                // ��ֱ��pre_fail���Ϊ0���˳�����ǰ����failָ��ָ����ڵ㡣
            }
            // NOTE: ����Patterns�����Ϊһ��û�л�Ϊǰ׺����������������"output��"����m_patterns���������䡣
        }
        return this;
    }

private:
    set<Node> m_tree; // �����ڲ����б��������RB������Ϊ��ʱ����Trie���Ľṹ
    vector<Pattern> m_patterns;
};

PatternSearch::PatternSearch(initializer_list<Pattern> protos) {
    AhoCorasickBuilder builder(protos);
    builder.build(this);
}

// ����ת�ƣ�ֱ��ƥ����һ���ɹ���ģʽ���ѯ����
const PatternSearch::generator& PatternSearch::generator::operator++() {
    while (ref->m_base[state] >= 0 && !target.empty()) {
        int next = ref->m_base[state] + EncodeCharset(target[0]);
        if (ref->m_check[next] == state) { // ��������ƥ��target
            state = next; // ƥ��ɹ�ת��
        } else {
            state = ref->m_fail[state]; // ƥ��ʧ��ת��
        }
        ++offset, target.remove_prefix(1);
    }
    return *this;
}

// ������ǰ�ɹ�ƥ�䵽��ģʽ
PatternSearch::Record PatternSearch::generator::operator*() const {
    return { ref->m_patterns[-ref->m_base[state]], offset };
}

PatternSearch::generator PatternSearch::execute(string_view target) {
    return generator{ target, this };
}

vector<PatternSearch::Record> PatternSearch::matches(string_view target) {
    vector<Record> records;
    for (auto record : execute(target)) { 
        records.push_back(record); 
    }
    return records;
}

/* ------------------- BoardMap��ʵ�� ------------------- */

constexpr tuple<int, int> ParseIndex(Position pose, Direction direction) {
    // ����ǰ���MAX_PATTERN_LEN - 1λ��Ϊ'?'��Խ��λ�����������ʼoffset
    switch (int offset = MAX_PATTERN_LEN - 1; direction) {
        case Direction::Horizontal: // 0 + x��[0, WIDTH) | y: 0 -> HEIGHT
            return { pose.x(), offset + pose.y() };
        case Direction::Vertical:   // WIDTH + y��[0, HEIGHT) | x: 0 -> WIDTH
            return { WIDTH + pose.y(), offset + pose.x() };
        case Direction::LeftDiag:   // (WIDTH + HEIGHT) + (HEIGHT - 1) + x-y��[-(HEIGHT - 1), WIDTH) | min(x, y): (x, y) -> (x+1, y+1)
            return { WIDTH + 2 * HEIGHT - 1 + pose.x() - pose.y(), offset + std::min(pose.x(), pose.y()) };
        case Direction::RightDiag:  // 2*(WIDTH + HEIGHT) - 1 + x+y��[0, WIDTH + HEIGHT - 1) | min(WIDTH - x, y): (x, y) -> (x-1, y+1)
            return { 2 * (WIDTH + HEIGHT) - 1 + pose.x() + pose.y(), offset + std::min(WIDTH - pose.x(), pose.y()) };
        default:
            throw direction;
    }
}

BoardMap::BoardMap(Board* board) : m_board(board ? board : new Board) {
    this->reset();
}

string_view BoardMap::lineView(Position pose, Direction direction) {
    const auto [index, offset] = ParseIndex(pose, direction);
    return string_view(&m_lineMap[index][offset - TARGET_LEN / 2],  TARGET_LEN);
}

Player BoardMap::applyMove(Position move) {
    for (auto direction : Directions) {
        const auto [index, offset] = ParseIndex(move, direction);
        m_lineMap[index][offset] = m_board->m_curPlayer == Player::Black ? 'x' : 'o';
    }
    return m_board->applyMove(move, false);
}

Player BoardMap::revertMove(size_t count) {
    for (int i = 0; i < count; ++i) {
        for (auto direction : Directions) {
            const auto [index, offset] = ParseIndex(m_board->m_moveRecord.back(), direction);
            m_lineMap[index][offset] = '-';
        }
        m_board->revertMove();
    }
    return m_board->m_curPlayer;
}

void BoardMap::reset() {
    m_hash = 0ul;
    m_board->reset();
    for (auto& line : m_lineMap) {
        line.resize(MAX_PATTERN_LEN - 1, '?'); // ��ǰ���Խ��λ('?')
    }
    for (auto i = 0; i < BOARD_SIZE; ++i) 
    for (auto direction : Directions) {
        auto [index, _] = ParseIndex(i, direction);
        m_lineMap[index].push_back('-'); // ��ÿ��λ������λ('-')
    }
    for (auto& line : m_lineMap) {
        line.append(MAX_PATTERN_LEN - 1, '?'); // �ߺ����Խ��λ('?')
    }
}

/* ------------------- Evaluator��ʵ�� ------------------- */

template <size_t Length = TARGET_LEN, typename Array_t>
inline auto& LineView(Array_t& src, Position move, Direction dir) {
    static vector<Array_t::pointer> view_ptrs(Length);
    auto [dx, dy] = *dir;
    for (int i = 0, j = i - Length / 2; i < Length; ++i, ++j) {
        auto x = move.x() + dx * j, y = move.y() + dy * j;
        if ((x >= 0 && x < WIDTH) && (y >= 0 && y < HEIGHT)) {
            view_ptrs[i] = &src[Position{ x, y }];
        }
        else {
            view_ptrs[i] = nullptr;
        }
    }
    return view_ptrs;
}

template <int Size = MAX_SURROUNDING_SIZE, typename Array_t>
inline auto BlockView(Array_t& src, Position move) {
    static Eigen::Map<Matrix<Array_t::value_type, HEIGHT, WIDTH>> view(nullptr);
    new (&view) decltype(view)(src.data()); // placement new
    auto left_bound  = std::max(move.x() - Size / 2, 0);
    auto right_bound = std::min(move.x() + Size / 2, WIDTH - 1);
    auto up_bound    = std::max(move.y() - Size / 2, 0);
    auto down_bound  = std::min(move.y() + Size / 2, HEIGHT - 1);
    Position lu{ left_bound, up_bound }, rd{ right_bound, down_bound };
    Position origin = move - Position{ Size / 2, Size / 2 };
    return make_tuple(
        view.block(lu.x(), lu.y(), rd.x() - lu.x() + 1, rd.y() - lu.y() + 1),
        Position(lu - origin), Position(rd - origin)
    );
}

inline const auto SURROUNDING_WEIGHTS = []() {
    Eigen::Array<
        int, MAX_SURROUNDING_SIZE, MAX_SURROUNDING_SIZE
    > W;
    W << 2, 0, 0, 2, 0, 0, 2,
         0, 4, 3, 3, 3, 4, 0,
         0, 3, 5, 4, 5, 3, 0,
         1, 3, 4, 0, 4, 3, 1,
         0, 3, 5, 4, 5, 3, 0,
         0, 4, 3, 3, 3, 4, 0,
         2, 0, 0, 2, 0, 0, 2;
    return W;
}();

Evaluator::Evaluator(Board * board) : m_boardMap(board) {
    this->reset();
}

// ����Ǽ��������ͣ������������Ч��λ('_'�Ϳ�λ��
// ��������������λҲ���ܷ�ס������('_' + '^'�Ϳ�λ����'^'��һ�������ã�������1~2�ε����ڱ����֦����
int Evaluator::patternCount(Position move, Pattern::Type type, Player perspect, Player curPlayer) {
    auto raw_counts = distribution(perspect, type)[move];
    auto critical_blanks = raw_counts & ((1 << 8 * sizeof(int) / 2) - 1); // '_'�Ϳ�λ
    auto irrelevant_blanks = raw_counts >> (8 * sizeof(int) / 2); // '^'�Ϳ�λ
    return perspect == curPlayer ? critical_blanks : critical_blanks + irrelevant_blanks;
}

void Evaluator::updatePattern(string_view target, int delta, Position move, Direction dir) {
    for (auto [pattern, offset] : Patterns.execute(target)) {
        if (pattern.type == Pattern::Five) {
            // ��ǰboard�����applyMove���ʴ˴�����curPlayerΪNone���ᷢ��������
            board().m_curPlayer = Player::None;
            board().m_winner = pattern.belonging;
        } else {
            auto view = LineView(distribution(pattern.belonging, pattern.type), move, dir);
            for (auto idx = 0; idx < pattern.str.length(); ++idx) {
                auto offset = 0;
                switch (pattern.str[idx]) {
                case '-': offset = 0; break;
                case '^': offset = 8 * sizeof(int) / 2; break;
                default: continue;
                }
                auto view_idx = offset + idx;
                *view[view_idx] += delta * (1 << offset);
            }
        }
    }
}

void Evaluator::updateOnePiece(int delta, Position move, Player src_player) {
    auto sgn = [](int x) { return x < 0 ? -1 : 1; }; // 0���ϼ�Ϊ������Ϊԭλ��û���壩
    auto [view, lu, rd] = BlockView(distribution(src_player, Pattern::One), move);
    auto weight_block = SURROUNDING_WEIGHTS.block(lu.x(), lu.y(), rd.x() - lu.x() + 1, rd.y() - lu.y() + 1);
    view.array() += view.array().unaryExpr(sgn) * delta * weight_block; // sgn��Ӧԭλ���Ƿ����壬delta��Ӧ���廹�ǻ���
    for (auto perspect : { Player::Black, Player::White }) {
        auto& count = distribution(perspect, Pattern::One)[move];
        switch (delta) {
        case 1: // ������������һ��
            count *= -1; // ��ת�������������λ����������ռ��
            count -= 1; // ��֤countһ���Ǹ���
            break;
        case -1: // ���������һ��
            count += 1; // ��ȥ֮ǰ�����ĸ�����
            count *= -1; // ��ת����������������������Ʒ���
            break;
        }
    }
}

void Evaluator::updateMove(Position move, Player src_player) {
    for (auto dir : Directions) {
        auto target = m_boardMap.lineView(move, dir);
        updatePattern(target, -1, move, dir); // remove pattern count on original state
    }
    if (src_player != Player::None) {
        m_boardMap.applyMove(move);
        updateOnePiece(1, move, board().m_curPlayer);
    } else {
        m_boardMap.revertMove();
        updateOnePiece(-1, move, -board().m_curPlayer | board().m_winner);
    }
    for (auto dir : Directions) {
        auto target = m_boardMap.lineView(move, dir);
        updatePattern(target, 1, move, dir); // update pattern count on new state
    }
}

Player Evaluator::applyMove(Position move) {
    if (board().m_curPlayer != Player::None && board().checkMove(move)) {
        updateMove(move, board().m_curPlayer);
    }
    return board().m_curPlayer;
}

Player Evaluator::revertMove(size_t count) {
    for (auto i = 0; i < count && !board().m_moveRecord.empty(); ++i) {
        updateMove(board().m_moveRecord.back(), Player::None);
    }
    return board().m_curPlayer;
}

bool Evaluator::checkGameEnd() {
    if (board().m_curPlayer == Player::None) {
        return true;
    } else if (board().moveCounts(Player::None) == 0) {
        board().m_winner = Player::None;
        board().m_curPlayer = Player::None;
        return true;
    } else {
        return false;
    }
}

void Evaluator::reset() {
    m_boardMap.reset();
    for (auto& temp : m_distributions)
    for (auto& distribution : temp) {
        distribution.resize(BOARD_SIZE, 0);
    }
}

// ����ģʽ�����Ϊû�л�Ϊǰ׺��������
PatternSearch Evaluator::Patterns = {
    { "+xxxxx",   Pattern::Five,      99999 },
    { "-_oooo_",  Pattern::LiveFour,  75000 },
    { "-xoooo_",  Pattern::DeadFour,  2500 },
    { "-o_ooo",   Pattern::DeadFour,  3000 },
    { "-oo_oo",   Pattern::DeadFour,  2600 },
    { "-~_ooo^",  Pattern::LiveThree, 3000 },
    { "-^o_oo^",  Pattern::LiveThree, 2800 },
    { "-xooo__",  Pattern::DeadThree, 500 },
    { "-xoo_o_",  Pattern::DeadThree, 800 },
    { "-xo_oo_",  Pattern::DeadThree, 999 },
    { "-oo__o",   Pattern::DeadThree, 600 },
    { "-o_o_o",   Pattern::DeadThree, 550 },
    { "-x_ooo_x", Pattern::DeadThree, 400 },
    { "-^oo__~",  Pattern::LiveTwo,   650 },
    { "-^_o_o_^", Pattern::LiveTwo,   600 },
    { "-x^o_o_^", Pattern::LiveTwo,   550 },
    { "-^o__o^",  Pattern::LiveTwo,   550 },
    { "-xoo___",  Pattern::DeadTwo,   150 },
    { "-xo_o__",  Pattern::DeadTwo,   250 },
    { "-xo__o_",  Pattern::DeadTwo,   200 },
    { "-o___o",   Pattern::DeadTwo,   180 },
    { "-x_oo__x", Pattern::DeadTwo,   100 },
    { "-x_o_o_x", Pattern::DeadTwo,   100 },
    { "-^o___~",  Pattern::LiveOne,   150 },
    { "-x^_o__^", Pattern::LiveOne,   140 },
    { "-x^__o_^", Pattern::LiveOne,   150 },
    { "-xo___~",  Pattern::DeadOne,   30 },
    { "-x_o___x", Pattern::DeadOne,   35 },
    { "-x__o__x", Pattern::DeadOne,   40 },
};

}