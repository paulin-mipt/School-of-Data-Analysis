#include <iostream>
#include <iterator>
#include <numeric>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

template <class Iterator>
class IteratorRange {
 public:
  IteratorRange(Iterator begin, Iterator end) : begin_(begin), end_(end) {}

  Iterator begin() const { return begin_; }
  Iterator end() const { return end_; }

 private:
  Iterator begin_, end_;
};


template <class Vertex, class Graph>
class DepthFirstSearcher {
 public:
  explicit DepthFirstSearcher(const Graph& graph)
      : graph_(graph) {}

  bool IsVertexBlack(const Vertex& vertex) const;
  bool IsVertexGrey(const Vertex& vertex) const;
  std::unordered_set<Vertex> GetVisitedVertices();

  template <class Visitor>
  void operator() (const Vertex& origin_vertex, Visitor& visitor);

 private:
  enum class VertexStatus { kGrey, kBlack };

  const Graph& graph_;
  std::unordered_map<Vertex, VertexStatus> vertices_status_;

  template <class Visitor>
  void Search(const Vertex& origin_vertex, Visitor& visitor);
};

template <class Graph>
class DfsVisitor {
 public:
  using Vertex = typename Graph::Vertex;
  using Edge = typename Graph::Edge;

  virtual void DiscoverVertex(const Vertex& /*vertex*/) {}
  virtual void FinishVertex(const Vertex& /*vertex*/) {}
  virtual void ForwardOrCrossEdge(const Edge& /*edge*/) {}
  virtual void BackEdge(const Edge& /*edge*/) {}
  virtual void InitializeComponent() {};
  virtual ~DfsVisitor() = default;
};

template <class Graph, class Visitor, class VertexIterator>
void DisjointDepthFirstSearch(const Graph& graph, Visitor& visitor,
                              VertexIterator order_begin, VertexIterator order_end);

template <class Graph, class Visitor>
void DisjointDepthFirstSearch(const Graph& graph, Visitor& visitor);

class Graph {
 public:
  using Vertex = int;
  
  class Edge {
   public:
    Edge(Vertex source, Vertex target) : source_(source), target_(target) {}
    Vertex GetTarget() const { return target_; }
    Vertex GetSource() const { return source_; }
   private:
    Vertex source_;
    Vertex target_;
  };

  using EdgeIterator = std::vector<Edge>::const_iterator;

  using VertexIterator = std::vector<Vertex>::const_iterator;
  using VertexIteratorRange = IteratorRange<VertexIterator>;

  using VertexReverseIterator = std::vector<Vertex>::reverse_iterator;
  using VertexReverseIteratorRange = IteratorRange<VertexReverseIterator>;

  using VertexMapping = std::unordered_map<Vertex, Vertex>;
  using VertexSets = std::vector<std::unordered_set<Vertex>>;

  explicit Graph(int vertices_count) : adjacency_lists_(vertices_count) {};
  Graph(int vertices_count, EdgeIterator edges_begin, EdgeIterator edges_end);

  void AddEdge(const Graph::Edge& egde);
  VertexIteratorRange GetAdjacentVertices(Vertex vertex) const;
  int GetVerticesCount() const { return adjacency_lists_.size(); }

 private:
  std::vector<std::vector<Vertex>> adjacency_lists_;
};


struct Game {
  Game(int winner, int loser) : winner_id(winner), loser_id(loser) {}

  operator Graph::Edge() const { return { winner_id, loser_id }; }
  int winner_id;
  int loser_id;
};


template <class Graph>
class TopologicalSorter : public DfsVisitor<Graph> {
 public:
  TopologicalSorter() : order_(0), has_order_(true) {}

  using Vertex = typename Graph::Vertex;
  using Edge = typename Graph::Edge;

  void FinishVertex(const Vertex& vertex) override { order_.push_back(vertex); }
  void BackEdge(const Edge& /*edge*/) override { has_order_ = false; }

  const std::vector<Vertex>& GetOrder() { return order_; }
  bool HasOrder() const { return has_order_; }

 private:
  std::vector<Vertex> order_;
  bool has_order_;
};


template <class Graph>
class StrongConnectComponentFinder : public DfsVisitor<Graph> {
 public:
  StrongConnectComponentFinder() : current_component_id_(kNoComponents) {}

  using Vertex = typename Graph::Vertex;
  using Edge = typename Graph::Edge;

  void FinishVertex(const Vertex& vertex) override;
  void InitializeComponent() override;

  typename Graph::VertexSets GetComponentVertices() const;
  int GetComponentCount() const { return component_vertices_.size(); }

 private:
  static constexpr int kNoComponents = -1;

  int current_component_id_;
  std::vector<std::unordered_set<Vertex>> component_vertices_;
};

Graph MakeTransposedGraph(const Graph& graph);

template <class Graph>
std::vector<typename Graph::Vertex> ComputeTopologicalOrder(const Graph& graph);

template <class Graph>
Graph MakeCondensedGraph(const Graph& graph,
                         const typename Graph::VertexSets& vertices_components);

