#include<algorithm>
#include<deque>
#include<iostream>
#include<string>
#include<vector>
#include<queue>
#include<map>
#include<unordered_set>
#include<iterator>
#include<sstream>
#include<memory>
#include<stdexcept>
#include<utility>
#include <typeinfo>

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template<class Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator begin, Iterator end) :
    begin_(begin), end_(end) {}
    
    Iterator begin() const {
        return begin_;
    }
    
    Iterator end() const {
        return end_;
    }
private:
    Iterator begin_, end_;
};

namespace traverses {
    template<class Visitor, class Graph, class Vertex>
    void BreadthFirstSearch(Vertex origin_vertex, const Graph& graph, Visitor visitor) {
        std::queue<Vertex> queue;
        queue.push(origin_vertex);
        visitor.DiscoverVertex(origin_vertex);
        while (queue.size() != 0) {
            Vertex vertex = queue.front();
            visitor.ExamineVertex(vertex);
            queue.pop();
            std::vector<typename Graph::Edge> edges = graph.OutgoingEdges(vertex);
            for (int ind = 0; ind < edges.size(); ++ind) {
                visitor.DiscoverVertex(GetTarget(edges[ind]));
                visitor.ExamineEdge(edges[ind]);
                queue.push(GetTarget(edges[ind]));
            }
        }
    }
    
    
    template<class Vertex, class Edge>
    class BfsVisitor {
    public:
        virtual void DiscoverVertex(Vertex /*vertex*/) {}
        virtual void ExamineEdge(const Edge&) {}
        virtual void ExamineVertex(Vertex) {}
        virtual ~BfsVisitor() {}
    };
    
} // namespace traverses


namespace aho_corasick {
    
struct AutomatonNode {
    AutomatonNode() :
    suffix_link(nullptr),
    terminal_link(nullptr) {
    }
        
    std::vector<size_t> matched_string_ids;
    std::map<char, AutomatonNode> trie_transitions;
    std::map<char, AutomatonNode*> automaton_transitions;
    AutomatonNode* suffix_link;
    AutomatonNode* terminal_link;
    
    bool operator==(AutomatonNode another) {
        return another.suffix_link == suffix_link && another.terminal_link == terminal_link;
    }
};
    
AutomatonNode* GetTrieTransition(AutomatonNode* node, char character) {
    if (node->trie_transitions.count(character) != 0) {
        return &(node->trie_transitions.find(character)->second);
    }
    return nullptr;
}
    
AutomatonNode* GetNextNode(AutomatonNode* node, AutomatonNode* root, char character) {
    return node->automaton_transitions.find(character)->second;
}
    
namespace internal {
        
class AutomatonGraph;
        
class AutomatonGraph {
public:
    struct Edge {
        Edge(AutomatonNode* source,
            AutomatonNode* target,
            char character) :
            source(source),
            target(target),
            character(character) {
        }
                
        AutomatonNode* source;
        AutomatonNode* target;
        char character;
    };
            
    std::vector<Edge> OutgoingEdges(AutomatonNode* vertex) const {
        std::vector<Edge> ans;
        for (std::pair<char, AutomatonNode> pair : vertex->trie_transitions) {
                ans.push_back(Edge(vertex,
                                &(vertex->trie_transitions.find(pair.first)->second),
                                pair.first));
        }
        return ans;
    }
};
        
AutomatonNode* GetTarget(const AutomatonGraph::Edge& edge) {
    return edge.target;
}
        
class SuffixLinkCalculator :
        public traverses::BfsVisitor<AutomatonNode*, AutomatonGraph::Edge> {
public:
    explicit SuffixLinkCalculator(AutomatonNode* root) :
        root_(root) {}
    void ExamineVertex(AutomatonNode* node) {
        if (node == root_) {
            node->suffix_link = root_;
        }
        return;
    }
    void ExamineEdge(const AutomatonGraph::Edge& edge) {
        if (edge.source == root_) {
            edge.target->suffix_link = root_;
            return;
        }
        AutomatonNode* cur = edge.source->suffix_link;
        while (cur != root_ && cur->automaton_transitions.count(edge.character) == 0) {
            cur = cur->suffix_link;
        }
        if (cur->automaton_transitions.count(edge.character) != 0) {
            cur = cur->automaton_transitions.find(edge.character)->second;
            edge.target->suffix_link = cur;
            AutomatonNode* new_cur = edge.source->suffix_link;
            while (new_cur != root_ &&
                   new_cur->automaton_transitions.count(edge.character) == 0) {
                new_cur->automaton_transitions.insert(std::pair<char, AutomatonNode*>(
                                                                    edge.character, cur));
                new_cur = new_cur->suffix_link;
            }
        } else {
              AutomatonNode* new_cur = edge.source->suffix_link;
              while (new_cur != root_) {
                  new_cur->automaton_transitions.insert(std::pair<char, AutomatonNode*>(
                                                                                        edge.character, root_));
                  new_cur = new_cur->suffix_link;
              }
              edge.target->suffix_link = root_;
          }
        return;
    }
            
private:
    AutomatonNode* root_;
};
        
class TerminalLinkCalculator :
    public traverses::BfsVisitor<AutomatonNode*, AutomatonGraph::Edge> {
public:
    explicit TerminalLinkCalculator(AutomatonNode* root) :
        root_(root) {}
    void DiscoverVertex(AutomatonNode* node) {
        if (node->suffix_link->matched_string_ids.size() != 0) {
            node->terminal_link = node->suffix_link;
            return;
        }
        node->terminal_link = node->suffix_link->terminal_link;
    }
private:
    AutomatonNode* root_;
};
        
} // namespace internal
    
class NodeReference {
public:
    typedef std::vector<size_t>::const_iterator MatchedStringIterator;
    typedef IteratorRange<MatchedStringIterator> MatchedStringIteratorRange;
        
