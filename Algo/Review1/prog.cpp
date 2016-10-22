#include <algorithm>
#include <cstring>
#include <deque>
#include <iostream>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

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
                        Visitor visitor)
{
    std::unordered_set<Vertex> gray;
    std::unordered_set<Vertex> black;
    std::queue<typename std::unordered_set<Vertex>::iterator> queue;
    queue.push(gray.insert(origin_vertex).first);
    while (!queue.empty()) {
        auto vertex_iterator = queue.front();
        visitor.ExamineVertex(*vertex_iterator);
        auto edges = OutgoingEdges(graph, *vertex_iterator);
        for (auto& edge: edges) {
            auto target = GetTarget(graph, edge);
            if (gray.find(target) == gray.end() && black.find(target) == black.end()) {
                visitor.DiscoverVertex(target);
                queue.push(gray.insert(target).first);
            }
            visitor.ExamineEdge(edge);
        }
        queue.pop();
        black.insert(*vertex_iterator);
        gray.erase(vertex_iterator);
    }
}

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
    auto direct_transition = node->trie_transitions.find(character);
    return (direct_transition != node->trie_transitions.end()) ?
           &direct_transition->second :
           nullptr;
}

// Provides constant amortized runtime
AutomatonNode *GetAutomatonTransition(AutomatonNode *node, AutomatonNode *root,
                                      char character)
{
    auto& result = node->automaton_transitions_cache[character];
    if (result != nullptr) {
        return result;
    } else {
        auto direct_transition = GetTrieTransition(node, character);
        if (direct_transition != nullptr) {
            return result = direct_transition;
        } else {
            return result = (node != root) ?
                            GetAutomatonTransition(node->suffix_link, root, character) :
                            root;
        }
    }
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
    const AutomatonGraph & /*graph*/, AutomatonNode *vertex)
{
    std::vector<typename AutomatonGraph::Edge> result;
    for (auto& pair: vertex->trie_transitions) {
        result.emplace_back(vertex, &pair.second, pair.first);
    }
    return result;
}

AutomatonNode *GetTarget(const AutomatonGraph & /*graph*/,
                         const AutomatonGraph::Edge &edge)
{
    return edge.target;
}

class SuffixLinkCalculator
    : public traverses::BfsVisitor<AutomatonNode *, AutomatonGraph::Edge> {
 public:
  explicit SuffixLinkCalculator(AutomatonNode *root) : root_(root) {}

  void ExamineVertex(AutomatonNode *node) override {
    if (node->suffix_link == nullptr) {
        node->suffix_link = root_;
    }
  }

  void ExamineEdge(const AutomatonGraph::Edge &edge) override {
    auto& suffix_link = edge.target->suffix_link = edge.source->suffix_link;
    if (edge.source == root_) {
        return;
    }
    for (size_t root_counter = 0; root_counter < 2; 
         suffix_link = suffix_link->suffix_link, root_counter += (suffix_link == root_)) {
        auto candidate = GetTrieTransition(suffix_link, edge.character);
        if (candidate != nullptr) {
            suffix_link = candidate;
            return;
        }
    }
  }

 private:
  AutomatonNode *root_;
};

class TerminalLinkCalculator
    : public traverses::BfsVisitor<AutomatonNode *, AutomatonGraph::Edge> {
 public:
  explicit TerminalLinkCalculator(AutomatonNode *root) : root_(root) {}

  void DiscoverVertex(AutomatonNode *node) override {
    if (node->suffix_link->terminated_string_ids.empty()) {
        node->terminal_link = node->suffix_link->terminal_link;
    } else {
        node->terminal_link = node->suffix_link;
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
    return NodeReference(GetAutomatonTransition(node_, root_, character), 
                         root_);
  }

  template <class Callback>
  void GenerateMatches(Callback on_match) const {
    for (NodeReference node = *this; node; node = node.TerminalLink()) {
        for (auto id: node.TerminatedStringIds()) {
            on_match(id);
        }
    }
  }

  bool IsTerminal() const {
    return *this && !node_->terminated_string_ids.empty();
  }

  explicit operator bool() const { return node_ != nullptr; }

  // будем считать, что сравниваются всё-таки ссылки, а не узлы
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
    return {node_->terminated_string_ids.cbegin(),
            node_->terminated_string_ids.cend()};
  }

  AutomatonNode *node_;
  AutomatonNode *root_;
};

class NodeReferenceCounter {
  public:
    typedef std::map<NodeReference, std::shared_ptr<NodeReferenceCounter>> automaton_type;
    typedef  
        std::shared_ptr<automaton_type>
        automaton_type_pointer;

