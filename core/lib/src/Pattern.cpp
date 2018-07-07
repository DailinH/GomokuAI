#include "Pattern.h"
#include <numeric>
#include <queue>
#include <set>

using namespace std;
using Eigen::Array;

namespace Gomoku {

constexpr Direction Directions[] = {
    Direction::Horizontal, Direction::Vertical, Direction::LeftDiag, Direction::RightDiag
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
      favour(proto[0] == '+' ? Player::Black : Player::White) {

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
            flipped.favour = -flipped.favour;
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
            char enemy = m_patterns[i].favour == Player::Black ? 'x' : 'o';
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
PatternSearch::Entry PatternSearch::generator::operator*() const {
    return { ref->m_patterns[-ref->m_base[state]], offset };
}

PatternSearch::generator PatternSearch::execute(string_view target) {
    return generator{ target, this };
}

vector<PatternSearch::Entry> PatternSearch::matches(string_view target) {
    vector<Entry> entries;
    for (auto record : execute(target)) { 
        entries.push_back(record); 
    }
    return entries;
}

/* ------------------- BoardMap��ʵ�� ------------------- */

constexpr tuple<int, int> BoardMap::ParseIndex(Position pose, Direction direction) {
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
    static vector<Array_t::value_type*> view_ptrs(Length);
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

template <int Size = BLOCK_SIZE, typename Array_t>
inline auto BlockView(Array_t& src, Position move) {
    static Eigen::Map<Array<Array_t::value_type, HEIGHT, WIDTH>> view(nullptr);
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

void Evaluator::updateLine(string_view target, int delta, Position move, Direction dir) {
    for (auto [pattern, offset] : Patterns.execute(target)) {
        if (pattern.type == Pattern::Five) {
            // ��ǰboard�����applyMove���ʴ˴�����curPlayerΪNone���ᷢ��������
            board().m_curPlayer = Player::None;
            board().m_winner = pattern.favour;
        } else {
            for (int i = 0; i < pattern.str.length(); ++i) {
                auto piece = pattern.str[i];
                if (piece == '_' || piece == '^') { // '_'��������Ч��λ��'^'����Է����ƿ�λ
                    auto pose = Shift(move, offset + i - TARGET_LEN / 2, dir);
                    auto perspective = (piece == '_' ? pattern.favour : -pattern.favour);
                    auto multiplier = (dir == Direction::LeftDiag || dir == Direction::RightDiag ? 1.2 : 1);
                    // �����ÿ��۵���ø����Ķ�Ч������
                    auto update_pattern = [&]() { 
                        m_patternDist[pose][pattern.type].set(delta, pattern.favour, perspective, dir);
                        m_patternDist.back()[pattern.type].set(delta, pattern.favour); // �޸��ܼ���
                        switch (int idx = (pattern.favour == Player::Black); piece) { // ������switch�Ĵ�͸����
                            case '_': scores(pattern.favour, pattern.favour)[move] += delta * multiplier * pattern.score;
                            case '^': scores(pattern.favour, -pattern.favour)[move] += delta * multiplier * pattern.score;
                        }
                    };
                    auto update_compound = [&]() {
                        if (Compound::Test(this, move, pattern.favour)) {
                            Compound::Update(this, delta, move, pattern.favour);
                        }
                    };
                    if (delta == 1) {
                        update_pattern(), update_compound();
                    } else {
                        update_compound(), update_pattern();
                    }
                }
            }
        }
    }
}

void Evaluator::updateBlock(int delta, Position move, Player src_player) {
    // ����׼��
    auto sgn = [](int x) { return x < 0 ? -1 : 1; }; // 0���ϼ�Ϊ������Ϊԭλ��û���壩
    auto relu = [](int x) { return std::max(x, 0); };
    auto [density_block, lu, rd] = BlockView(m_density[src_player == Player::Black], move);
    auto [score_block, _, _] = BlockView(m_scores[src_player == Player::Black], move);
    auto [weight_block, score] = BlockWeights;
    // ��ʽ����
    weight_block = weight_block.block(lu.x(), lu.y(), rd.x() - lu.x() + 1, rd.y() - lu.y() + 1);   
    // ��ȥ��density��������ʼ�����density_blockΪ0���󣬼�ȥҲû��Ӱ�죩
    score_block -= score * density_block.unaryExpr(relu);    
    // ����density��sgn��Ӧԭλ���Ƿ����壬delta��Ӧ���廹�ǻ��壩��
    density_block += density_block.unaryExpr(sgn) * delta * weight_block;
    // �޸�����λ�õ�״̬���Ƿ���������Ʒ֣�
    for (auto perspective : { Player::Black, Player::White }) {
        switch (auto& count = m_density[perspective == Player::Black][move]; delta) {
        case 1: // ������������һ��
            count *= -1, count -= 1; break; // ��ת�������������λ����������ռ�ã�����ȥһ����������֤countһ���Ǹ���
        case -1: // ���������һ��
            count += 1, count *= -1; break; // ��ȥ֮ǰ�����ĸ������󣬷�ת����������������������Ʒ���
        }
    }
    // ������density����
    score_block += score * density_block.unaryExpr(relu);
}

void Evaluator::updateMove(Position move, Player src_player) {
    for (auto dir : Directions) {
        auto target = m_boardMap.lineView(move, dir);
        updateLine(target, -1, move, dir); // remove pattern count on original state
    }
    if (src_player != Player::None) {
        m_boardMap.applyMove(move);
        updateBlock(1, move, board().m_curPlayer);
    } else {
        m_boardMap.revertMove();
        updateBlock(-1, move, -board().m_curPlayer | board().m_winner);
    }
    for (auto dir : Directions) {
        auto target = m_boardMap.lineView(move, dir);
        updateLine(target, 1, move, dir); // update pattern count on new state
    }
}

Evaluator::Evaluator(Board * board) : m_boardMap(board) {
    this->reset();
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

void Evaluator::syncWithBoard(const Board& board) {
    for (int i = 0; i < board.m_moveRecord.size(); ++i) {
        if (i < this->board().m_moveRecord.size()) {
            if (this->board().m_moveRecord[i] == board.m_moveRecord[i]) {
                continue;
            } else {
                revertMove(this->board().m_moveRecord.size() - i); // ���˵��״β�ͬ����״̬
            }
        }
        applyMove(board.m_moveRecord[i]);
    }
}

void Evaluator::reset() {
    m_boardMap.reset();
    for (auto& scores : m_scores) {
        scores.resize(BOARD_SIZE), scores.setZero();
    }
    for (auto& density : m_density) {
        density.resize(BOARD_SIZE), density.setZero();
    }
    for (auto& distribution : m_patternDist) {
        distribution.fill(Record{}); // ���һ��Ԫ�������ܼ���
    }
}

void Evaluator::Record::set(int delta, Player player) {
    unsigned offset = (player == Player::Black ? 4*sizeof(field) : 0);
    field += delta << offset;
}

void Evaluator::Record::set(int delta, Player favour, Player perspective, Direction dir) {
    unsigned group = (favour == Player::Black) << 1 | (perspective == Player::Black);
    unsigned offset = 4 * group + int(dir);
    field &= ~(1 << offset) | ((delta == 1) << offset);
}

bool Evaluator::Record::get(Player favour, Player perspective, Direction dir) const {
    unsigned group = Group(favour, perspective);
    unsigned offset = 4 * group + int(dir);
    return (field >> offset) & 1;
}

unsigned Evaluator::Record::get(Player favour, Player perspective) const {
    unsigned group = Group(favour, perspective);
    return (field >> 4*group) & 0b1111; 
}

unsigned Evaluator::Record::get(Player player) const {
    unsigned offset = (player == Player::Black ? 4*sizeof(field) : 0);
    return (field >> offset) & ((1 << 4*sizeof(field)) - 1);
}

bool Evaluator::Compound::Test(Evaluator * ev, Position pose, Player player) {
    unsigned bit_field = 0; // �����Ǹ��˸о�������㷨����
    for (auto comp_type : Components) {
        // bit_field�������ĸ������L3/D3/L2���������
        // ����λ������ԣ�һ������ͬʱ��LiveTwo��DeadThreeʱ��ֻ�����һ�Σ������BUG��
        bit_field |= ev->m_patternDist[pose][comp_type].get(player, player);
    }
    // is-power-of-2�㷨�������ж��Ƿ���>=2��bitλΪ1��>=2��������D2��L3��
    return bit_field & (bit_field - 1);
}

void Evaluator::Compound::Update(Evaluator* ev, int delta, Position pose, Player player) {
    // ׼������
    enum State { S0, L2, LD3, To33, To43, To44 } state = S0; // �Զ�����״̬����
    array<Pattern::Type, 4> components; components.fill(Pattern::Type(-1)); // -1����÷�����û��ƥ�䵽��Чģʽ��
    bool restricted = false; // Ϊtrue����������Ҫ���ƣ�ֻ�ܶ�����ؼ��㡣
    // �Զ�����ʽ���
    for (auto dir : Directions) {
        // ׼��ת������
        int cond = S0;
        for (auto comp_type : Components) {
            if (ev->m_patternDist[pose][comp_type].get(player, player, dir)) {
                switch (comp_type) {
                    case Pattern::LiveThree:
                        restricted = true;
                    case Pattern::DeadThree:
                        cond = LD3; break;
                    case Pattern::LiveTwo:
                        cond = L2; break;
                }
                components[int(dir)] = comp_type;
                break; // �ҵ���һ������������Pattern���˳������Pattern�����ȼ�֮�֡�
            }
        }
        // û���ڸ÷�����ƥ�䵽ģʽʱ��״̬����ת��
        if (cond == S0) {
            continue;
        }
        // ״̬ת��
        switch (int offset; state) {           
            case S0:                           //                    To44(5)
                offset = 0; goto Transfer;     //                 �J   ��
            case L2: case LD3:                 //          LD3(2) �� To33(4)  ״̬ת��ͼ����������
                offset = 1; goto Transfer;     //        �J       �J   ��
            case To33: case To43: case To44:    //  S0(0) �� L2(1) �� To22(3)
                restricted = true;
                offset = (state == To44 ? -cond : -1); // To44Ϊ����״̬��ֻ������
            Transfer: 
                state = State(state + cond + offset);
        }
    }
    // ��ʽ����
    auto type = state - State::To33;
    for (int i = 0; i < 4; ++i) {
        if (components[i] == -1) {
            continue;
        }
        auto dir = Direction(i);
        // ���¼������������
        ev->m_compoundDist[pose][type].set(delta, player, player, dir);
        ev->m_compoundDist.back()[type].set(delta, player); // ���Ӽ���
        ev->scores(player, player)[pose] += delta * 4 * Compound::BaseScore; // 4Ϊ��������
        // ���¶Է����Ʒ���
        if (restricted) { // ֻ���¹ؼ��㣨pose��
            ev->m_compoundDist[pose][type].set(delta, player, -player, dir);
            ev->scores(player, -player)[pose] += delta * 4 * Compound::BaseScore;
        } else { // �������пɷ����������ϵ����͵Ŀյ㡣���չؼ����������Ϊ�ǹؼ����4����
            // Ϊ��ֹ�����������̽������/����ʽ����
            for (int ds : { 1, -1 }) { // ds��dx, dy��˼��࡭��
                Position current(pose);
                for (int offset = 0; offset < TARGET_LEN / 2; ++offset) {
                    // ��ʹ�����ˣ���MCTS��AlphaBetaЭ����Ҳ�ܺܿ챻���֦��
                    if (ev->m_patternDist[current][components[i]].get(player, -player, dir)) {
                        ev->m_compoundDist[current][type].set(delta, player, -player, dir);
                        ev->scores(player, -player)[current] += delta * Compound::BaseScore; // �޽�������
                    }
                    auto [x, y] = current; auto [dx, dy] = *dir;
                    if ((x + dx >= 0 && x + dx < WIDTH) && (y + dy >= 0 && y + dy < HEIGHT)) {
                        current = Shift(current, ds, dir);
                    } else {
                        break;
                    }
                }
            }
        }
    }
}

// ����ģʽ�����Ϊû�л�Ϊǰ׺��������
PatternSearch Evaluator::Patterns = {
    { "+xxxxx",   Pattern::Five,      9999 },
    { "-_oooo_",  Pattern::LiveFour,  9000 },
    { "-xoooo_",  Pattern::DeadFour,  2500 },
    { "-o_ooo",   Pattern::DeadFour,  3000 },
    { "-oo_oo",   Pattern::DeadFour,  2600 },
    { "-~_ooo_~", Pattern::LiveThree, 3000 },
    { "-x^ooo_~", Pattern::LiveThree, 2900 },
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
    { "-x_o___x", Pattern::DeadOne,   40 },
    { "-x__o__x", Pattern::DeadOne,   50 },
};

tuple<Array<int, BLOCK_SIZE, BLOCK_SIZE>, int> Evaluator::BlockWeights = []() {
    tuple_element_t<0, decltype(BlockWeights)> weight;
    tuple_element_t<1, decltype(BlockWeights)> score = 15;
    weight<< 2, 0, 0, 2, 0, 0, 2,
             0, 4, 3, 3, 3, 4, 0,
             0, 3, 5, 4, 5, 3, 0,
             1, 3, 4, 0, 4, 3, 1,
             0, 3, 5, 4, 5, 3, 0,
             0, 4, 3, 3, 3, 4, 0,
             2, 0, 0, 2, 0, 0, 2;
    return make_tuple(weight, score);
}();

const Pattern::Type Evaluator::Compound::Components[3] = { 
    Pattern::LiveThree, Pattern::DeadThree, Pattern::LiveTwo 
};
const int Evaluator::Compound::BaseScore = 800;

}