    NodeReference() :
        node_(nullptr),
        root_(nullptr) {
    }
        
    NodeReference(AutomatonNode* node, AutomatonNode* root) :
        node_(node), root_(root) {
    }
        
    NodeReference Next(char character) const {
        if (node_->automaton_transitions.count(character) != 0) {
            return NodeReference(node_->automaton_transitions.find(character)->second, root_);
        }
        AutomatonNode* tec_node = node_;
        while (tec_node != root_ && tec_node->automaton_transitions.count(character) == 0) {
            tec_node = tec_node->suffix_link;
        }
        if (tec_node->automaton_transitions.count(character) != 0) {
            tec_node = tec_node->automaton_transitions.find(character)->second;
        }
        node_->automaton_transitions.insert(std::pair<char, AutomatonNode*>(character, tec_node));
        return NodeReference(tec_node, root_);
    }
    
    NodeReference suffixLink() const {
        return NodeReference(node_->suffix_link, root_);
    }
        
    NodeReference terminalLink() const {
        return NodeReference(node_->terminal_link, root_);
    }
        
    MatchedStringIteratorRange matchedStringIds() const {
        return MatchedStringIteratorRange(node_->matched_string_ids.begin(),
                                            node_->matched_string_ids.end());
    }
        
    explicit operator bool() const {
        return node_ != nullptr;
    }
        
    bool operator==(NodeReference other) const {
        return node_ == other.node_ && root_ == other.root_;
    }

private:
    AutomatonNode* node_;
    AutomatonNode* root_;
};
    
using std::rel_ops::operator !=;
    
class AutomatonBuilder;
    
class Automaton {
public:
    Automaton() = default;
        
    NodeReference Root() {
        return NodeReference(&root_, &root_);
    }
    
    template<class Callback>
    void GenerateMatches(NodeReference node, Callback on_match) {
        NodeReference tec_node = node;
        while (tec_node) {
            NodeReference::MatchedStringIteratorRange matched_string_ids =
                                                tec_node.matchedStringIds();
            for (size_t ind : matched_string_ids) {
                on_match(ind);
            }
            tec_node = tec_node.terminalLink();
        }
    }

private:
    AutomatonNode root_;
    
    Automaton(const Automaton&) = delete;
    Automaton& operator=(const Automaton&) = delete;
        
    friend class AutomatonBuilder;
};
    
class AutomatonBuilder {
public:
    void Add(const std::string& string, size_t id) {
        words_.push_back(string);
        ids_.push_back(id);
    }
        
    std::unique_ptr<Automaton> Build() {
        auto automaton = make_unique<Automaton>();
        BuildTrie(words_, ids_, automaton.get());
        BuildSuffixLinks(automaton.get());
        BuildTerminalLinks(automaton.get());
        return automaton;
    }
        
private:
    static void BuildTrie(const std::vector<std::string>& words,
                            const std::vector<size_t>& ids,
                            Automaton* automaton) {
        for (size_t i = 0; i < words.size(); ++i) {
            AddString(automaton->root_, ids[i], words[i]);
        }
    }
        
    static void AddString(AutomatonNode& root, size_t string_id, const std::string& string) {
        AutomatonNode* cur_state = &root;
        for (int ind = 0; ind < string.size(); ++ind) {
            if (cur_state->trie_transitions.count(string[ind]) != 0) {
                cur_state = &cur_state->trie_transitions.find(string[ind])->second;
            } else {
                  AutomatonNode new_state;
                  cur_state->trie_transitions.insert
                                    (std::pair<char, AutomatonNode>(string[ind], new_state));
                  AutomatonNode* new_cur_state = &cur_state->
                                                  trie_transitions.find(string[ind])->second;
                  cur_state->automaton_transitions.insert
                                (std::pair<char, AutomatonNode*>(string[ind], new_cur_state));
                  cur_state = new_cur_state;
              }
            }
            cur_state->matched_string_ids.push_back(string_id);
    }
        