template <class Graph>
typename Graph::VertexSets FindStrongConnectivityComponents(const Graph& graph);

template <class Graph>
std::unordered_set<typename Graph::Vertex> FindSourceVertices(const Graph& graph);

template <class Graph>
int CountMinSourceComponentSize(const Graph& graph);

int FindMaxCompanySize(const int people_number, const std::vector<Game>& games);

std::vector<Game> ReadGames(std::istream& input = std::cin);
int ReadPeopleNumber(std::istream& input = std::cin);

void WriteMaxCompanySize(int max_size, std::ostream& output = std::cout);

int main() {
  const int people_number = ReadPeopleNumber();
  const auto games = ReadGames();
  WriteMaxCompanySize(FindMaxCompanySize(people_number, games));
  return 0;
}

// code section begin

template<class Vertex, class Graph>
template<class Visitor>
void DepthFirstSearcher<Vertex, Graph>::Search(
    const Vertex& origin_vertex, 
    Visitor& visitor) {
  vertices_status_[origin_vertex] = VertexStatus::kGrey;
  for (const auto& vertex : graph_.GetAdjacentVertices(origin_vertex)) {
    typename Graph::Edge edge(origin_vertex, vertex);
    if (!IsVertexBlack(vertex)) {
      if (!IsVertexGrey(vertex)) {
        visitor.DiscoverVertex(vertex);
        Search(vertex, visitor);
      } else {
        visitor.BackEdge(edge);
      }
    } else {
      visitor.ForwardOrCrossEdge(edge);
    }
  }
  visitor.FinishVertex(origin_vertex);
  vertices_status_[origin_vertex] = VertexStatus::kBlack;
}

template<class Vertex, class Graph>
template <class Visitor>
void DepthFirstSearcher<Vertex, Graph>::operator()(
    const Vertex& origin_vertex,
    Visitor& visitor) {
  if (!IsVertexBlack(origin_vertex)) {
    visitor.DiscoverVertex(origin_vertex);
    Search(origin_vertex, visitor);
  }
}

template<class Vertex, class Graph>
bool DepthFirstSearcher<Vertex, Graph>::IsVertexGrey(const Vertex& vertex) const {
  const auto status = vertices_status_.find(vertex);
  return (status != vertices_status_.end() && status->second == VertexStatus::kGrey);
}

template<class Vertex, class Graph>
std::unordered_set<Vertex> DepthFirstSearcher<Vertex, Graph>::GetVisitedVertices() {
  std::unordered_set<Vertex> black_vertices;
  for (const auto& pair : vertices_status_) {
    if (pair.second == VertexStatus::kBlack) {
      black_vertices.insert(pair.first);
    }
  }
  return black_vertices;
}

template<class Vertex, class Graph>
bool DepthFirstSearcher<Vertex, Graph>::IsVertexBlack(const Vertex& vertex) const {
  const auto status = vertices_status_.find(vertex);
  return (status != vertices_status_.end() && status->second == VertexStatus::kBlack);
}

Graph::Graph(int vertices_count, EdgeIterator edges_begin, EdgeIterator edges_end) :
    adjacency_lists_(vertices_count) {
  for (auto it = edges_begin; it != edges_end; ++it) {
    adjacency_lists_[it->GetSource()].push_back(it->GetTarget());
  }
}

void Graph::AddEdge(const Graph::Edge& edge) {
  adjacency_lists_[edge.GetSource()].push_back(edge.GetTarget());
}

Graph::VertexIteratorRange Graph::GetAdjacentVertices(Vertex vertex) const {
  return {adjacency_lists_[vertex].begin(), adjacency_lists_[vertex].end()};
}

Graph MakeTransposedGraph(const Graph& graph) {
  std::vector<Graph::Edge> reversed_edges;
  reversed_edges.reserve(graph.GetVerticesCount());
  for (int source = 0; source < graph.GetVerticesCount(); ++source) {
    for (const auto& target : graph.GetAdjacentVertices(source)) {
      reversed_edges.emplace_back(target, source);
    }
  }

  return Graph(graph.GetVerticesCount(), reversed_edges.begin(), reversed_edges.end());
}

template<class Graph>
void StrongConnectComponentFinder<Graph>::InitializeComponent() {
  ++current_component_id_;
  component_vertices_.emplace_back();
}

template<class Graph>
typename Graph::VertexSets StrongConnectComponentFinder<Graph>::GetComponentVertices() const {
  return component_vertices_;
}

template <class Graph>
void StrongConnectComponentFinder<Graph>::FinishVertex(const Vertex& vertex) {
  component_vertices_[current_component_id_].insert(vertex);
}

int FindMaxCompanySize(const int people_number, const std::vector<Game>& games) {
  std::vector<Graph::Edge> edges(games.begin(), games.end());
  const auto graph = Graph(people_number, edges.begin(), edges.end());

  const int min_component_size = CountMinSourceComponentSize(graph);
  const int max_number_of_coworkers = people_number - min_component_size + 1;
  return max_number_of_coworkers;
}

