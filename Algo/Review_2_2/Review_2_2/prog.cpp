#include <algorithm>
#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <unordered_set>
#include <tuple>

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

template <class Vertex, class Graph>
class DepthFirstSearch {
  public:
    explicit DepthFirstSearch(const Graph& graph) : graph_(graph) {}

    bool IsBlack(Vertex vertex) const;
    bool IsGray(Vertex vertex) const;

    template <class Visitor>
    void operator() (const Vertex& origin_vertex, Visitor& visitor);

  private:
    const Graph& graph_;
    std::unordered_set<Vertex> black_vertices_;
    std::unordered_set<Vertex> gray_vertices_;

    template <class Visitor>
    void Search(const Vertex& origin_vertex, Visitor& visitor);
};

template <class Vertex, class Edge>
class DfsVisitor {
 public:
  virtual void DiscoverVertex(Vertex /*vertex*/) {}
  virtual void FinishVertex(Vertex /*vertex*/) {}
  virtual void ForwardOrCrossEdge(Edge /*edge*/) {}
  virtual void BackEdge(Edge /*edge*/) {}
  virtual ~DfsVisitor() = default;
};

}  // namespace traverses

namespace graphs {

class Edge {
  public:
    typedef size_t Vertex;

    Edge(Vertex source, Vertex target) : source_(source), target_(target) {}
        
    Edge Reverse() const { return Edge(target_, source_); }

    Vertex GetSource() const { return source_; }
    Vertex GetTarget() const { return target_; }

  private:
    Vertex source_;
    Vertex target_;
};

class Graph {
  public:
    typedef size_t Vertex;
    typedef graphs::Edge Edge;
    typedef std::vector<Edge> Edges;

    Graph(size_t number_of_vertices, Edges edges);

    IteratorRange<Edges::const_iterator> OutgoingEdges(Vertex vertex) const;

    Graph Reverse() const;

    size_t GetNumberOfVertices() const;

  private:
    size_t number_of_vertices_;
    Edges edges_;
    // begins of continuous segments of outgoing edges for vertices
    std::vector<Edges::const_iterator> begins_; 
};

} // namespace graphs

template <class Vertex, class Edge>
class TopoOrderCalculator
    : public traverses::DfsVisitor<Vertex, Edge> {
  public:    
    TopoOrderCalculator() : order_(0), has_order_(true) {}

    void FinishVertex(Vertex vertex) override { order_.push_back(vertex); }
    void BackEdge(Edge /*edge*/) override { has_order_ = false; }

    const std::vector<Vertex>& GetOrder() const { return order_; }
    bool HasOrder() const { return has_order_; }

  private:
    std::vector<Vertex> order_;
    bool has_order_;
};

template <class Vertex, class Edge>
class SourceComponentSizeCalculator
    : public traverses::DfsVisitor<Vertex, Edge> {
  public:    
    SourceComponentSizeCalculator() : is_source_(true) {}

    void FinishVertex(Vertex vertex) override { component_vertices_.insert(vertex); }
    void ForwardOrCrossEdge(Edge edge) override;

    size_t GetSize() const { return component_vertices_.size(); }
    bool IsSource() const { return is_source_; }

  private:
    bool is_source_;
    std::unordered_set<Vertex> component_vertices_;
};

template <class Graph>
size_t FindMinSourceComponentSize(const Graph& graph);

std::unique_ptr<graphs::Edge> 
ReadEdgeAndTransformToNativeFormat(std::istream& input = std::cin);

graphs::Graph ReadAndMakeGraph(
        size_t number_of_vertices, size_t number_of_edges,
        std::istream& input = std::cin);

int main() {
    std::ifstream input_stream("B.in");
    std::ofstream output_stream("B.out");
    size_t number_of_people, number_of_games;
    input_stream >> number_of_people >> number_of_games;
    auto graph = 
        ReadAndMakeGraph(number_of_people, number_of_games, input_stream);
    size_t min_component_size = FindMinSourceComponentSize(graph);
    size_t max_number_of_coworkers = number_of_people - min_component_size + 1;
    output_stream << max_number_of_coworkers;
    return 0;
}

// CODE

