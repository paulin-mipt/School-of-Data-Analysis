#include <algorithm>
#include <cstring>
#include <deque>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
#include <fstream>

//  std::make_unique will be available since c++14
//  Implementation was taken from http://herbsutter.com/gotw/_102/
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args &&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template <class Iterator>
class IteratorRange {
 public:
  IteratorRange(Iterator begin, Iterator end) : begin_(begin), end_(end) {}

  Iterator begin() const { return begin_; }
  Iterator end() const { return end_; }

 private:
  Iterator begin_, end_;
};

namespace traverses {

template <class Vertex, class Graph, class Visitor>
void BreadthFirstSearch(Vertex origin_vertex, const Graph &graph,
                        Visitor visitor) {
  std::queue<Vertex> vertex_queue;
  vertex_queue.push(origin_vertex);
  while (!vertex_queue.empty()) {
    Vertex vertex = vertex_queue.front();
    vertex_queue.pop();
    visitor.ExamineVertex(vertex);
    for (const auto& edge : OutgoingEdges(graph, vertex)) {
      visitor.ExamineEdge(edge);
      visitor.DiscoverVertex(GetTarget(graph, edge));
      vertex_queue.push(GetTarget(graph, edge));
    }
  }
}

// See "Visitor Event Points" on
// http://www.boost.org/doc/libs/1_57_0/libs/graph/doc/breadth_first_search.html
template <class Vertex, class Edge>
class BfsVisitor {
 public:
  virtual void DiscoverVertex(Vertex /*vertex*/) {}
  virtual void ExamineEdge(const Edge & /*edge*/) {}
  virtual void ExamineVertex(Vertex /*vertex*/) {}
  virtual ~BfsVisitor() = default;
};

}  // namespace traverses

namespace aho_corasick {

struct AutomatonNode {
  AutomatonNode() : terminated(false), suffix_link(nullptr), terminal_link(nullptr),
                    dp(1001, -1)  {}

  // is there strings which are ended at this node
  bool terminated;
  // Stores tree structure of nodes
  std::map<char, AutomatonNode> trie_transitions;

  // Stores pointers to the elements of trie_transitions
  std::map<char, AutomatonNode *> automaton_transitions_cache;
  AutomatonNode *suffix_link;
  AutomatonNode *terminal_link;
  std::vector<int32_t> dp;
};

AutomatonNode *GetTrieTransition(AutomatonNode *node, char character) {
  return node->automaton_transitions_cache.count(character) ?
    node->automaton_transitions_cache[character] : nullptr;
}

// Provides constant amortized runtime
AutomatonNode *GetAutomatonTransition(AutomatonNode *node, AutomatonNode *root,
                                      char character) {
  if (node->automaton_transitions_cache[character]) {
      return node->automaton_transitions_cache[character];
  }
  while (node != root && !GetTrieTransition(node, character)) {
    node = node->suffix_link;
  }

  auto transition = GetTrieTransition(node, character);
  node->automaton_transitions_cache[character] = transition ? transition : node;
  return node->automaton_transitions_cache[character];
}

namespace internal {

class AutomatonGraph {
 public:
  struct Edge {
    Edge(AutomatonNode *source, AutomatonNode *target, char character)
        : source(source), target(target), character(character) {}

    AutomatonNode *source;
    AutomatonNode *target;
    char character;
  };
};

std::vector<typename AutomatonGraph::Edge> OutgoingEdges(
    const AutomatonGraph & /*graph*/, AutomatonNode *vertex) {
  std::vector<typename AutomatonGraph::Edge> out_edges;
  for (const auto& transition : vertex->automaton_transitions_cache) {
    out_edges.emplace_back(vertex, transition.second, transition.first);
  }
  return out_edges;
}

AutomatonNode *GetTarget(const AutomatonGraph & /*graph*/,
                         const AutomatonGraph::Edge &edge) {
  return edge.target;
}

class SuffixLinkCalculator
    : public traverses::BfsVisitor<AutomatonNode *, AutomatonGraph::Edge> {
 public:
  explicit SuffixLinkCalculator(AutomatonNode *root) : root_(root) {}

  void ExamineEdge(const AutomatonGraph::Edge &edge) override {
    AutomatonNode* current_node = edge.source->suffix_link;
    while (current_node && !current_node->automaton_transitions_cache.count(edge.character)) {
      current_node = current_node->suffix_link;
    }

    edge.target->suffix_link = current_node ?
      current_node->automaton_transitions_cache[edge.character] :
      root_;
  }

 private:
  AutomatonNode *root_;
};


class TerminalLinkCalculator
    : public traverses::BfsVisitor<AutomatonNode *, AutomatonGraph::Edge> {
 public:
  explicit TerminalLinkCalculator(AutomatonNode *root) : root_(root) {}

  void DiscoverVertex(AutomatonNode *node) override {
    if (node == root_)
      return;

    if (node->suffix_link->terminated) {
      node->terminal_link = node->suffix_link;
    } else {
      node->terminal_link = node->suffix_link->terminal_link;
    }
  }

 private:
  AutomatonNode *root_;
};

}  // namespace internal


class NodeReference {
 public:
  NodeReference() : node_(nullptr), root_(nullptr) {}

