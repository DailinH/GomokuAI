#ifndef GOMOKU_AHO_CORASICK_H_
#define GOMOKU_AHO_CORASICK_H_
#include "../include/Pattern.h"
#include <set>

namespace Gomoku {

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

class AhoCorasickBuilder {
public:
    // ���ڹ���˫�������ʱ���
    struct Node {
        int code, depth, first; mutable int last; // last������Ƚϣ��ʿ�����Ϊmutable��
        
        // Ĭ�Ϲ���ʱ���ɸ����(0, 0, 0, 0)
        Node(int code = 0, int depth = 0, int first = 0, int last = -1)
            : code(code), depth(depth), first(first), last(last == -1 ? first: last) { }

        // ����λ�����ַ��������
        Node(char ch, int depth = 0, int first = 0, int last = -1)
            : code(EncodeCharset(ch)), depth(depth), first(first), last(last == -1 ? first: last) { }
        
        bool operator<(const Node& other) const { // ����depth->first�ֵ���Ƚϡ�
            return std::tie(depth, first) < std::tie(other.depth, other.first);
        }

        bool operator==(const Node& other) const {
            return std::tie(code, depth, first, last) == std::tie(other.code, other.depth, other.first, other.last);
        }
    };

    using NodeIter = std::set<Node>::iterator;

public:
    AhoCorasickBuilder(std::initializer_list<Pattern> protos);

    void build(PatternSearch* searcher);

public:
    // ���ԳƵ�pattern����������ԭpattern�ȼ�
    void reverseAugment();

    // pattern�ĺڰ���ɫ�Ե�����ԭpattern�ȼ�
    void flipAugment();

    // ���߽��ס��pattern�뱻���ֶ�סһ�ǵ�pattern�ȼ�
    void boundaryAugment();

    // ���ݱ����ֵ�������patterns
    void sortPatterns();

    // DFS�������ɻ��ڽ���Trie��
    void buildNodeBasedTrie();

    // DFS��������˫����Trie��
    void buildDAT(PatternSearch* ps);

    // BFS������ΪDAT����AC�Զ�����failָ������
    void buildACGraph(PatternSearch* ps);

private:
    std::pair<NodeIter, NodeIter> children(NodeIter node) {
        auto first = m_tree.lower_bound({ 0, node->depth + 1, node->first }); // �ӽڵ��½磨no less than��
        auto last = m_tree.upper_bound({ 0, node->depth + 1, node->last - 1 }); // �ӽڵ��Ͻ磨greater than��
        return { first, last };
    }

private:
    std::set<Node> m_tree; // �����ڲ����б��������RB������Ϊ��ʱ����Trie���Ľṹ
    std::vector<Pattern> m_patterns;
};

}
#endif // !GOMOKU_AHO_CORASICK_H_
