#include <iostream>
#include <limits>

#include "stxxl_writer.h"
#include <stxxl/cmdline>
#include <stxxl/vector>
#include <stxxl/sorter>

#include <cstdint>

using Node = long long unsigned int;
using Degree = Node;
using Edge = STXXLEdge;
using EdgeVector = stxxl::VECTOR_GENERATOR<Edge>::result;

// STXXL comparator for edges with (anti-)lexicographical less
template<bool First=true>
struct EdgeComparator {
    bool operator() (const Edge& a, const Edge& b) const {
        if (First)
            return std::tie(a.first, a.second) < std::tie(b.first, b.second);
        else
            return std::tie(a.second, a.first) < std::tie(b.second, b.first);
    }

    Edge min_value() const {
        return {std::numeric_limits<Node>::min(), std::numeric_limits<Node>::min()};
    }

    Edge max_value() const {
        return {std::numeric_limits<Node>::max(), std::numeric_limits<Node>::max()};
    }
};

// STXXL Comparator for node (less)
struct NodeComparator {
    bool operator() (const Node& a, const Node& b) const {return a < b;}
    Node min_value() const {return std::numeric_limits<Node>::min();}
    Node max_value() const {return std::numeric_limits<Node>::max();}
};

struct DirectedDegree {
    Node in;
    Node out;
};

// Takes an ordered stream and removes adjacenct duplicates
// It also counts the multiplicity (degree) and rank in the unique stream
template <typename InputStream, typename ValueType>
class UniqueStream {
    InputStream& _in;

    Degree _degree;
    ValueType _id;
    Node _rank;
    bool _empty;

    void _fetch() {
        if (UNLIKELY(_in.empty())) {
            _empty = true;
            return;
        }

        // fetch id
        _id = *_in;

        // count total degree
        _degree=0;
        for(; !_in.empty() && *_in == _id; ++_in) {
            ++_degree;
        }

    }

public:
    UniqueStream(InputStream& in) :
        _in(in), _degree(0), _rank(0)
    {
        _fetch();
    }

    void operator++() {
        _fetch();
        ++_rank;
    }

    bool empty() const {return _empty;}
    const ValueType& id() const {return _id;}
    const Node& rank() const {return _rank;}
    const Degree& degree() const {return _degree;}
};


