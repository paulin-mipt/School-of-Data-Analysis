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
    const Vertex vertex = vertex_queue.front();
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
  const auto direct_transition = node->trie_transitions.find(character);
  return (direct_transition != node->trie_transitions.end()) ?
          &direct_transition->second :
          nullptr;
}

// Provides constant amortized runtime
AutomatonNode *GetAutomatonTransition(AutomatonNode *node, AutomatonNode *root,
                                      char character) {
  auto &result = node->automaton_transitions_cache[character];
  if (result != nullptr) {
    return result;
  } 

  const auto direct_transition = GetTrieTransition(node, character);
  if (direct_transition != nullptr) {
    result = direct_transition;
  } else {
    result = (node != root) ?
        GetAutomatonTransition(node->suffix_link, root, character) :
        root;
  }
  return result;
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
  for (auto &transition : vertex->trie_transitions) {
    out_edges.emplace_back(vertex, &transition.second, transition.first);
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
    if (node->suffix_link == nullptr) {
      node->suffix_link = root_;
    }
  }

  void ExamineEdge(const AutomatonGraph::Edge &edge) override {
    auto& current_node = edge.target->suffix_link = edge.source->suffix_link;
    if (edge.source == root_) {
      return;
    }

    current_node = GetAutomatonTransition(current_node, root_, edge.character);
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

  explicit operator bool() const { return node_ != nullptr; }

  bool operator==(NodeReference other) const {
    return node_ == other.node_ && root_ == other.root_;
  }

 private:
  typedef std::vector<size_t>::const_iterator TerminatedStringIterator;
  typedef IteratorRange<TerminatedStringIterator> TerminatedStringIteratorRange;

  NodeReference TerminalLink() const {
    return NodeReference(node_->terminal_link, root_);
  }

  TerminatedStringIteratorRange TerminatedStringIds() const {
    return {node_->terminated_string_ids.begin(), node_->terminated_string_ids.end()};
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
    auto current_node = root;
    for (const char symbol : string) {
      current_node = &(current_node->trie_transitions[symbol]);
    }
    current_node->terminated_string_ids.push_back(string_id);
  }

  static void BuildSuffixLinks(Automaton *automaton) {
    internal::SuffixLinkCalculator suffix_link_calculator(&automaton->root_);
    traverses::BreadthFirstSearch(
        &automaton->root_,
        internal::AutomatonGraph(),
        suffix_link_calculator);
  }

  static void BuildTerminalLinks(Automaton *automaton) {
    internal::TerminalLinkCalculator terminal_link_calculator(&automaton->root_);
    traverses::BreadthFirstSearch(
        &automaton->root_,
        internal::AutomatonGraph(),
        terminal_link_calculator);
  }

  std::vector<std::string> words_;
  std::vector<size_t> ids_;
};

}  // namespace aho_corasick

// Consecutive delimiters are not grouped together and are deemed
// to delimit empty strings
template <class Predicate>
std::vector<std::string> Split(const std::string &string,
                               Predicate is_delimiter) {
  std::vector<std::string> substrings;
  std::string current_string = "";
  for (const char symbol : string) {
    if (is_delimiter(symbol)) {
      substrings.push_back(std::move(current_string));
      current_string.clear();
    } else {
      current_string.push_back(symbol);
    }
  }

  if (!current_string.empty()) {
    substrings.push_back(std::move(current_string));
  }

  return substrings;
}

// Wildcard is a character that may be substituted
// for any of all possible characters
class WildcardMatcher {
 public:
  WildcardMatcher() : number_of_words_(0), pattern_length_(0) {}

  void Init(const std::string &pattern, char wildcard) {
    aho_corasick::AutomatonBuilder builder;
    const std::vector<std::string> patterns = Split(
        pattern,
        [wildcard](char symbol) -> bool {
          return symbol == wildcard;
        });

    size_t total_length = 0;
    size_t number_of_words = 0;
    for (const auto& pattern : patterns) {
      total_length += pattern.length();
      if (!pattern.empty()) {
        builder.Add(pattern, total_length);
        ++number_of_words;
      }
      ++total_length;
    }

    number_of_words_ = number_of_words;
    pattern_length_ = pattern.length();

    aho_corasick_automaton_ = std::move(builder.Build());
    Reset();
  }

  // Resets matcher to start scanning new stream
  void Reset() {
    words_occurrences_by_position_.clear();
    state_ = aho_corasick_automaton_->Root();
  }

  template <class Callback>
  void Scan(char character, Callback on_match) {
    state_ = state_.Next(character);

    UpdateWordOccurrences();

    if (words_occurrences_by_position_.size() >= pattern_length_) {
      if (words_occurrences_by_position_.front() == number_of_words_) {
        on_match();
      }
      ShiftWordOccurrencesCounters();
    }
  }

 private:
  void UpdateWordOccurrences() {
    words_occurrences_by_position_.push_back(0);
    state_.GenerateMatches(
        [this](size_t id) {
          if (words_occurrences_by_position_.size() >= id) {
            size_t index = words_occurrences_by_position_.size() - id;
            ++(this->words_occurrences_by_position_[index]);
          }
        });
  }

  void ShiftWordOccurrencesCounters() {
    words_occurrences_by_position_.pop_front();
  }

  // Storing only O(|pattern|) elements allows us
  // to consume only O(|pattern|) memory for matcher
  std::deque<size_t> words_occurrences_by_position_;
  aho_corasick::NodeReference state_;
  size_t number_of_words_;
  size_t pattern_length_;
  std::unique_ptr<aho_corasick::Automaton> aho_corasick_automaton_;
};

std::string ReadString(std::istream &input_stream) {
  std::string input_string;
  input_stream >> input_string;
  return input_string;
}

// Returns positions of the first character of every match
std::vector<size_t> FindFuzzyMatches(const std::string &pattern_with_wildcards,
                                     const std::string &text, char wildcard) {
  WildcardMatcher matcher;
  matcher.Init(pattern_with_wildcards, wildcard);
  std::vector<size_t> occurrences;
  for (size_t offset = 0; offset < text.size(); ++offset) {
    matcher.Scan(
        text[offset],
        [&occurrences, offset, &pattern_with_wildcards] {
          occurrences.push_back(offset + 1 - pattern_with_wildcards.size());
        });
  }
  return occurrences;
}


void Print(const std::vector<size_t> &sequence) {
  std::cout << sequence.size() << std::endl;

  for (const auto element : sequence) {
    std::cout << element << " ";
  }
  
  std::cout << std::endl;
}



int main(int argc, char *argv[]) {

  constexpr char kWildcard = '?';
  const std::string pattern_with_wildcards = ReadString(std::cin);
  const std::string text = ReadString(std::cin);
  Print(FindFuzzyMatches(pattern_with_wildcards, text, kWildcard));
  return 0;
}

