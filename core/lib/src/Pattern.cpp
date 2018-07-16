#include "Pattern.h"
#include "ACAutomata.h"
#include <iostream>
#include <bitset>

using namespace std;
using namespace Eigen;

namespace Gomoku {

/* ------------------- Pattern��ʵ�� ------------------- */

Pattern::Pattern(std::string_view proto, Type type, int score) 
    : str(proto.begin() + 1, proto.end()), score(score), type(type),
      favour(proto[0] == '+' ? Player::Black : Player::White) {

}

/* ------------------- PatternSearch��ʵ�� ------------------- */

bool PatternSearch::HasCovered(const Entry& entry, size_t pose) {
    const auto [pattern, offset] = entry;
    return 0 <= offset - pose && offset - pose < pattern.str.length();
}

PatternSearch::PatternSearch(initializer_list<Pattern> protos) {
    AhoCorasickBuilder builder(protos);
    builder.build(this);
}

// ����ת�ƣ�ֱ��ƥ����һ���ɹ���ģʽ���ѯ����
const PatternSearch::generator& PatternSearch::generator::operator++() {
    do {
        if (target.empty()) { // Ŀ��ƥ����ȫ����
            state = 0; // ��״̬����Ϊ��ʼ״̬
            break; // �˴�break�󣬸�generator�ﵽ��end״̬
        }
        int code = target[0]; // ȡ����ǰҪ����Ŀ��λ
        if (state == ref->m_invariants[code]) { // �жϵ�ǰ״̬�ڽ���code���Ƿ񲻷���ת�ƣ����������㡹״̬��
            while (!target.empty() && target[0] == code) { // ������ת�߳�����ת��״̬
                ++offset, target.remove_prefix(1); 
            }
            continue;
        }
        int next = ref->m_base[state] + code; // ��ȡ��һ״̬������
        if (ref->m_check[next] == state) { // ��������ƥ��target
            state = next; // ƥ��ɹ�ת��
        } else if (state != 0) { // ƥ��ʧ��ʱ���жϡ��������ڸ��ڵ�ƥ��ʧ�ܣ����������ַ���������
            state = ref->m_fail[state]; // ƥ��ʧ��ת��
            continue; // ʧ��ת�ƺ�����һѭ���ٴγ���ƥ��
        }
        ++offset, target.remove_prefix(1);
    } while (ref->m_check[ref->m_base[state]] != state); // ����Ҷ�ӽ��ʱ����ʱ�ж�ƥ��
    return *this;
}

// ������ǰ�ɹ�ƥ�䵽��ģʽ
PatternSearch::Entry PatternSearch::generator::operator*() const {
    const auto leaf = ref->m_base[state];
    return { ref->m_patterns[-ref->m_base[leaf]], offset };
}

PatternSearch::generator PatternSearch::execute(string_view target) {
    return generator{ target, this };
}

const vector<PatternSearch::Entry>& PatternSearch::matches(string_view target) {
    static vector<Entry> entries;
    entries.clear();
    for (auto record : execute(target)) { 
        entries.push_back(record); 
    }
    return entries;
}

/* ------------------- BoardMap��ʵ�� ------------------- */

tuple<int, int> BoardMap::ParseIndex(Position pose, Direction direction) {
    // ����ǰ���MAX_PATTERN_LEN - 1λ��Ϊ'?'��Խ��λ�����������ʼoffset
    switch (int offset = MAX_PATTERN_LEN - 1; direction) {
        case Direction::Horizontal: // 0 + y��[0, HEIGHT) | x: 0 -> WIDTH
            return { pose.y(), offset + pose.x() };
        case Direction::Vertical:   // HEIGHT + x��[0, WIDTH) | y: 0 -> HEIGHT
            return { HEIGHT + pose.x(), offset + pose.y() };
        case Direction::LeftDiag:   // (WIDTH + HEIGHT) + (HEIGHT - 1) + x-y��[-(HEIGHT - 1), WIDTH) | min(x, y): (x, y) -> (x+1, y+1)
            return { WIDTH + 2 * HEIGHT - 1 + pose.x() - pose.y(), offset + std::min(pose.x(), pose.y()) };
        case Direction::RightDiag:  // 2*(WIDTH + HEIGHT) - 1 + x+y��[0, WIDTH + HEIGHT - 1) | min(WIDTH - 1 - x, y): (x, y) -> (x-1, y+1)
            return { 2 * (WIDTH + HEIGHT) - 1 + pose.x() + pose.y(), offset + std::min(WIDTH - 1 - pose.x(), pose.y()) };
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
        m_lineMap[index][offset] = EncodeCharset(m_board->m_curPlayer == Player::Black ? 'x' : 'o');
    }
    return m_board->applyMove(move, false);
}

Player BoardMap::revertMove(size_t count) {
    for (int i = 0; i < count; ++i) {
        for (auto direction : Directions) {
            const auto [index, offset] = ParseIndex(m_board->m_moveRecord.back(), direction);
            m_lineMap[index][offset] = EncodeCharset('-');
        }
        m_board->revertMove();
    }
    return m_board->m_curPlayer;
}

void BoardMap::reset() {
    m_hash = 0ul;
    m_board->reset();
    for (auto& line : m_lineMap) {
        line.resize(MAX_PATTERN_LEN - 1, EncodeCharset('?')); // ��ǰ���Խ��λ('?')
    }
    for (auto i = 0; i < BOARD_SIZE; ++i) 
    for (auto direction : Directions) {
        auto [index, _] = ParseIndex(i, direction);
        m_lineMap[index].push_back(EncodeCharset('-')); // ��ÿ��λ������λ('-')
    }
    for (auto& line : m_lineMap) {
        line.append(MAX_PATTERN_LEN - 1, EncodeCharset('?')); // �ߺ����Խ��λ('?')
    }
}

/* ------------------- Evaluator��ʵ�� ------------------- */

template <size_t Length = TARGET_LEN, typename Array_t>
inline auto& LineView(Array_t& src, Position move, Direction dir) {
    static vector<typename Array_t::value_type*> view_ptrs(Length);
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

template <int Size = BLOCK_SIZE, typename Array_t, typename value_t = Array_t::value_type>
inline auto BlockView(Array_t& src, Position move) {
    auto left_bound  = std::max(move.x() - Size / 2, 0);
    auto right_bound = std::min(move.x() + Size / 2, WIDTH - 1);
    auto up_bound    = std::max(move.y() - Size / 2, 0);
    auto down_bound  = std::min(move.y() + Size / 2, HEIGHT - 1);
    Position lu{ left_bound, up_bound }, rd{ right_bound, down_bound };
    if constexpr (std::is_same_v<Array_t, Array<value_t, Size, Size, RowMajor>>) { // Ȩ�ؾ�����������
        Position coord_transform = move - Position{ Size / 2, Size / 2 };
        lu = lu - coord_transform, rd = rd - coord_transform;
        return src.block(lu.y(), lu.x(), rd.y() - lu.y() + 1, rd.x() - lu.x() + 1);
    } else { // һ��board-like����������������������
        Eigen::Map<Array<value_t, HEIGHT, WIDTH, RowMajor>> view(src.data());
        return view.block(lu.y(), lu.x(), rd.y() - lu.y() + 1, rd.x() - lu.x() + 1);
    }
}

void Evaluator::updateLine(string_view target, int delta, Position move, Direction dir) {
    const auto& match_results = Patterns.matches(target);
    const auto update_pattern = [&]() {
        auto& ev = *this;
        for (const auto [pattern, offset] : match_results) {
            if (pattern.type == Pattern::Five) {
                // ��ǰboard�����applyMove���ʴ˴�����curPlayerΪNone���ᷢ��������
                board().m_curPlayer = Player::None;
                board().m_winner = pattern.favour;
                continue;
            }
            m_patternDist.back()[pattern.type].set(delta, pattern.favour); // �޸��ܼ���
            for (int i = 0; i < pattern.str.length(); ++i) { // �޸Ŀ�λ����
                const auto piece = pattern.str.rbegin()[i];
                if (piece == '_' || piece == '^') { // '_'��������Ч��λ��'^'����Է����ƿ�λ
                    const auto pose = Shift(move, offset - i - TARGET_LEN / 2, dir);
                    const auto multiplier = (dir == Direction::LeftDiag || dir == Direction::RightDiag ? 1.2 : 1);
                    const auto score = int(delta * multiplier * pattern.score);
                    switch (piece) { // ������switch�Ĵ�͸����
                    case '_':
                        m_patternDist[pose][pattern.type].set(delta, pattern.favour, pattern.favour, dir);
                        scores(pattern.favour, pattern.favour)[pose] += score;
                        if (scores(pattern.favour, pattern.favour)[pose] < 0) {
                            auto str = ev.board().toString();
                        }
                    case '^':
                        m_patternDist[pose][pattern.type].set(delta, pattern.favour, -pattern.favour, dir);
                        scores(pattern.favour, -pattern.favour)[pose] += score;
                        if (scores(pattern.favour, -pattern.favour)[pose] < 0) {
                            auto str = ev.board().toString();
                        }
                    }
                }
            }
        }
    };
    const auto update_compound = [&]() {
        for (auto [pattern, offset] : match_results) {
            for (int i = 0; i < pattern.str.length(); ++i) {
                auto piece = pattern.str.rbegin()[i];
                auto pose = Shift(move, offset - i - TARGET_LEN / 2, dir);
                if (piece == '_' && // BUG DETECTED: ��3+��3�ᱻ�ж�Ϊ43
                    pattern.type >= Pattern::LiveTwo && pattern.type < Pattern::DeadFour &&
                    Compound::Test(*this, pose, pattern.favour)) {
                    Compound::Update(*this, delta, pose, pattern, dir);
                }
            }
        }
    };
    if (delta == 1) {
        update_pattern(), update_compound();
    } else {
        update_compound(), update_pattern();
    }
}

void Evaluator::updateBlock(int delta, Position move, Player src_player) {
    // ����׼��
    auto sgn = [](int x) { return x < 0 ? -1 : 1; }; // 0���ϼ�Ϊ������Ϊԭλ��û���壩
    auto relu = [](int x) { return std::max(x, 0); };
    auto [weights, score] = BlockWeights;
    auto density_block = BlockView(m_density[Group(src_player)], move);
    auto score_block   = BlockView(m_scores[Group(src_player)], move);
    auto weight_block  = BlockView(weights, move);
    // ��ȥ��density��������ʼ�����density_blockΪ0���󣬼�ȥҲû��Ӱ�죩
    //score_block -= score * density_block.unaryExpr(relu);    
    // ����density��sgn��Ӧԭλ���Ƿ����壬delta��Ӧ���廹�ǻ��壩��
    density_block += density_block.unaryExpr(sgn) * delta * weight_block;
    // �޸�����λ�õ�״̬���Ƿ���������Ʒ֣�
    for (auto perspective : { Player::Black, Player::White }) {
        switch (auto& count = m_density[Group(perspective)][move]; delta) {
        case 1: // ������������һ��
            count *= -1, count -= 1; break; // ��ת�������������λ����������ռ�ã�����ȥһ����������֤countһ���Ǹ���
        case -1: // ���������һ��
            count += 1, count *= -1; break; // ��ȥ֮ǰ�����ĸ������󣬷�ת����������������������Ʒ���
        }
    }
    //cout << "Current: " << to_string(src_player) << endl;
    //cout << endl << "White:\n" << Map<Array<int, WIDTH, HEIGHT, RowMajor>>(m_density[0].data()) << endl
    //     << "Black:\n" << Map<Array<int, WIDTH, HEIGHT, RowMajor>>(m_density[1].data()) << endl;
    // ������density����
    //score_block += score * density_block.unaryExpr(relu);
}

void Evaluator::updateMove(Position move, Player src_player) {
    for (const auto dir : Directions) {
        auto target = m_boardMap.lineView(move, dir);
        updateLine(target, -1, move, dir); // remove pattern count on original state
    }
    if (src_player != Player::None) {
        m_boardMap.applyMove(move);
        updateBlock(1, move, src_player);
    } else {
        m_boardMap.revertMove();
        updateBlock(-1, move, board().m_curPlayer);
    }
    for (const auto dir : Directions) {
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
    for (int i = 0; i < BOARD_SIZE; ++i) {
        if (!board().moveState(Player::None, i)) {
            for (int j = 0; j < 4; ++j) {
                if (m_scores[j][i] != 0) {
                    auto b = board().toString();
                    throw b;
                }
            }
        }
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
    int i;
    for (i = 0; i < board.m_moveRecord.size(); ++i) {
        if (i < this->board().m_moveRecord.size()) {
            if (this->board().m_moveRecord[i] == board.m_moveRecord[i]) {
                continue;
            } else {
                revertMove(this->board().m_moveRecord.size() - i); // ���˵��״β�ͬ����״̬
            }
        } 
        applyMove(board.m_moveRecord[i]);
    }
    revertMove(this->board().m_moveRecord.size() - i); // ���˵�������
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
    for (auto& distribution : m_compoundDist) {
        distribution.fill(Record{}); // ���һ��Ԫ�������ܼ���
    }
}

void Evaluator::Record::set(int delta, Player player) {
    unsigned offset = 4*sizeof(field)*Group(player);
    field += delta << offset;
}

void Evaluator::Record::set(int delta, Player favour, Player perspective, Direction dir) {
    unsigned group = Group(favour, perspective), offset = (4*group + int(dir))*2;
    unsigned lower = 1 << offset, higher = lower << 1, mask = higher | lower;
    unsigned value = (delta == 1 ? (field << 1) | lower : (field >> 1) & ~higher);
    field = (field & ~mask) | (value & mask);
}

unsigned Evaluator::Record::get(Player favour, Player perspective, Direction dir) const {
    unsigned group = Group(favour, perspective);
    unsigned offset = (4*group + int(dir))*2;
    return (field >> offset) & 0b11;
}

unsigned Evaluator::Record::get(Player favour, Player perspective) const {
    unsigned group = Group(favour, perspective);
    return (field >> 8*group) & 0xff; 
}

unsigned Evaluator::Record::get(Player player) const {
    unsigned offset = 4*sizeof(field)*Group(player);
    return (field >> offset) & ((1 << 4*sizeof(field)) - 1);
}

bool Evaluator::Compound::Test(Evaluator& ev, Position pose, Player player) {
    unsigned bit_field = 0; // �����Ǹ��˸о�������㷨����
    for (auto comp_type : Components) {
        // bit_field�������ĸ������L3/D3/L2���������
        // ����λ������ԣ�һ������ͬʱ��LiveTwo��DeadThreeʱ��ֻ�����һ�Σ������BUG��
        bit_field |= ev.m_patternDist[pose][comp_type].get(player, player);
    }
    // is-power-of-2�㷨�������ж��Ƿ���>=2��bitλΪ1��>=2��������D2��LD3��
    return bit_field & (bit_field - 1);
}

void Evaluator::Compound::Update(Evaluator& ev, int delta, Position pose, const Pattern& base, Direction base_dir) {
    // ׼��״̬����
    auto str = ev.board().toString();
    enum State { S0, L2, LD3, To33, To43, To44 } state = S0; // �Զ�����״̬����
    static vector<tuple<Direction, Pattern::Type>> components; // �洢���ɸ���ģʽ�ĸ�����ģʽ�����������ڷ���
    components.clear(); // �����˳�������returnλ�ò��̶������ڽ��뺯��ʱ����
    bool triple_cross = false; // �Ƿ�������ģʽ�㼯��poseλ
    int l3_count = 0; // ����ģʽ�л���������
    Player player = base.favour;

    // ��λ����ģʽ������Լ����״̬��
    for (auto dir : Directions) {
        // ׼��ת������
        int cond = S0; // ��ʼת������
        int count = 0; // ��ǰ������ģʽ����
        for (auto comp_type : Components) {
            switch (ev.m_patternDist[pose][comp_type].get(player, player, dir)) {
                case 0b00: count = 0; break;
                case 0b01: count = 1; break;
                case 0b11: count = 2; break;
            }
            if (count != 0) {
                switch (comp_type) {
                    case Pattern::LiveThree:
                        l3_count += 1;
                    case Pattern::DeadThree:
                        cond = LD3; break;
                    case Pattern::LiveTwo:
                        cond = L2;  break;
                }
                components.insert(components.end(), count, { dir, comp_type });
                if (dir == base_dir && (comp_type > base.type || count == 2)) {
                    // ����ǰ�����и������ȼ���ģʽ��������2��ģʽ����ֹ�ظ�������
                    return; // ������ĸ���״̬����ת�ƣ��������ֱ���˳�
                }
                break; // �ҵ���һ������������Pattern���˳������Pattern�����ȼ�֮�֡�
            }
        }

        // û���ڸ÷�����ƥ�䵽ģʽʱ��״̬����ת��
        if (cond == S0) continue;

        // ״̬ת��
        for (int i = 0; i < count; ++i) {
            switch (int offset; state) {
                case S0:                           //                    To44(5)
                    offset = 0; goto Transfer;     //                 �J   ��
                case L2: case LD3:                 //          LD3(2) �� To43(4)  ״̬ת��ͼ����������
                    offset = 1; goto Transfer;     //        �J       �J   ��
                case To33: case To43: case To44:    //  S0(0) �� L2(1) �� To33(3)
                    triple_cross = true;
                    offset = (state == To44 ? -cond : -1); // To44Ϊ����״̬��ֻ������
                Transfer:
                    state = State(state + cond + offset);
            }
        }
    }

    // ׼����������
    const auto type = Compound::Type(state - State::To33);
    const auto count = bitset<8>(ev.m_compoundDist[pose][type].get(player, player)).count(); // ��λ����״̬
    const auto update_pose = [&](Direction dir, Pattern::Type comp_type) {
        // ����self_worthy���������
        ev.m_compoundDist[pose][type].set(delta, player, player, dir);
        ev.scores(player, player)[pose] += delta * Compound::BaseScore;
        // ����rival_anti���������
        ev.m_compoundDist[pose][type].set(delta, player, -player, dir);
        ev.scores(player, -player)[pose] += delta * Compound::BaseScore;
        assert(ev.scores(player, player)[pose] >= 0);
        assert(ev.scores(player, -player)[pose] >= 0);
    };
    const auto update_antis = [&](PatternSearch::generator& generator, int delta, Direction dir, Pattern::Type comp_type) { // �������пɷ��Ƹ÷����ϵ�ģʽ�Ŀյ㡣
        for (auto [pattern, offset] : generator) {
            if (pattern.type == comp_type && PatternSearch::HasCovered({ pattern, offset }) // �����ǣ���.ģʽ����Ϊcomp_type
                && pattern.str.rbegin()[offset - TARGET_LEN / 2] == '_') { // ��.pose�ǹؼ���'_'��ģʽ������
                auto current = Shift(pose, offset - TARGET_LEN / 2, dir);
                for (int i = 0; i < pattern.str.length(); ++i, SelfShift(current, -1, dir)) {
                    auto piece = pattern.str.rbegin()[i];
                    if ((piece == '_' || piece == '^') && current != pose) { // �����¹ؼ���
                        // ��ʹ�������������ˣ���MCTS��AlphaBetaЭ����Ҳ�ܺܿ챻���֦��
                        ev.m_compoundDist[current][type].set(delta, player, -player, dir);
                        ev.scores(player, -player)[current] += delta * Compound::BaseScore; // ���¶����rival_anti����
                        assert(ev.scores(player, -player)[current] >= 0);
                    }
                }
                break; // һ��ֻ����һ��Pattern
            }
        }
    };

    // ��count��Ϊ״̬��delta��restricted��Ϊ�����ĸ���״̬��
    if ((count == 0 && delta == -1)) { // 0״̬��delta == -1ʱ
        return; // ״̬��ת�ƣ���˵��ת�����������涨λ״̬�����return��ʹ��ͬ���Ĺ��ܣ�
    } else if (count + delta == 1) { // { 0, 1 } �� { 2, -1 }����ת�ƹ�ϵ������ģʽ�Ĵ�����
        ev.m_compoundDist.back()[type].set(delta, player); // ���¸���ģʽ�ܼ���
        auto generator = PatternSearch::generator{};
        auto pre_dir = Direction(-1);
        for (auto [dir, comp_type] : components) { // �������з���
            update_pose(dir, comp_type);
            if (pre_dir != dir) {
                generator = ev.Patterns.execute(ev.m_boardMap.lineView(pose, dir));
                pre_dir = dir;
            }
            if (!triple_cross && l3_count == 0) {
                update_antis(generator, delta, dir, comp_type);
            }
        }
    } else if ((count == 2 && delta == 1) || (count == 3 && delta == -1)) { // ��ת��Ӱ�쵽�Ƿ���¶���antis����
        update_pose(base_dir, base.type);
        if (l3_count == (base.type == Pattern::LiveThree)) { // �������˴���Ļ���������ģʽ��û�������Ļ���
            auto generator = PatternSearch::generator{};
            auto pre_dir = Direction(-1);
            for (auto [dir, comp_type] : components) { // �������з���
                if (comp_type == base.type && dir == base_dir) {
                    continue;
                }
                if (pre_dir != dir) {
                    generator = ev.Patterns.execute(ev.m_boardMap.lineView(pose, dir));
                    pre_dir = dir;
                }
                update_antis(generator, -delta, dir, comp_type);
            }
        }
    } else { // count > 3ʱ���룬������һ������
        assert(count != 1);
        update_pose(base_dir, base.type);
    }
    if (bitset<8>(ev.m_compoundDist[pose][type].get(player, player)).count() == 4) {
        ev.board().toString();
    }
}

PatternSearch Evaluator::Patterns = {
    { "+xxxxx",    Pattern::Five,      9999 },
    { "-_oooo_",   Pattern::LiveFour,  9000 },
    { "-xoooo_",   Pattern::DeadFour,  2500 },
    { "-o_ooo",    Pattern::DeadFour,  3000 },
    { "-oo_oo",    Pattern::DeadFour,  2600 },
    { "-~_ooo_~",  Pattern::LiveThree, 3000 },
    { "-x^ooo_~",  Pattern::LiveThree, 2900 },
    { "-~o_oo~",   Pattern::LiveThree, 2800 },
    { "-~o~oo_~",  Pattern::DeadThree, 1400 },
    { "-~oo~o_~",  Pattern::DeadThree, 1200 },
    { "-x_o~oo~",  Pattern::DeadThree, 1300 },
    { "-x_oo~o~",  Pattern::DeadThree, 1100 },
    { "-xooo__~",  Pattern::DeadThree, 510 },
    { "-xoo_o_~",  Pattern::DeadThree, 520 },
    { "-xoo__o~",  Pattern::DeadThree, 520 },
    { "-xo_oo_~",  Pattern::DeadThree, 530 },
    { "-xo__oo",   Pattern::DeadThree, 530 },
    { "-xooo__x",  Pattern::DeadThree, 500 },
    { "-xoo_o_x",  Pattern::DeadThree, 500 },
    { "-xoo__ox",  Pattern::DeadThree, 500 },
    { "-xo_oo_x",  Pattern::DeadThree, 500 },
    { "-x_ooo_x",  Pattern::DeadThree, 500 },
    { "-~oo__o~",  Pattern::DeadThree, 750 },
    { "-oo__oo",   Pattern::DeadThree, 540 },
    { "-o_o_o",    Pattern::DeadThree, 550 },
    { "-~oo__~",   Pattern::LiveTwo,   650 },
    { "-~_o_o_~",  Pattern::LiveTwo,   600 },
    { "-x^o_o_^",  Pattern::LiveTwo,   550 },
    { "-^o__o^",   Pattern::LiveTwo,   550 },
    { "-xoo___",   Pattern::DeadTwo,   150 },
    { "-xo_o__",   Pattern::DeadTwo,   160 },
    { "-xo__o_",   Pattern::DeadTwo,   170 },
    { "-o___o",    Pattern::DeadTwo,   180 },
    { "-x_oo__x",  Pattern::DeadTwo,   120 },
    { "-x_o_o_x",  Pattern::DeadTwo,   120 },
    { "-~o___~",   Pattern::LiveOne,   150 },
    { "-x~_o__^",  Pattern::LiveOne,   140 },
    { "-x~__o_^",  Pattern::LiveOne,   150 },
    { "-xo___~",   Pattern::DeadOne,   30 },
    { "-x_o___x",  Pattern::DeadOne,   40 },
    { "-x__o__x",  Pattern::DeadOne,   50 },
};

tuple<Array<int, BLOCK_SIZE, BLOCK_SIZE, RowMajor>, int> Evaluator::BlockWeights = []() {
    tuple_element_t<0, decltype(BlockWeights)> weight;
    tuple_element_t<1, decltype(BlockWeights)> score = 10;
    weight << 2, 0, 0, 1, 0, 0, 2,
              0, 4, 3, 3, 3, 4, 0,
              0, 3, 5, 4, 5, 3, 0,
              1, 3, 4, 0, 4, 3, 1,
              0, 3, 5, 4, 5, 3, 0,
              0, 4, 3, 3, 3, 4, 0,
              2, 0, 0, 1, 0, 0, 2;
    return make_tuple(weight, score);
}();

const Pattern::Type Evaluator::Compound::Components[3] = { 
    Pattern::LiveThree, Pattern::DeadThree, Pattern::LiveTwo 
};
const int Evaluator::Compound::BaseScore = 600;

}