int main(int argc, char** argv) {
    constexpr size_t SORTER_MEM = 1llu << 28;

    // parse command line
    std::string fin, fout_bin, fout_txt, fout_map_txt;
    bool remove_multi_edges = false;
    {
        stxxl::cmdline_parser cp;

        cp.add_string('i', "input",  fin, "Name of the output file");

        cp.add_string('o', "output", fout_bin, "Name of the output file");
        cp.add_string('a', "text-output", fout_txt, "Name of the output file in tab-separated ascii");

        cp.add_string('m', "map-file", fout_map_txt, "Name of the ascii mapping file");

        cp.add_flag('c', "clean-multi-edges", remove_multi_edges, "Remove repeated edges");

        if (!cp.process(argc, argv)) return -1;

        if (fin.empty() || (fout_bin.empty() && fout_txt.empty())) {
            std::cerr << "Need input and output files" << std::endl;
            return -1;
        }
    }

    // acquire edge list
    size_t number_of_edges = 0;

    stxxl::sorter<Node, NodeComparator> sorter_nodes(NodeComparator(), SORTER_MEM);
    stxxl::sorter<Edge, EdgeComparator<true>> sorter_edges_first(EdgeComparator<true>(), SORTER_MEM);
    stxxl::sorter<Edge, EdgeComparator<false>> sorter_edges_second(EdgeComparator<false>(), SORTER_MEM);

    {
        stxxl::linuxaio_file input_file(fin, stxxl::file::RDONLY | stxxl::file::DIRECT);
        EdgeVector input_vector(&input_file);

        number_of_edges = input_vector.size();
        std::cout << "Input file " << fin << " contains " << number_of_edges << " edges" << std::endl;

        uint64_t  elements_removed = 0;

        // this is a very inefficient way to treat duplicates, but increases readability
        if (remove_multi_edges) {
            // read file and sort edges
            stxxl::sorter<Edge, EdgeComparator<true>> unique_sorter(EdgeComparator<true>(), SORTER_MEM);
            for (auto &edge : typename EdgeVector::bufreader_type(input_vector)) {
                unique_sorter.push(edge);
            }
            unique_sorter.sort();

            // push unique elements
            for(UniqueStream<decltype(unique_sorter), Edge> stream(unique_sorter);
                !stream.empty();
                ++stream
            ) {
                const auto & edge = stream.id();
                elements_removed += stream.degree() - 1;

                sorter_edges_second.push(edge);
                sorter_nodes.push(edge.first);
                sorter_nodes.push(edge.second);                
            }

            // update and output stats
            std::cout << "Removed " << elements_removed << " (" << (100.0 * elements_removed / number_of_edges) << "%) edge duplicates" << std::endl;
            assert(sorter_edges_second.size() == number_of_edges - elements_removed);
            number_of_edges -= elements_removed;

        } else {
            for (auto &edge : typename EdgeVector::bufreader_type(input_vector)) {
                sorter_nodes.push(edge.first);
                sorter_nodes.push(edge.second);
                sorter_edges_second.push(edge);
            }
        }

        sorter_nodes.sort();
        sorter_edges_second.sort();
    }

    Node number_of_nodes = 0;
    // reader edges sorted by second node
    {
        // degree distribution
        auto comp = [] (const DirectedDegree& a, const DirectedDegree& b) {
            return std::tie(a.in, a.out) < std::tie(b.in, b.out);};
        std::map<DirectedDegree, unsigned int, decltype(comp)> degree_dist(comp);
        uint64_t out_degree = 0;

        // open mapping file (if requested)
        std::unique_ptr<std::ofstream> map_txt;
        if (!fout_map_txt.empty())
            map_txt.reset(new std::ofstream(fout_map_txt));

        UniqueStream<decltype(sorter_nodes), Node> node_stream(sorter_nodes);

        // we scan through each edge sorted by target node and replace the
        // target id by the rank of this node in a sorted node list.
        // optionally, mapping information are written into a file an
        // the degree distribution is computed
        for (; !sorter_edges_second.empty(); ++sorter_edges_second) {
            auto edge = *sorter_edges_second;

            while (node_stream.id() < edge.second) {
                // write info of node
                degree_dist[{node_stream.degree() - out_degree, out_degree}]++;
                out_degree = 0;

                // write out mapping (if requested)
                if (map_txt) *map_txt << node_stream.id() << "\n";

                // advance to next node
                ++node_stream;
            }

            ++out_degree;

            edge.second = node_stream.rank();
            sorter_edges_first.push(edge);
        }

        // forward to end of sorter in order to find largest node id,
        // the number of nodes and (maybe) write the mapping file
        Node largest_input_id = 0;
        while(!node_stream.empty()) {
            // write info of node
            degree_dist[{node_stream.degree() - out_degree, out_degree}]++;
            out_degree = 0;

            // write out mapping (if requested)
            if (map_txt) *map_txt << node_stream.id() << "\n";

            largest_input_id = node_stream.id();

            // advance to next node
            ++node_stream;
        }

        sorter_nodes.rewind();
        sorter_edges_first.sort();

        // output statistics
        number_of_nodes = node_stream.rank();
        std::cout << "Number of nodes: " << number_of_nodes << std::endl;
        std::cout << "Number of edges: " << number_of_edges << std::endl;
        std::cout << "Avg. Degree: " << (static_cast<double>(number_of_edges) / number_of_nodes) << std::endl;
        std::cout << "Largest input node id: " << largest_input_id << std::endl;

        std::cout << "Degree distribution:" << std::endl;
        Degree tot_in_degree = 0, tot_out_degree = 0;
        for(auto it = degree_dist.begin(); it != degree_dist.end(); ++it) {
            std::cout << " [in-deg: " << it->first.in << ", out-deg: " << it->first.out << "] appears " << it->second << " times" << std::endl;
            tot_in_degree += it->first.in * it->second;
            tot_out_degree += it->first.out * it->second;
        }
        std::cout << "Total degree: in:" << tot_in_degree << " out: " << tot_out_degree << std::endl;

        // plausibility checks
        assert(tot_in_degree == tot_out_degree);
        assert(tot_in_degree + tot_out_degree == sorter_nodes.size());
        assert(tot_in_degree + tot_out_degree == 2*number_of_edges);
    }


    // reader edges sorted by first node
    // replace node by rank as before and concurrently write out result
    {
        STXXLEdgeWriter* out_bin = nullptr;
        if (!fout_bin.empty())
            out_bin = stxxl_writer_open_file(fout_bin.c_str(), number_of_edges);

        std::unique_ptr<std::ofstream> out_txt;
        if (!fout_txt.empty())
            out_txt.reset(new std::ofstream(fout_txt));



        UniqueStream<decltype(sorter_nodes), Node> node_stream(sorter_nodes);

        for (; !sorter_edges_first.empty(); ++sorter_edges_first) {
            auto edge = *sorter_edges_first;

            // compute new node id as rank in the sorter
            while (node_stream.id() < edge.first) {
                assert(!sorter_nodes.empty()); // this cannot happen
                ++node_stream;
            }
            edge.first = node_stream.rank();


            if (out_bin) stxxl_writer_push(out_bin, &edge);
            if (out_txt) *out_txt << edge.first << "\t" << edge.second << "\n";
        }

        if (out_bin) {
            stxxl_writer_close(out_bin);
        }
    }

    return 0;
}