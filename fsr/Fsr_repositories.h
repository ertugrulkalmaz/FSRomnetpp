#ifndef __FSR_repositories_h__
#define __FSR_repositories_h__

#include <string.h>
#include <set>
#include <vector>

#include "inet/common/INETDefs.h"

#include "inet/networklayer/common/L3Address.h"

namespace inet {

namespace inetmanet {

typedef L3Address nsaddr_t;


typedef std::vector<nsaddr_t>   PathVector; // Route
typedef std::list<PathVector>   RouteList;  // Route

typedef struct Fsr_rt_entry : public cObject
{
    nsaddr_t    dest_addr_; ///< Address of the destination node.
    nsaddr_t    next_addr_; ///< Address of the next hop.
    nsaddr_t    iface_addr_;    ///< Address of the local interface.
    uint32_t    dist_;      ///< Distance in hops to the destination.
    double quality;
    double delay;
    int index;
    inline int & local_iface_index() {return index;}
    PathVector  route;

    inline nsaddr_t & dest_addr()   { return dest_addr_; }
    inline nsaddr_t & next_addr()   { return next_addr_; }
    inline nsaddr_t & iface_addr()  { return iface_addr_; }


    void    setDest_addr(const nsaddr_t &dest_addr) {  dest_addr_ = dest_addr; }
    void    setNext_addr(const nsaddr_t &next_addr) {  next_addr_ = next_addr; }
    void    setIface_addr(const nsaddr_t    &iface_addr)    {  iface_addr_ = iface_addr; }

    inline uint32_t&    dist()      { return dist_; }
    Fsr_rt_entry() {}
    Fsr_rt_entry(const Fsr_rt_entry * e)
    {
        dest_addr_ = e->dest_addr_;  ///< Address of the destination node.
        next_addr_ = e->next_addr_;   ///< Address of the next hop.
        iface_addr_ = e->iface_addr_; ///< Address of the local interface.
        dist_ = e->dist_;     ///< Distance in hops to the destination.
        route = e->route;
        index = e->index;
    }
    virtual Fsr_rt_entry *dup() const {return new Fsr_rt_entry(this);}

} FSR_rt_entry;

/// An Interface Association Tuple.
typedef struct Fsr_iface_assoc_tuple : public cObject
{
    /// Interface address of a node.
    nsaddr_t    iface_addr_;
    /// Main address of the node.
    nsaddr_t    main_addr_;
    /// Time at which this tuple expires and must be removed.
    double      time_;
    cObject *asocTimer;
    // cMessage *asocTimer;

    int index;
    inline int & local_iface_index() {return index;}

    inline nsaddr_t & iface_addr()  { return iface_addr_; }
    inline nsaddr_t & main_addr()   { return main_addr_; }


    void    setIface_addr(const nsaddr_t &a)    { iface_addr_ = a; }
    void    setMain_addr(const nsaddr_t &a) {  main_addr_ = a; }

    inline double&      time()      { return time_; }
    Fsr_iface_assoc_tuple() {asocTimer = nullptr;}
    Fsr_iface_assoc_tuple(const Fsr_iface_assoc_tuple * e)
    {
        memcpy((void*)this, (void*)e, sizeof(Fsr_iface_assoc_tuple));
        asocTimer = nullptr;
    }
    virtual Fsr_iface_assoc_tuple *dup() const {return new Fsr_iface_assoc_tuple(this);}

} FSR_iface_assoc_tuple;

/// A Link Tuple.
typedef struct Fsr_link_tuple : public cObject
{
    /// Interface address of the local node.
    nsaddr_t    local_iface_addr_;
    /// Interface address of the neighbor node.
    nsaddr_t    nb_iface_addr_;
    /// The link is considered bidirectional until this time.
    double      sym_time_;
    /// The link is considered unidirectional until this time.
    double      asym_time_;
    /// The link is considered lost until this time (used for link layer notification).
    double      lost_time_;
    /// Time at which this tuple expires and must be removed.
    double      time_;
    int         index;
    //cMessage *asocTimer;
    cObject *asocTimer;

    inline nsaddr_t & local_iface_addr()    { return local_iface_addr_; }
    inline nsaddr_t & nb_iface_addr()       { return nb_iface_addr_; }
    inline int & local_iface_index() {return index;}


    void    setLocal_iface_addr(const nsaddr_t &a)  {local_iface_addr_ = a; }
    void    setNb_iface_addr(const nsaddr_t &a) {nb_iface_addr_ = a; }

    inline double&      sym_time()      { return sym_time_; }
    inline double&      asym_time()     { return asym_time_; }
    inline double&      lost_time()     { return lost_time_; }
    inline double&      time()          { return time_; }

    Fsr_link_tuple() {asocTimer = nullptr;}
    Fsr_link_tuple(const Fsr_link_tuple * e)
    {
        memcpy((void*)this,(void*) e, sizeof(Fsr_link_tuple));
        asocTimer = nullptr;
    }
    virtual Fsr_link_tuple *dup() const {return new Fsr_link_tuple(this);}

} FSR_link_tuple;

/// A Neighbor Tuple.
typedef struct Fsr_nb_tuple : public cObject
{
    /// Main address of a neighbor node.
    nsaddr_t nb_main_addr_;
    /// Neighbor Type and Link Type at the four less significative digits.
    uint8_t status_;
    /// A value between 0 and 7 specifying the node's willingness to carry traffic on behalf of other nodes.
    uint8_t willingness_;
    //cMessage *asocTimer;
    cObject *asocTimer;

    inline nsaddr_t & nb_main_addr()    { return nb_main_addr_; }
    void    setNb_main_addr(const nsaddr_t &a)  { nb_main_addr_ = a; }

    inline uint8_t& getStatus() { return status_; }
    inline uint8_t& willingness()   { return willingness_; }

    Fsr_nb_tuple() {asocTimer = nullptr;}
    Fsr_nb_tuple(const Fsr_nb_tuple * e)
    {
        memcpy((void*)this, (void*)e, sizeof(Fsr_nb_tuple));
        asocTimer = nullptr;
    }
    virtual Fsr_nb_tuple *dup() const {return new Fsr_nb_tuple(this);}
    ~Fsr_nb_tuple()
    {
        status_ = 0;
    }
} FSR_nb_tuple;

/// A 2-hop Tuple.
typedef struct Fsr_nb2hop_tuple : public cObject
{
    /// Main address of a neighbor.
    nsaddr_t    nb_main_addr_;
    /// Main address of a 2-hop neighbor with a symmetric link to nb_main_addr.
    nsaddr_t    nb2hop_addr_;
    /// Time at which this tuple expires and must be removed.
    double      time_;
    //cMessage *asocTimer;
    cObject *asocTimer;

    inline nsaddr_t & nb_main_addr()    { return nb_main_addr_; }
    inline nsaddr_t & nb2hop_addr() { return nb2hop_addr_; }
    void    setNb_main_addr(const nsaddr_t &a)  { nb_main_addr_ = a; }
    void    setNb2hop_addr(const nsaddr_t &a)   { nb2hop_addr_ = a; }

    inline double&      time()      { return time_; }

    Fsr_nb2hop_tuple() {asocTimer = nullptr;}
    Fsr_nb2hop_tuple(const Fsr_nb2hop_tuple * e)
    {
        memcpy((void*)this, (void*)e, sizeof(Fsr_nb2hop_tuple));
        asocTimer = nullptr;
    }
    virtual Fsr_nb2hop_tuple *dup() const {return new Fsr_nb2hop_tuple(this);}

} FSR_nb2hop_tuple;

/// An MPR-Selector Tuple.
typedef struct Fsr_mprsel_tuple : public cObject
{
    /// Main address of a node which have selected this node as a MPR.
    nsaddr_t    main_addr_;
    /// Time at which this tuple expires and must be removed.
    double      time_;
    // cMessage *asocTimer;
    cObject *asocTimer;

    inline nsaddr_t & main_addr()   { return main_addr_; }
    void    setMain_addr(const nsaddr_t &a) {main_addr_ = a; }
    inline double&      time()      { return time_; }

    Fsr_mprsel_tuple() {asocTimer = nullptr;}
    Fsr_mprsel_tuple(const Fsr_mprsel_tuple * e)
    {
        memcpy((void*)this, (void*)e, sizeof(Fsr_mprsel_tuple));
        asocTimer = nullptr;
    }
    virtual Fsr_mprsel_tuple *dup() const {return new Fsr_mprsel_tuple(this);}


} FSR_mprsel_tuple;

/// The type "list of interface addresses"
typedef std::vector<nsaddr_t> addr_list_t;

/// A Duplicate Tuple
typedef struct Fsr_dup_tuple : public cObject
{
    /// Originator address of the message.
    nsaddr_t    addr_;
    /// Message sequence number.
    uint16_t    seq_num_;
    /// Indicates whether the message has been retransmitted or not.
    bool        retransmitted_;
    /// List of interfaces which the message has been received on.
    addr_list_t iface_list_;
    /// Time at which this tuple expires and must be removed.
    double      time_;
    // cMessage *asocTimer;
    cObject *asocTimer;

    inline nsaddr_t & getAddr()     { return addr_; }
    void    setAddr(const nsaddr_t &a)  {addr_ = a; }

    inline uint16_t&    seq_num()   { return seq_num_; }
    inline bool&        retransmitted() { return retransmitted_; }
    inline addr_list_t& iface_list()    { return iface_list_; }
    inline double&      time()      { return time_; }

    Fsr_dup_tuple() {asocTimer = nullptr;}
    Fsr_dup_tuple(const Fsr_dup_tuple * e)
    {
        memcpy((void*)this, (void*)e, sizeof(Fsr_dup_tuple));
        asocTimer = nullptr;
    }
    virtual Fsr_dup_tuple *dup() const {return new Fsr_dup_tuple(this);}

} FSR_dup_tuple;

/// A Topology Tuple
typedef struct Fsr_topology_tuple : public cObject
{
    /// Main address of the destination.
    nsaddr_t    dest_addr_;
    /// Main address of a node which is a neighbor of the destination.
    nsaddr_t    last_addr_;
    /// Sequence number.
    uint16_t    seq_;
    /// Time at which this tuple expires and must be removed.
    double      time_;
    int index;

    // cMessage *asocTimer;
    cObject *asocTimer;


    inline nsaddr_t & dest_addr()   { return dest_addr_; }
    inline nsaddr_t & last_addr()   { return last_addr_; }
    inline void setDest_addr(const nsaddr_t &a) {dest_addr_ = a; }
    inline void setLast_addr(const nsaddr_t &a) {last_addr_ = a; }
    inline uint16_t&    seq()       { return seq_; }
    inline double&      time()      { return time_; }
    inline int & local_iface_index() {return index;}

    Fsr_topology_tuple() {asocTimer = nullptr;}
    Fsr_topology_tuple(const Fsr_topology_tuple * e)
    {
        dest_addr_ = e->dest_addr_;
        last_addr_ = e->last_addr_;
        seq_ = e->seq_;
        time_ = e->time_;
        index = e->index;
        asocTimer = nullptr;
    }
    virtual Fsr_topology_tuple *dup() const {return new Fsr_topology_tuple(this);}

} FSR_topology_tuple;


typedef std::set<nsaddr_t>          mprset_t;   ///< MPR Set type.
typedef std::vector<Fsr_mprsel_tuple*>     mprselset_t;    ///< MPR Selector Set type.
typedef std::vector<Fsr_link_tuple*>       linkset_t;  ///< Link Set type.
typedef std::vector<Fsr_nb_tuple*>     nbset_t;    ///< Neighbor Set type.
typedef std::vector<Fsr_nb2hop_tuple*>     nb2hopset_t;    ///< 2-hop Neighbor Set type.
typedef std::vector<Fsr_topology_tuple*>   topologyset_t;  ///< Topology Set type.
typedef std::vector<Fsr_dup_tuple*>        dupset_t;   ///< Duplicate Set type.
typedef std::vector<Fsr_iface_assoc_tuple*>    ifaceassocset_t; ///< Interface Association Set type.

} // namespace inetmanet

} // namespace inet

#endif