namespace traverses {

// DepthFirstSearch

template <class Vertex, class Graph>
bool DepthFirstSearch<Vertex, Graph>::IsBlack(Vertex vertex) const {
    return black_vertices_.find(vertex) != black_vertices_.end();
}

template <class Vertex, class Graph>
bool DepthFirstSearch<Vertex, Graph>::IsGray(Vertex vertex) const {
    return gray_vertices_.find(vertex) != gray_vertices_.end();
}

template <class Vertex, class Graph>
template <class Visitor>
void DepthFirstSearch<Vertex, Graph>::operator() (const Vertex& origin_vertex, Visitor& visitor) {
    if (!IsBlack(origin_vertex)) {
        visitor.DiscoverVertex(origin_vertex);
        Search(origin_vertex, visitor);
    }
}

template <class Vertex, class Graph>
template <class Visitor>
void DepthFirstSearch<Vertex, Graph>::Search(const Vertex& origin_vertex, Visitor& visitor) {
    gray_vertices_.insert(origin_vertex);
    for (const auto& edge: graph_.OutgoingEdges(origin_vertex)) {
        auto vertex = edge.GetTarget();
        if (!IsBlack(vertex)) {
            if (!IsGray(vertex)) {
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
    black_vertices_.insert(origin_vertex);
    gray_vertices_.erase(origin_vertex);
}

// DepthFirstSearch

}  // namespace traverses


namespace graphs {

// Graph

Graph::Graph(size_t number_of_vertices, Graph::Edges edges)
        : number_of_vertices_(number_of_vertices),
          edges_(std::move(edges)),
          begins_(number_of_vertices + 1)
{
    std::sort(edges_.begin(), edges_.end(), 
              [](const Edge& first, const Edge& second) {
                  return first.GetSource() < second.GetSource();
              });
    auto current_iterator = edges_.cbegin();
    for (size_t vertex_index = 0; vertex_index < number_of_vertices_; ++vertex_index) {
        begins_[vertex_index] = current_iterator;
        current_iterator = std::find_if(current_iterator, edges_.cend(), [vertex_index](Edge edge) {
                    return edge.GetSource() != vertex_index;
                });
    }
    begins_[number_of_vertices_] = edges_.cend();
}

IteratorRange<Graph::Edges::const_iterator> Graph::OutgoingEdges(Vertex vertex) const {
    return IteratorRange<Edges::const_iterator>(begins_[vertex], begins_[vertex + 1]);
}

Graph Graph::Reverse() const {
    Edges reversed_edges;
    reversed_edges.reserve(edges_.size());
    for (const auto& edge: edges_) {
        reversed_edges.push_back(edge.Reverse());
    }
    return Graph(number_of_vertices_, reversed_edges);
}

size_t Graph::GetNumberOfVertices() const {
    return number_of_vertices_;
}

} // namespace graphs

// SourceComponentSizeCalculator

template <class Vertex, class Edge>
void SourceComponentSizeCalculator<Vertex, Edge>::ForwardOrCrossEdge(Edge edge) {
    if (component_vertices_.find(edge.GetTarget()) == component_vertices_.end()) {
        is_source_ = false;
    }
}

// SourceComponentSizeCalculator

template <class Graph>
size_t FindMinSourceComponentSize(const Graph& graph) {
    TopoOrderCalculator<typename Graph::Vertex, typename Graph::Edge> order_calculator;
    traverses::DepthFirstSearch<typename Graph::Vertex, Graph> forward_dfs(graph);
    for (size_t index = 0; index < graph.GetNumberOfVertices(); ++index) {
        forward_dfs(index, order_calculator); 
    }
    const auto& ordered_vertices = order_calculator.GetOrder();

    Graph reversed_graph = graph.Reverse();
    typedef SourceComponentSizeCalculator<
        typename Graph::Vertex, typename Graph::Edge> SizeCalculator;
    traverses::DepthFirstSearch<typename Graph::Vertex, Graph> backward_dfs(reversed_graph);
    size_t min_size = graph.GetNumberOfVertices();
    for (auto iterator = ordered_vertices.rbegin(); 
         iterator != ordered_vertices.rend(); 
         ++iterator)
    {
        if (!backward_dfs.IsBlack(*iterator)) {
            SizeCalculator size_calculator;
            backward_dfs(*iterator, size_calculator); 
            size_t size = size_calculator.GetSize();
            bool is_source = size_calculator.IsSource();
            if (is_source && size < min_size) {
                min_size = size;
            }
        }
    }   
    return min_size;
}

std::unique_ptr<graphs::Edge> 
ReadEdgeAndTransformToNativeFormat(std::istream& input) {
    static const size_t kWin = 1;
    static const size_t kLosing = 2;
    size_t first, second, result;
    input >> first >> second >> result;
    --first;
    --second;
    if (result == kWin) {
        return make_unique<graphs::Edge>(first, second);
    } else if (result == kLosing) {
        return make_unique<graphs::Edge>(second, first);
    } else {
        return nullptr;
    }
}

graphs::Graph ReadAndMakeGraph(
        size_t number_of_vertices, size_t number_of_edges,
        std::istream& input)
{
    graphs::Graph::Edges edges;
    edges.reserve(number_of_edges);
    for (size_t index = 0; index < number_of_edges; ++index) {
        std::unique_ptr<graphs::Edge> edge = ReadEdgeAndTransformToNativeFormat(input);
        if (edge) {
            edges.push_back(*edge);
        }
    }
    return graphs::Graph(number_of_vertices, edges);
}


