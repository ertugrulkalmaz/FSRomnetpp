#ifndef __Fsr_Etx_dijkstra_h__
#define __Fsr_Etx_dijkstra_h__

#include <vector>
#include <set>
#include <map>
#include "inet/routing/extras/fsr/Fsr_Etx_parameter.h"
#include "inet/routing/extras/fsr/Fsr_repositories.h"

namespace inet {

namespace inetmanet {

typedef struct edge
{
    nsaddr_t last_node_; // last node to reach node X
    double   quality_; // link quality
    double   delay_; // link delay

    inline nsaddr_t& last_node() { return last_node_; }
    inline double&   quality()     { return quality_; }
    inline double&   getDelay()     { return delay_; }
} edge;

typedef struct hop
{
    edge     link_; // indicates the total cost to reach X and the last node to reach it
    int      hop_count_; // number of hops in this path

    inline   edge& link() { return link_; }
    inline   int&  hop_count() { return hop_count_; }
} hop;



///
/// \brief Class that implements the Dijkstra Algorithm
///
///
class Dijkstra : public cOwnedObject
{
  private:
    typedef std::set<nsaddr_t> NodesSet;
    typedef std::map<nsaddr_t, std::vector<edge*> > LinkArray;
    NodesSet * nonprocessed_nodes_;
    LinkArray * link_array_;
    int highest_hop_;

    NodesSet::iterator best_cost();
    edge* get_edge(const nsaddr_t &, const nsaddr_t &);
    Fsr_Etx_parameter *parameter;

  public:
    NodesSet *all_nodes_;

    // D[node].first == cost to reach 'node' through this hop
    // D[node].second.first == last hop to reach 'node'
    // D[node].second.second == number of hops to reach 'node'
    typedef std::map<nsaddr_t, hop >  DijkstraMap;
    DijkstraMap dijkstraMap;

    Dijkstra();
    ~Dijkstra();

    void add_edge(const nsaddr_t &, const nsaddr_t &, double, double, bool);
    void run();
    void clear();

    inline int highest_hop() { return highest_hop_; }
    inline std::set<nsaddr_t> * all_nodes() { return all_nodes_; }
    inline hop& D(const nsaddr_t &node) { return dijkstraMap[node]; }
};

} // namespace inetmanet

} // namespace inet

#endif