    static void BuildSuffixLinks(Automaton* automaton) {
        aho_corasick::internal::SuffixLinkCalculator suff_link_calc(&automaton->root_);
        traverses::BreadthFirstSearch<aho_corasick::internal::SuffixLinkCalculator,
            aho_corasick::internal::AutomatonGraph,
            aho_corasick::AutomatonNode*>(&automaton->root_, internal::AutomatonGraph(),
                                                                          suff_link_calc);
    }
        
    static void BuildTerminalLinks(Automaton* automaton) {
        aho_corasick::internal::TerminalLinkCalculator termi_link_calc(&automaton->root_);
        traverses::BreadthFirstSearch<aho_corasick::internal::TerminalLinkCalculator,
               aho_corasick::internal::AutomatonGraph,
               aho_corasick::AutomatonNode*>(&automaton->root_, internal::AutomatonGraph(),
                                                                          termi_link_calc);
    }
        
    std::vector<std::string> words_;
    std::vector<size_t> ids_;
};
    
} // namespace aho_corasick

template<class Predicate>
std::vector<std::string> Split(const std::string& string, Predicate is_delimiter) {
    std::vector<std::string> ans;
    std::string cur;
    bool is_wildcards = true;
    if (string[0] != is_delimiter) {
        is_wildcards = false;
    }
    for (int ind = 0; ind < string.size(); ++ind) {
        if (string[ind] != is_delimiter && is_wildcards == false) {
            cur.push_back(string[ind]);
            continue;
        }
        if (string[ind] != is_delimiter && is_wildcards == true) {
            ans.push_back(cur);
            cur.clear();
            cur.push_back(string[ind]);
            is_wildcards = false;
            continue;
        }
        if (string[ind] == is_delimiter && is_wildcards == true) {
            cur.push_back(string[ind]);
            continue;
        }
        if (string[ind] == is_delimiter && is_wildcards == false) {
            ans.push_back(cur);
            cur.clear();
            cur.push_back(string[ind]);
            is_wildcards = true;
        }
    }
    if (cur.size() != 0) {
        ans.push_back(cur);
    }
    return ans;
}

class WildCardMatcher {
public:
    WildCardMatcher() :
    number_of_words_(0),
    pattern_length_(0) {
    }
    
    void Init(const std::string& pattern, char wildcard) {
        std::vector<std::string> splitted_pattern = Split(pattern, wildcard);
        aho_corasick::AutomatonBuilder automaton_builder;
        int tec_length = 0;
        for (int ind = 0; ind < splitted_pattern.size(); ++ind) {
            tec_length += splitted_pattern[ind].size();
            if (splitted_pattern[ind][0] != '?') {
                number_of_words_ += 1;
                automaton_builder.Add(splitted_pattern[ind], tec_length);
            }
        }
        aho_corasick_automaton_ = automaton_builder.Build();
        state_ = aho_corasick_automaton_->Root();
        pattern_length_ = pattern.size();
    }
    
    void Reset() {
        state_ = aho_corasick_automaton_->Root();
        words_occurence_by_position.clear();
    }
    
    template<class Callback>
    void Scan(char character, Callback on_match) {
        state_ = state_.Next(character);
        if (words_occurence_by_position.size() == pattern_length_) {
            words_occurence_by_position.pop_front();
        }
        words_occurence_by_position.push_back(0);
        aho_corasick_automaton_->GenerateMatches(state_, [this](int num) {
            if (words_occurence_by_position.size() >= num) {
                words_occurence_by_position[words_occurence_by_position.size() - num] += 1;
            }
        });
        if (words_occurence_by_position.size() == pattern_length_ &&
                                     words_occurence_by_position[0] == number_of_words_) {
            on_match();
        }
    }

private:
    std::deque<size_t> words_occurence_by_position;
    aho_corasick::NodeReference state_;
    size_t number_of_words_;
    size_t pattern_length_;
    std::unique_ptr<aho_corasick::Automaton> aho_corasick_automaton_;
};

std::string ReadString(std::istream& input_stream) {
    std::string ans;
    input_stream >> ans;
    return ans;
}

std::vector<size_t> FindFuzzyMatches(const std::string& patternWithWildcards,
                                     const std::string& text,
                                     char wildcard) {
    WildCardMatcher matcher;
    matcher.Init(patternWithWildcards, wildcard);
    std::vector<size_t> occurences;
    for (size_t offset = 0; offset < text.size(); ++offset) {
        matcher.Scan(text[offset],
                     [&occurences, offset, &patternWithWildcards] {
                         occurences.push_back(offset + 1 - patternWithWildcards.size());
                     });
    }
    return occurences;
}

void Print(const std::vector<size_t>& sequence) {
    std::cout << sequence.size() << std::endl;
    for (int ind = 0; ind < sequence.size(); ++ind) {
        std::cout << sequence[ind] << " ";
    }
    std::cout << std::endl;
}

void TestAll();

int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    const char wildcard = '?';
    const std::string patternWithWildcards = ReadString(std::cin);
    const std::string text = ReadString(std::cin);
    Print(FindFuzzyMatches(patternWithWildcards, text, wildcard));
    return 0;
}