std::vector<Game> ReadGames(std::istream& input) {
  static const int kWin = 1;
  static const int kLose = 2;

  std::vector<Game> games;
  int games_number;
  input >> games_number;
  games.reserve(games_number);

  for (int i = 0; i < games_number; ++i) {
    int first, second, result;
    input >> first >> second >> result;
    --first;
    --second;
    if (result == kWin) {
      games.emplace_back(first, second);
    } else if (result == kLose) {
      games.emplace_back(second, first);
    }
  }

  return games;
}

int ReadPeopleNumber(std::istream& input) {
  int people_number;
  input >> people_number;
  return people_number;
}

void WriteMaxCompanySize(int max_size, std::ostream& output) {
  output << max_size << std::endl;
}

template <class Graph>
std::vector<typename Graph::Vertex> ComputeTopologicalOrder(const Graph& graph) {
  TopologicalSorter<Graph> order_calculator;

  DisjointDepthFirstSearch(graph, order_calculator);
  return order_calculator.GetOrder();
}

template <class Graph>
typename Graph::VertexSets FindStrongConnectivityComponents(const Graph& graph) {
  std::vector<typename Graph::Vertex> ordered_vertices = ComputeTopologicalOrder(graph);
  StrongConnectComponentFinder<Graph> component_finder;

  const auto transposed_graph = MakeTransposedGraph(graph);
  DisjointDepthFirstSearch(std::move(transposed_graph), component_finder,
                           ordered_vertices.rbegin(), ordered_vertices.rend());

  const int components_number = component_finder.GetComponentCount();
  return component_finder.GetComponentVertices();
}

template <class Graph>
Graph MakeCondensedGraph(const Graph& graph,
                         const typename Graph::VertexSets& vertices_components) {
  typename Graph::VertexMapping component_mapping;
  Graph condensation_graph(vertices_components.size());

  for (typename Graph::Vertex i = 0; i < vertices_components.size(); ++i) {
    for (const auto& component_vertex : vertices_components[i]) {
      component_mapping[component_vertex] = i;
    }
  }

  for (int component_id = 0; component_id < vertices_components.size(); ++component_id) {
    std::unordered_set<typename Graph::Vertex> adjacent_components_id;
    for (const auto& component_vertex : vertices_components[component_id]) {
      for (const auto& adjacent_vertex : graph.GetAdjacentVertices(component_vertex)) {
        const auto vertex_component = component_mapping[adjacent_vertex];
        if (adjacent_components_id.find(vertex_component) == adjacent_components_id.end() &&
            component_mapping[adjacent_vertex] != component_id) {
          condensation_graph.AddEdge({ component_id, component_mapping[adjacent_vertex] });
          adjacent_components_id.insert(component_mapping[adjacent_vertex]);
        }
      }
    }
  }

  return condensation_graph;
}

template <class Graph>
std::unordered_set<typename Graph::Vertex> FindSourceVertices(const Graph& graph) {
  std::vector<bool> is_source(graph.GetVerticesCount(), true);
  for (typename Graph::Vertex i = 0; i < graph.GetVerticesCount(); ++i) {
    for (const auto& vertex : graph.GetAdjacentVertices(i)) {
      is_source[vertex] = false;
    }
  }

  std::unordered_set<typename Graph::Vertex> source_vertices;
  for (typename Graph::Vertex i = 0; i < graph.GetVerticesCount(); ++i) {
    if (is_source[i]) {
      source_vertices.insert(i);
    }
  }

  return source_vertices;
}

template <class Graph>
int CountMinSourceComponentSize(const Graph& graph) {
  const typename Graph::VertexSets components = FindStrongConnectivityComponents(graph);
  const Graph condensation_graph = MakeCondensedGraph(graph, components);

  int min_size = graph.GetVerticesCount();
  for (const auto& source : FindSourceVertices(condensation_graph)) {
    if (components[source].size() < min_size) {
      min_size = components[source].size();
    }
  }
  return min_size;
}


template <class Graph, class Visitor, class VertexIterator>
void DisjointDepthFirstSearch(const Graph& graph, Visitor& visitor,
                              VertexIterator order_begin, VertexIterator order_end) {
  DepthFirstSearcher<typename Graph::Vertex, Graph> dfs(graph);
  for (auto it = order_begin; it != order_end; ++it) {
    if (!dfs.IsVertexBlack(*it)) {
      visitor.InitializeComponent();
      dfs(*it, visitor);
    }
  }
}

template <class Graph, class Visitor>
void DisjointDepthFirstSearch(const Graph& graph, Visitor& visitor) {
  std::vector<typename Graph::Vertex> dfs_order(graph.GetVerticesCount());
  std::iota(dfs_order.begin(), dfs_order.end(), 0);

  DisjointDepthFirstSearch(graph, visitor, dfs_order.cbegin(), dfs_order.cend());
}

// code section end
