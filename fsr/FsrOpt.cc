#include <math.h>
#include <limits.h>

#include "inet/networklayer/ipv4/Ipv4InterfaceData.h"
#include "inet/routing/extras/fsr/FsrPkt_m.h"
#include "inet/routing/extras/fsr/FsrOpt.h"

namespace inet {

namespace inetmanet {


#define UDP_HDR_LEN 8

#define RT_PORT 698
#define IP_DEF_TTL 32


#define state_      (*state_ptr)



Define_Module(FsrOpt);





void
FsrOpt::recv_fsr(Packet* msg)
{

    nsaddr_t src_addr;
    int index;
    bool recalculateRoutes = false;

    // All routing messages are sent from and to port RT_PORT,
    // so we check it.

    bool proc = check_packet(msg, src_addr, index);
    if (!proc) {
        delete msg;
        return;
    }
    auto &op = msg->popAtFront<FsrPkt>();

    // If the packet contains no messages must be silently discarded.
    // There could exist a message with an empty body, so the size of
    // the packet would be pkt-hdr-size + msg-hdr-size.

    if (op->getChunkLength() < B(FSR_PKT_HDR_SIZE + FSR_MSG_HDR_SIZE))
    {
        delete msg;
        return;
    }

// Process Fsr information
    assert(op->msgArraySize() >= 0 && op->msgArraySize() <= FSR_MAX_MSGS);
    for (int i = 0; i < (int) op->msgArraySize(); i++)
    {
        auto msgAux = op->msg(i);
        FsrMsg msg = msgAux;

        // If ttl is less than or equal to zero, or
        // the receiver is the same as the originator,
        // the message must be silently dropped
        // if (msg.ttl() <= 0 || msg.orig_addr() == ra_addr())
        if (msg.ttl() <= 0 || isLocalAddress(msg.orig_addr()))
            continue;

        // If the message has been processed it must not be
        // processed again
        bool do_forwarding = true;
        Fsr_dup_tuple* duplicated = state_.find_dup_tuple(msg.orig_addr(), msg.msg_seq_num());
        if (duplicated == nullptr)
        {
            // Process the message according to its type
            if (msg.msg_type() == FSR_HELLO_MSG){
                recalculateRoutes += process_hello(msg, ra_addr(), src_addr, index);
            }
            else if (msg.msg_type() == FSR_TC_MSG){
                recalculateRoutes += process_tc(msg, src_addr, index);
            }
            else if (msg.msg_type() == FSR_MID_MSG)
                process_mid(msg, src_addr, index);
            else
            {
                debug("%f: Node %s can not process FSR packet because does not "
                      "implement FSR type (%x)\n",
                      CURRENT_TIME,
                      getNodeId(ra_addr()).c_str(),
                      msg.msg_type());
            }
        }
        else
        {
            // If the message has been considered for forwarding, it should
            // not be retransmitted again
            for (auto it = duplicated->iface_list().begin();
                    it != duplicated->iface_list().end();
                    it++)
            {
                if (*it == ra_addr())
                {
                    do_forwarding = false;
                    break;
                }
            }
        }

        if (do_forwarding)
        {
            // HELLO messages are never forwarded.
            // TC and MID messages are forwarded using the default algorithm.
            // Remaining messages are also forwarded using the default algorithm.
            if (msg.msg_type() != FSR_HELLO_MSG)
                forward_default(msg, duplicated, ra_addr(), src_addr);
        }

    }
    delete msg;

    // After processing all FSR messages, we must recompute routing table
    if (recalculateRoutes || getTopologyChanged()){
        rtable_computation();
    }

}


///
/// \brief Processes a HELLO message following RFC 3626 specification.
///
/// Link sensing and population of the Neighbor Set, 2-hop Neighbor Set and MPR
/// Selector Set are performed.
///
/// \param msg the %FSR message which contains the HELLO message.
/// \param receiver_iface the address of the interface where the message was received from.
/// \param sender_iface the address of the interface where the message was sent from.
///
bool
FsrOpt::process_hello(FsrMsg& msg, const nsaddr_t &receiver_iface, const nsaddr_t &sender_iface, const int &index)
{
    assert(msg.msg_type() == FSR_HELLO_MSG);

    bool ret2 = link_sensing(msg, receiver_iface, sender_iface, index);
    populate_nbset(msg); // this does not populate, just updates willingness
    bool ret1 = populate_nb2hopset(msg);

    if (ret1 || ret2)
        mpr_computation();

    populate_mprselset(msg);
    return (ret1 || ret2);
}

///
/// \brief Processes a TC message following RFC 3626 specification.
///
/// The Topology Set is updated (if needed) with the information of
/// the received TC message.
///
/// \param msg the %FSR message which contains the TC message.
/// \param sender_iface the address of the interface where the message was sent from.
///
bool
FsrOpt::process_tc(FsrMsg& msg, const nsaddr_t &sender_iface, const int &index)
{
    assert(msg.msg_type() == FSR_TC_MSG);
    double now = CURRENT_TIME;
    Fsr_tc& tc = msg.tc();

    // 1. If the sender interface of this message is not in the symmetric
    // 1-hop neighborhood of this node, the message MUST be discarded.
    Fsr_link_tuple* link_tuple = state_.find_sym_link_tuple(sender_iface, now);
    if (link_tuple == nullptr)
        return false;

    // 2. If there exist some tuple in the topology set where:
    //  T_last_addr == originator address AND
    //  T_seq       >  ANSN,
    // then further processing of this TC message MUST NOT be
    // performed.
    Fsr_topology_tuple* topology_tuple =
        state_.find_newer_topology_tuple(msg.orig_addr(), tc.ansn());
    if (topology_tuple != nullptr)
        return false;
      return update_topology_tuples(msg, index);
}

int
FsrOpt::update_topology_tuples(FsrMsg& msg, int index)
{

    double now = CURRENT_TIME;
    Fsr_tc& tc = msg.tc();
    if (tc.count == 0)
        return 0;
    int changedTuples = 0; // needed to know if we have to recalculate the routes
    std::set<int> tccounter;
    /* Standard says:
     *  3    All tuples in the topology set where:

               T_last_addr == originator address AND

               T_seq       <  ANSN

          MUST be removed from the topology set.

     4    For each of the advertised neighbor main address received in
          the TC message:

          4.1  If there exist some tuple in the topology set where:

                    T_dest_addr == advertised neighbor main address, AND

                    T_last_addr == originator address,

               then the holding time of that tuple MUST be set to:

                    T_time      =  current time + validity time.

          4.2  Otherwise, a new tuple MUST be recorded in the topology
               set where:

                    T_dest_addr = advertised neighbor main address,

                    T_last_addr = originator address,

                    T_seq       = ANSN,

                    T_time      = current time + validity time.
     *
     * This shoud achieve the same but with less erase&add.
     *
     */
    for (auto it = topologyset().begin(); it != topologyset().end();)
    {
        bool foundTuple = 0;
        if ((*it)->last_addr_ == msg.orig_addr()){ // for any tuple in the list that is
            // passing for this node
            for (int i = 0; i < tc.count; i++)
            {
                assert(i >= 0 && i < FSR_MAX_ADDRS);
                nsaddr_t addr = tc.nb_main_addr(i);
                if((*it)->dest_addr() == addr){ // found a tuple to be updated
                    (*it)->time() = now + FsrOpt::emf_to_seconds(msg.vtime());
                    (*it)->seq() = tc.ansn();
                    foundTuple = 1;
                    tccounter.insert(i);
                }
            }
            if (!foundTuple){ // the tuple was not in present in the TC, erase it
                changedTuples++;
                it = topologyset().erase(it); // erase and increment iterator
                continue;
            }else{
                it++;
                continue;
            }
        }
        it++; // did not enter the main if, increment iterator
    }
    for (int i = 0; i < tc.count; i++)
    {
        if(tccounter.find(i) == tccounter.end()){ // we did not update this, let's add it
            nsaddr_t addr = tc.nb_main_addr(i);
            Fsr_topology_tuple* topology_tuple = new Fsr_topology_tuple;
            topology_tuple->dest_addr() = addr;
            topology_tuple->last_addr() = msg.orig_addr();
            topology_tuple->seq() = tc.ansn();
            topology_tuple->time() = now + FsrOpt::emf_to_seconds(msg.vtime());
            topology_tuple->local_iface_index() = index;
            add_topology_tuple(topology_tuple);
            // Schedules topology tuple deletion
            Fsr_TopologyTupleTimer* topology_timer =
                    new Fsr_TopologyTupleTimer(this, topology_tuple);
            topology_timer->resched(DELAY(topology_tuple->time()));
            changedTuples++;
        }
    }
    return changedTuples;
}


///
/// \brief  Updates Link Set according to a new received HELLO message (following RFC 3626
///     specification). Neighbor Set is also updated if needed.
///
/// \param msg the FSR message which contains the HELLO message.
/// \param receiver_iface the address of the interface where the message was received from.
/// \param sender_iface the address of the interface where the message was sent from.
///
bool
FsrOpt::link_sensing(FsrMsg& msg, const nsaddr_t &receiver_iface, const nsaddr_t &sender_iface, const int &index)
{
    Fsr_hello& hello = msg.hello();
    double now = CURRENT_TIME;
    bool updated = false;
    bool created = false;

    Fsr_link_tuple* link_tuple = state_.find_link_tuple(sender_iface);
    if (link_tuple == nullptr)
    {
        // We have to create a new tuple
        link_tuple = new Fsr_link_tuple;
        link_tuple->nb_iface_addr() = sender_iface;
        link_tuple->local_iface_addr() = receiver_iface;
        link_tuple->local_iface_index() = index;
        link_tuple->sym_time() = now - 1;
        link_tuple->lost_time() = 0.0;
        link_tuple->time() = now + FsrOpt::emf_to_seconds(msg.vtime());
        add_link_tuple(link_tuple, hello.willingness());
        created = true;
    }
//    else
//        updated = true;

    link_tuple->asym_time() = now + FsrOpt::emf_to_seconds(msg.vtime());
    assert(hello.count >= 0 && hello.count <= FSR_MAX_HELLOS);
    for (int i = 0; i < hello.count; i++)
    {
        Fsr_hello_msg& hello_msg = hello.hello_msg(i);
        int lt = hello_msg.link_code() & 0x03;
        int nt = hello_msg.link_code() >> 2;

        // We must not process invalid advertised links
        if ((lt == FSR_SYM_LINK && nt == FSR_NOT_NEIGH) ||
                (nt != FSR_SYM_NEIGH && nt != FSR_MPR_NEIGH
                 && nt != FSR_NOT_NEIGH))
            continue;

        assert(hello_msg.count >= 0 && hello_msg.count <= FSR_MAX_ADDRS);
        for (int j = 0; j < hello_msg.count; j++)
        {
            if (hello_msg.nb_iface_addr(j) == receiver_iface)
            {
                if (lt == FSR_LOST_LINK)
                {
                    link_tuple->sym_time() = now - 1;
                    updated = true;
                }
                else if (lt == FSR_SYM_LINK || lt == FSR_ASYM_LINK)
                {
                    link_tuple->sym_time() =
                        now + FsrOpt::emf_to_seconds(msg.vtime());
                    link_tuple->time() =
                        link_tuple->sym_time() + FSR_NEIGHB_HOLD_TIME;
                    link_tuple->lost_time() = 0.0;
                    updated = true;
                }
                break;
            }
        }

    }
    link_tuple->time() = MAX(link_tuple->time(), link_tuple->asym_time());

    if (updated)
        updated_link_tuple(link_tuple, hello.willingness());

    // Schedules link tuple deletion
    if (created && link_tuple != nullptr)
    {
        Fsr_LinkTupleTimer* link_timer =
            new Fsr_LinkTupleTimer(this, link_tuple);
        link_timer->resched(DELAY(MIN(link_tuple->time(), link_tuple->sym_time())));
    }
    return (updated || created);
}

///
/// \brief  Updates the Neighbor Set according to the information contained in a new received
///     HELLO message (following RFC 3626). Actually this does not populate, just updates willingness
///
/// \param msg the %FSR message which contains the HELLO message.
///
bool
FsrOpt::populate_nbset(FsrMsg& msg)
{
    Fsr_hello& hello = msg.hello();

    Fsr_nb_tuple* nb_tuple = state_.find_nb_tuple(msg.orig_addr());
    if (nb_tuple != nullptr){ // it was already present
        nb_tuple->willingness() = hello.willingness();
    }
    return true; // added a new neighbor

}

///
/// \brief  Updates the 2-hop Neighbor Set according to the information contained in a new
///     received HELLO message (following RFC 3626).
///
/// \param msg the %FSR message which contains the HELLO message.
///
bool
FsrOpt::populate_nb2hopset(FsrMsg& msg)
{
    double now = CURRENT_TIME;
    Fsr_hello& hello = msg.hello();
    bool changedTopology = false;

    for (auto it_lt = linkset().begin(); it_lt != linkset().end(); it_lt++)
    {
        Fsr_link_tuple* link_tuple = *it_lt;
        if (get_main_addr(link_tuple->nb_iface_addr()) == msg.orig_addr())
        {
            if (link_tuple->sym_time() >= now)
            {
                assert(hello.count >= 0 && hello.count <= FSR_MAX_HELLOS);
                for (int i = 0; i < hello.count; i++)
                {
                    Fsr_hello_msg& hello_msg = hello.hello_msg(i);
                    int nt = hello_msg.link_code() >> 2;
                    assert(hello_msg.count >= 0 &&
                           hello_msg.count <= FSR_MAX_ADDRS);

                    for (int j = 0; j < hello_msg.count; j++)
                    {
                        nsaddr_t nb2hop_addr = hello_msg.nb_iface_addr(j);
                        if (nt == FSR_SYM_NEIGH || nt == FSR_MPR_NEIGH)
                        {
                            // if the main address of the 2-hop
                            // neighbor address = main address of
                            // the receiving node: silently
                            // discard the 2-hop neighbor address
                            if (nb2hop_addr != ra_addr())
                            {
                                // Otherwise, a 2-hop tuple is created
                                Fsr_nb2hop_tuple* nb2hop_tuple =
                                    state_.find_nb2hop_tuple(msg.orig_addr(), nb2hop_addr);
                                if (nb2hop_tuple == nullptr)
                                {
                                    nb2hop_tuple =
                                        new Fsr_nb2hop_tuple;
                                    nb2hop_tuple->nb_main_addr() =
                                        msg.orig_addr();
                                    nb2hop_tuple->nb2hop_addr() =
                                        nb2hop_addr;
                                    add_nb2hop_tuple(nb2hop_tuple);
                                    nb2hop_tuple->time() =
                                        now + FsrOpt::emf_to_seconds(msg.vtime());
                                    // Schedules nb2hop tuple
                                    // deletion
                                    Fsr_Nb2hopTupleTimer* nb2hop_timer =
                                        new Fsr_Nb2hopTupleTimer(this, nb2hop_tuple);
                                    nb2hop_timer->resched(DELAY(nb2hop_tuple->time()));
                                    changedTopology = true;
                                }
                                else
                                {
                                    nb2hop_tuple->time() =
                                        now + FsrOpt::emf_to_seconds(msg.vtime());
                                }

                            }
                        }
                        else if (nt == FSR_NOT_NEIGH)
                        {
                            // For each 2-hop node listed in the HELLO
                            // message with Neighbor Type equal to
                            // NOT_NEIGH all 2-hop tuples where:
                            // N_neighbor_main_addr == Originator
                            // Address AND N_2hop_addr  == main address
                            // of the 2-hop neighbor are deleted.
                            if(state_.erase_nb2hop_tuples(msg.orig_addr(),
                                                       nb2hop_addr))
                                changedTopology = true;
                        }
                    }
                }
            }
        }
    }
    return changedTopology;
}

///
/// \brief Performs all actions needed when a neighbor loss occurs.
///
/// Neighbor Set, 2-hop Neighbor Set, MPR Set and MPR Selector Set are updated.
///
/// \param tuple link tuple with the information of the link to the neighbor which has been lost.
///
void
FsrOpt::nb_loss(Fsr_link_tuple* tuple)
{
    bool topologychanged = false;
    debug("%f: Node %s detects neighbor %s loss\n",
          CURRENT_TIME,
          getNodeId(ra_addr()).c_str(),
          getNodeId(tuple->nb_iface_addr()).c_str());

    updated_link_tuple(tuple, FSR_WILL_DEFAULT);
    topologychanged |= state_.erase_nb2hop_tuples(get_main_addr(tuple->nb_iface_addr()));
    topologychanged |= state_.erase_mprsel_tuples(get_main_addr(tuple->nb_iface_addr()));
    EV_DEBUG << "Topology change << :" <<  topologychanged << endl;
    mpr_computation();
    rtable_computation();
}

} // namespace inetmanet

} // namespace inet

