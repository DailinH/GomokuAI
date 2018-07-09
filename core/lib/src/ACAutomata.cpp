#include "ACAutomata.h"
#include <algorithm>
#include <numeric>
#include <queue>

using namespace std;

namespace Gomoku {

AhoCorasickBuilder::AhoCorasickBuilder(initializer_list<Pattern> protos)
    : m_tree{ Node{} }, m_patterns(protos) {

}

void AhoCorasickBuilder::build(PatternSearch* searcher) {
    this->reverseAugment();
    this->flipAugment(); 
    this->boundaryAugment(); 
    this->sortPatterns();
    this->buildNodeBasedTrie();
    this->buildDAT(searcher);
    this->buildACGraph(searcher);
}

void AhoCorasickBuilder::reverseAugment() {
    for (int i = 0, size = m_patterns.size(); i < size; ++i) {
        Pattern reversed(m_patterns[i]);
        std::reverse(reversed.str.begin(), reversed.str.end());
        if (reversed.str != m_patterns[i].str) {
            m_patterns.push_back(std::move(reversed));
        }
    }
}

void AhoCorasickBuilder::flipAugment() {
    for (int i = 0, size = m_patterns.size(); i < size; ++i) {
        Pattern flipped(m_patterns[i]);
        flipped.favour = -flipped.favour;
        for (auto& piece : flipped.str) {
            if (piece == 'x') piece = 'o';
            else if (piece == 'o') piece = 'x';
        }
        m_patterns.push_back(std::move(flipped));
    }
}

void AhoCorasickBuilder::boundaryAugment() {
    for (int i = 0, size = m_patterns.size(); i < size; ++i) {
        char enemy = m_patterns[i].favour == Player::Black ? 'o' : 'x';
        auto first = m_patterns[i].str.find_first_of(enemy);
        auto last = m_patterns[i].str.find_last_of(enemy);
        if (first != string::npos) { // ���ֻ���������
            Pattern bounded(m_patterns[i]);
            bounded.str[first] = '?';
            m_patterns.push_back(bounded);
            if (last != first) {
                bounded.str[last] = '?';
                m_patterns.push_back(bounded);
                bounded.str[first] = enemy;
                m_patterns.push_back(std::move(bounded));
            }
        }
    }
}

void AhoCorasickBuilder::sortPatterns() {
    // �������յ�patterns����Ԥ���ռ�
    vector<int> codes(m_patterns.size());
    vector<int> indices(m_patterns.size());

    // ���롢���������
    std::transform(m_patterns.begin(), m_patterns.end(), codes.begin(), [](const Pattern& p) {
        auto align_offset = std::pow(std::size(Codeset), MAX_PATTERN_LEN - p.str.size());
        return std::reduce(p.str.begin(), p.str.end(), 0, [&](int sum, char ch) {
            return sum *= std::size(Codeset), sum += EncodeCharset(ch);
        }) * align_offset; // ��size(Codeset)���Ƽ���������
    }); // ͨ������Ļ���������ʵ���ֵ�������
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
}

/*
    ����set�������������Trie���Ļ����洢�����ڲ���������Ϊ��
    ��һ���ȼ���depthά��
      ���н��ͨ�������ɵ�һ������
    �ڶ����ȼ���siblingsά��
      ͬһ�����н���[first, last)���乹���˸��ڵ��[0, size)�����һ�����֡�
      ��ˣ�ͬһ��Ľ��ɰ���first�������������
    �ɴ˿ɼ�������Node��˵������set��λ�ÿ�ͨ��{ depth, first }Ψһȷ����last�ǿɱ�ġ�

    �����϶����֪��
      1. ĳ���{ depth, first, last }���ӽ�㼯��Ϊdepth + 1���[first, last)���䡣
      2. ������Ҷ�����last - first == 1������depth��ͬ��������໥���Ӻ󣬱��γ���������Patterns���顣
*/
void AhoCorasickBuilder::buildNodeBasedTrie() {
    // suffixΪ��parent���ӽ��Ϊ��㣬ֱ������pattern�����ĺ�׺
    function<void(string_view, NodeIter)> insert_pattern = [&](string_view suffix, NodeIter parent) {
        if (suffix.empty()) {
            // Base case: �ѵִ�Ҷ��㣬�����¼��һ��ƥ��ģʽ��
            // ȷ���ҵ�һ��ģʽ�󣬲����ڱ�Ҷ��㣬��ʹ�����Ҷ˵�����һλ��
            m_tree.emplace(0, parent->depth + 1, parent->first, ++parent->last);
        } else {
            Node key = { // ׼���ӽ������
                EncodeCharset(suffix[0]),
                parent->depth + 1
            };
            auto [first, last] = children(parent);
            auto child = std::find_if(first, last, [&key](const Node& node) { 
                return node.code == key.code; // ����ǰ׺�ַ�ƥ��ý��
            }); 
            if (child == last) { // ǰ׺ƥ��ʧ�ܣ�������һ����֧���
                key.first = parent->last;
                key.last = key.first;
                child = m_tree.insert(key).first;
            }
            insert_pattern(suffix.substr(1), child); // �ݹ齫patternʣ�µĲ��ֲ���trie����
            parent->last = child->last; // ���򴫲������������ұ߽�ֵ��
        }
    };
    auto root = m_tree.find({});
    for (auto pattern : m_patterns) {
        insert_pattern(pattern.str, root); // ���ֵ������pattern
    }
}

/*
    ������Ϻ�base��check���������Ϊ��
    base:
      1. ���ڷ�Ҷ��㣬baseֵΪ�����ӽ���ƫ�ƻ�׼ֵ��t = base[s] + c����
      2. ����Ҷ��㣬baseֵΪ�������Ӧģʽ���±�ĸ�����patterns[-base[l]]����
      3. ���ڿ���λ�ã�baseֵΪ���������ǰ�������±�ĸ�����v_{i-1} = -base[v_i])��
    check:
      1. ����һ���㣬checkֵΪ�丸�������λ�ã�s = check[t <- base[s] + c]����
      2. ���ڿ���λ�ã�checkֵΪ��������ĺ�̽����±�ĸ�����v_{i+1} = -check[v_i])��
      3. ���ڸ��ڵ㣬�������޸���㣬��û��һ��checkֵ����λ����������������������㡣

    �������϶��壬baseֵ�������������Ϊ0:
      1. ƫ��ֵΪ0������ڵ㣩
      2. ��Ӧģʽ���±�Ϊ0����һ��Ҷ��㣩
      3. ǰ�������±�Ϊ0���׸�����λ�ã�
    3��1,2�Ĺ�������û�н����������õ��Զ�����ƥ������в����������λ�ã�
    ����1,2��Ҷ�����ж�����Ϊ��check[base[s]] == s
      * ���ڵ��base[0] == 0�����������϶���check[0] < 0����˲������и��ڵ���Ҷ��㡣
      * ��Ӧģʽ���±�Ϊ0����check[0] < 0��Ҳ�����ж�Ҷ��㻹��Ҷ��㡣
    ���Ҳ���������ͻ��
*/
void AhoCorasickBuilder::buildDAT(PatternSearch* ps) {
    // indexΪnode��˫�����е�������֮ǰ�ĵݹ�����ȷ����
    function<void(int, NodeIter)> build_recursive = [&](int index, NodeIter node) {
        if (node->depth > 0 && node->code == 0) {
            // Base case: �ѵִ�Ҷ��㣬����index��baseΪ(-��Ӧpattern����±�)
            // ��ʱnode��last - first == 1, [first, last)Ψһȷ����һ�����
            ps->m_base[index] = -node->first; 
        } else {
            // ׼������
            auto [first, last] = children(node);
            int begin = 0, front = 0;

            /*
                ��ʼ������begin��0��ʼ��������ڵ�baseֵΪ0����front��0����Ѱ�ҿ�λ��
                �˳��������ҵ�һ����������index���������ӽ���beginֵ
                ״̬ת�ƣ���ǰ�������ҵ���һ����λ���������λ���������׸��ӽ���λ��
                �ο���http://www.aclweb.org/anthology/D13-1023
            */
            do {
                front = -ps->m_check[front]; // �׸��ӽ����±�
                begin = front - first->code; // �ӽ���ƫ�ƻ�׼ֵ

                // ���ڸ���baseֵ���������壬beginֵ�������0��
                if (begin < 0) continue;

                // �ռ䲻��ʱ����m_base��m_check���顣
                // ��ֵ��Ϊsize - 1�Ա�֤���һλΪ�գ���ʽ1�Ƶ�������Է�ֹ�����
                while (begin + std::size(Codeset) + 1 >= ps->m_check.size()) {
                    // ����ռ�
                    auto pre_size = ps->m_base.size();
                    ps->m_base.resize(2 * pre_size);
                    ps->m_check.resize(2 * pre_size);
                    // ����±겹ȫ˫����
                    for (int i = pre_size; i < ps->m_base.size(); ++i) {
                        ps->m_base[i] = -(i - 1); // ��������
                        ps->m_check[i] = -(i + 1); // ǰ������
                    }
                }

            // ��֤�Ƿ����к�ѡ�ӽ��λ�þ�δ��ռ��
            // ɸѡ���������ڵ�/checkֵ��С��0�Ľ���Ǳ�ռ�õġ�
            } while (!std::all_of(first, last, [&](const Node& node) {
                auto c_i = begin + node.code;
                return c_i != 0 && ps->m_check[c_i] < 0;
            }));

            // �����ӽ�㣬�������״̬����ӽ��ݹ鹹��
            for (auto cur = first; cur != last; ++cur) {
                // ȡ�õ�ǰ�ӽڵ��±ֵ꣨��ע�����Ҷ����±��Ϊbegin��
                int c_i = begin + cur->code;

                // ����ǰ�±��Ƴ����нڵ���������Dancing Links��
                ps->m_check[-ps->m_base[c_i]] = ps->m_check[c_i];
                ps->m_base[-ps->m_check[c_i]] = ps->m_base[c_i];

                // ���ӽ��checkֵ�븸����
                ps->m_check[c_i] = index;
            }
            // ������base����Ϊ�Һõ�beginֵ
            ps->m_base[index] = begin; 
            // Recursive Step: ��ÿ���ӽ��ݹ鹹��
            for (auto cur = first; cur != last; ++cur) {
                build_recursive(begin + cur->code, cur);
            }
        }
    };
    ps->m_base.resize(1, 0);   // ���ڵ�(0)û��ǰ����㣬����baseλ��Ϊ��������ı�ǵ㣬����������baseֵ��
    ps->m_check.resize(1, -1); // ���ڵ�(0)����û�и���㣬�����ޱ���checkֵ����λ��������Ϊǰ���������㡣
    auto root = m_tree.find({});
    build_recursive(0, root);
    ps->m_patterns.swap(m_patterns);
}

void AhoCorasickBuilder::buildACGraph(PatternSearch* ps) {
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

        // ���ڵ�failָ��ָ���Լ�����������
        if (cur_node == 0) continue;

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
}

}