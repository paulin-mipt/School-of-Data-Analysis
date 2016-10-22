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
  AutomatonNode() : suffix_link(nullptr), terminal_link(nullptr) {}

  // Stores ids of strings which are ended at this node
  std::vector<size_t> terminated_string_ids;
  // Stores tree structure of nodes
  std::map<char, AutomatonNode> trie_transitions;

  // Stores pointers to the elements of trie_transitions
  std::map<char, AutomatonNode *> automaton_transitions_cache;
  AutomatonNode *suffix_link;
  AutomatonNode *terminal_link;
};

AutomatonNode *GetTrieTransition(AutomatonNode *node, char character) {
  return node->automaton_transitions_cache.count(character) ?
    node->automaton_transitions_cache[character] : nullptr;
}

// Provides constant amortized runtime
AutomatonNode *GetAutomatonTransition(AutomatonNode *node, AutomatonNode *root,
                                      char character) {
  while (node != root && !GetTrieTransition(node, character)) {
    node = node->suffix_link;
  }

  auto transition = GetTrieTransition(node, character);
  return transition ? transition : node;
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

  void ExamineVertex(AutomatonNode *node) override {
    // can't understand what to do here :(
    return;
  }

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

    if (node->suffix_link->terminated_string_ids.size() > 0) {
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

  template <class Callback>
  void GenerateMatches(Callback on_match) const {
    auto node = *this;
    while (node) {
      for (auto id : node.TerminatedStringIds()) {
        on_match(id);
      }
      node = node.TerminalLink();
    }
  }

  bool IsTerminal() const {
    return node_->terminated_string_ids.size() > 0;
  }

  explicit operator bool() const { return node_ != nullptr; }

  bool operator<(NodeReference other) const {
    return node_ < other.node_;
  }

 private:
  typedef std::vector<size_t>::const_iterator TerminatedStringIterator;
  typedef IteratorRange<TerminatedStringIterator> TerminatedStringIteratorRange;

  NodeReference TerminalLink() const {
    return NodeReference(node_->terminal_link, root_);
  }

  TerminatedStringIteratorRange TerminatedStringIds() const {
    return IteratorRange<TerminatedStringIterator>(node_->terminated_string_ids.begin(),
      node_->terminated_string_ids.end());
  }

  AutomatonNode *node_;
  AutomatonNode *root_;
};

class Solver {
public:
    typedef std::map<NodeReference, std::shared_ptr<Solver>> automaton_type;
    typedef  std::shared_ptr<automaton_type> automaton_ptr;
    
    Solver(NodeReference node, size_t num, size_t alpha_size, 
           automaton_ptr automaton) 
    : node_(node), num_(num), alpha_size_(alpha_size), automaton_(automaton) {     
            dp_ = std::vector<size_t>(num + 1);
            dp_[0] = 1;
            auto dp_zero = dp_.begin();
            node_.GenerateMatches(
                [dp_zero](size_t id) {
                    *dp_zero = id * 0;
                    return;
                });
        }
        
        size_t Calculate(size_t len) {
            if (dp_[0] == 0) {
                return 0;
            }
            if (dp_[len] != 0) {
                return dp_[len];
            }
            for (char i; i < alpha_size_; ++i) {
                NodeReference next = node_.Next('a' + i);
                if (automaton_->find(next) == automaton_->end()) {
                    automaton_->emplace(
                        next, 
                        std::make_shared<Solver>(next, num_, alpha_size_, automaton_));
                }
                dp_[len] += automaton_->at(node_.Next('a' + i))->Calculate(len - 1);
                dp_[len] %= kMod;
            }
            return dp_[len];
        }
                         
private:
    int32_t kMod = 1000000007; 
    NodeReference node_;
    size_t num_;
    char alpha_size_;
    std::vector<size_t> dp_;
    automaton_ptr automaton_;
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
  void Add(const std::string &string, size_t id) {
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
  static void BuildTrie(const std::vector<std::string> &words,
                        const std::vector<size_t> &ids, Automaton *automaton) {
    for (size_t i = 0; i < words.size(); ++i) {
      AddString(&automaton->root_, ids[i], words[i]);
    }
  }

  static void AddString(AutomatonNode *root, size_t string_id,
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
    current_node->terminated_string_ids.push_back(string_id);
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
  std::vector<size_t> ids_;
};

}  // namespace aho_corasick


std::string ReadString(std::istream &input_stream) {
  std::string input_string;
  input_stream >> input_string;
  return input_string;
}

int64_t CountOkStrings(std::vector<std::string> &patterns, int num, int alpha_size) {
    aho_corasick::AutomatonBuilder builder;
    
    for (size_t i = 0; i < patterns.size(); ++i) {
        builder.Add(patterns[i], i);
    }
    
    std::unique_ptr<aho_corasick::Automaton> automaton;
    automaton = std::move(builder.Build());
    
    aho_corasick::Solver solver(
        automaton->Root(), num, alpha_size,                                             
        std::make_shared<aho_corasick::Solver::automaton_type>());
    return solver.Calculate(num);
}

int main(int argc, char *argv[]) {
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
    return 0;
}