    NodeReferenceCounter(NodeReference node, 
            size_t max_string_length, size_t alphabet_size, size_t modulo,
            automaton_type_pointer automaton) :
        node_(node), 
        max_string_length_(max_string_length), 
        alphabet_size_(alphabet_size),
        modulo_(modulo),
        automaton_(automaton)
    {
        dynamic_function_ = std::vector<size_t>(max_string_length + 1);
        dynamic_function_[0] = 1;
        auto dp_zero = dynamic_function_.begin();
        node_.GenerateMatches(
                [dp_zero](size_t id) {
                    *dp_zero = id * 0;
                    return;
                });
    }
   
    size_t Count(size_t residual) {
        if (dynamic_function_[0] == 0) {
            return 0;
        }
        if (dynamic_function_[residual] != 0) {
            return dynamic_function_[residual];
        }
        for (char character = 'a'; character < 'a' + alphabet_size_; ++character) {
            NodeReference next = node_.Next(character);
            if (automaton_->find(next) == automaton_->end()) {
                automaton_->emplace(
                    next, 
                    std::make_shared<NodeReferenceCounter>(
                        next, max_string_length_, alphabet_size_, modulo_, automaton_));
            }
            dynamic_function_[residual] += 
                automaton_->at(node_.Next(character))->Count(residual - 1);
            dynamic_function_[residual] %= modulo_;
        }
        return dynamic_function_[residual];
    }

  private:
    NodeReference node_;
    size_t max_string_length_;
    char alphabet_size_;
    size_t modulo_;
    std::vector<size_t> dynamic_function_;
    automaton_type_pointer automaton_;
};

class AutomatonBuilder;

class Automaton {
 public:
  Automaton() = default;

  Automaton(const Automaton &) = delete;
  Automaton &operator=(const Automaton &) = delete;

  NodeReference Root() {
    return {&root_, &root_};
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
                        const std::string &string)
  {
    auto node = root;  
    for (char character: string) {
        node = &(node->trie_transitions[character]);
    }
    node->terminated_string_ids.push_back(string_id);
  }

  static void BuildSuffixLinks(Automaton *automaton) {
      traverses::BreadthFirstSearch(&automaton->root_, 
                                    internal::AutomatonGraph(), 
                                    internal::SuffixLinkCalculator(&automaton->root_));
  }

  static void BuildTerminalLinks(Automaton *automaton) {
      traverses::BreadthFirstSearch(&automaton->root_, 
                                    internal::AutomatonGraph(), 
                                    internal::TerminalLinkCalculator(&automaton->root_));
  }

  std::vector<std::string> words_;
  std::vector<size_t> ids_;
};

}  // namespace aho_corasick


std::string ReadString(std::istream &input_stream) {
    std::string result;
    std::getline(input_stream, result);
    return result;
}

size_t FindNumberOfStringsThatDontIncludeProhibited(
        size_t string_length, 
        size_t alphabet_size,
        const std::vector<std::string>&  prohibited_strings,
        size_t modulo) {
    aho_corasick::AutomatonBuilder builder;
    for (size_t index = 0; index < prohibited_strings.size(); ++index) {
        builder.Add(prohibited_strings[index], index);
    }
    auto automaton = builder.Build();
    aho_corasick::NodeReferenceCounter counter(
            automaton->Root(), 
            string_length, alphabet_size, modulo,
            std::make_shared<aho_corasick::NodeReferenceCounter::automaton_type>());
    return counter.Count(string_length);
}

void Print(std::ostream &output_stream, const size_t number) {
    output_stream << number;
}

int main() {
  std::ifstream input_stream("B.in");
  std::ofstream output_stream("B.out");

  const size_t modulo = 1000000007;

  size_t string_length;
  input_stream >> string_length;
  size_t number_of_prohibited_strings;
  input_stream >> number_of_prohibited_strings;
  size_t alphabet_size;
  input_stream >> alphabet_size;
  ReadString(input_stream);

  std::vector<std::string> prohibited_strings;
  prohibited_strings.reserve(number_of_prohibited_strings); 
  for (size_t index = 0; index < number_of_prohibited_strings; ++index) {
      auto string = ReadString(input_stream);
      prohibited_strings.push_back(string);
  }
  Print(output_stream, 
        FindNumberOfStringsThatDontIncludeProhibited(string_length, alphabet_size, 
                                                    prohibited_strings, modulo));
  return 0;
}