  NodeReference(AutomatonNode *node, AutomatonNode *root)
      : node_(node), root_(root) {}

  NodeReference Next(char character) const {
    return NodeReference(GetAutomatonTransition(node_, root_, character), root_);
  }

  AutomatonNode * Node() {
    return node_;
  }

  bool IsTerminal() const {
    return node_->terminated;
  }

  explicit operator bool() const { return node_ != nullptr; }

  bool operator==(NodeReference other) const {
    return node_ == other.node_;
  }

 private:
  typedef std::vector<size_t>::const_iterator TerminatedStringIterator;
  typedef IteratorRange<TerminatedStringIterator> TerminatedStringIteratorRange;

  NodeReference TerminalLink() const {
    return NodeReference(node_->terminal_link, root_);
  }

  AutomatonNode *node_;
  AutomatonNode *root_;
};

class AutomatonBuilder;

class Automaton {
 public:
  Automaton() = default;

  Automaton(const Automaton &) = delete;
  Automaton &operator=(const Automaton &) = delete;

  NodeReference Root() {
    return NodeReference(&root_, &root_);
  }

 private:
  AutomatonNode root_;

  friend class AutomatonBuilder;
};

class AutomatonBuilder {
 public:
  void Add(const std::string &string) {
    words_.push_back(string);
  }

  std::unique_ptr<Automaton> Build() {
    auto automaton = make_unique<Automaton>();
    BuildTrie(words_, automaton.get());
    BuildSuffixLinks(automaton.get());
    BuildTerminalLinks(automaton.get());
    return automaton;
  }

 private:
  static void BuildTrie(const std::vector<std::string> &words,
                        Automaton *automaton) {
    for (size_t i = 0; i < words.size(); ++i) {
      AddString(&automaton->root_, words[i]);
    }
  }

  static void AddString(AutomatonNode *root,
                        const std::string &string) {
    AutomatonNode* current_node = root;
    for (char symbol : string) {
      AutomatonNode* next_node = GetTrieTransition(current_node, symbol);
      if (next_node) {
        current_node = next_node;
      } else {
        current_node->trie_transitions[symbol] = AutomatonNode();
        current_node->automaton_transitions_cache[symbol] = &current_node->trie_transitions[symbol];
        current_node = GetTrieTransition(current_node, symbol);
      }
    }
    current_node->terminated = true;
  }

  static void BuildSuffixLinks(Automaton *automaton) {
    internal::SuffixLinkCalculator suffixLinkCalculator(&automaton->root_);
    traverses::BreadthFirstSearch(&automaton->root_,
      internal::AutomatonGraph(),
      suffixLinkCalculator);
  }

  static void BuildTerminalLinks(Automaton *automaton) {
    internal::TerminalLinkCalculator terminalLinkCalculator(&automaton->root_);
    traverses::BreadthFirstSearch(&automaton->root_,
      internal::AutomatonGraph(),
      terminalLinkCalculator);
  }

  std::vector<std::string> words_;
};

}  // namespace aho_corasick

std::string ReadString(std::istream &input_stream) {
  std::string input_string;
  input_stream >> input_string;
  return input_string;
}

constexpr int32_t kMod = 1000000007;

// dp with memoization
int64_t LazyDP(aho_corasick::AutomatonNode *node, int len, const int alpha_size,
               const std::unique_ptr<aho_corasick::Automaton> &automaton) {  
  if (node->terminated || node->terminal_link) {
    return 0;
  }
  
  if (len == 0) {
      return 1;
  }
    
  if (node->dp[len] != -1) {
    return node->dp[len];
  }
  
  
  int32_t result = 0;
  for (int i = 0; i < alpha_size; ++i) {
    result += LazyDP(aho_corasick::GetAutomatonTransition(node,
                     automaton->Root().Node(),
                     'a' + i),
                     len - 1, alpha_size, automaton);
    result %= kMod;
  }
  
  node->dp[len] = result;
  return result;
}

int64_t CountOkStrings(std::vector<std::string> &patterns, int num, int alpha_size) {
  aho_corasick::AutomatonBuilder builder;

  for (size_t i = 0; i < patterns.size(); ++i) {
    builder.Add(patterns[i]);
  }

  std::unique_ptr<aho_corasick::Automaton> automaton;
  automaton = std::move(builder.Build());

  return LazyDP(automaton->Root().Node(), num, alpha_size, automaton);
}


int main() {
    int num, str_num, alpha_size;
    std::ifstream input_data;
    input_data.open("B.in");
    
    std::ofstream out_data;
    out_data.open("B.out");
    
    // std::cin >> num >> str_num >> alpha_size;
    input_data >> num >> str_num >> alpha_size;
    std::vector<std::string> patterns(str_num);
    for (int i = 0; i < str_num; ++i) {
        patterns[i] = ReadString(input_data);
    }
    
    out_data << CountOkStrings(patterns, num, alpha_size);
}
