#include "inet/routing/extras/fsr/Fsr_Etx.h"

#include <math.h>
#include <limits.h>

#include <omnetpp.h>
#include "inet/routing/extras/fsr/Fsr_Etx_dijkstra.h"
#include "inet/routing/extras/fsr/FsrPkt_m.h"

namespace inet {

namespace inetmanet {

#define UDP_HDR_LEN 8
#define RT_PORT 698
#define IP_DEF_TTL 32


#define state_      (*state_etx_ptr)



Define_Module(Fsr_Etx);

/********** Timers **********/

void Fsr_Etx_LinkQualityTimer::expire()
{
    Fsr_Etx *agentaux = check_and_cast<Fsr_Etx *>(agent_);
    agentaux->Fsr_Etx::link_quality();
    // Reschedule using the standard timer mechanism so the event is
    // properly queued and processed by scheduleEvent().  The original
    // implementation inserted the timer directly into the queue which
    // skipped necessary housekeeping.
    simtime_t next = simTime() + agentaux->hello_ival();
    this->resched(next);
}


/********** FSR_ETX class **********/
///
///
void
Fsr_Etx::initialize(int stage)
{
    ManetRoutingBase::initialize(stage);

    if (stage == INITSTAGE_ROUTING_PROTOCOLS)
    {
        if (isInMacLayer())
            this->setAddressSize(6);
        FsrAddressSize::ADDR_SIZE = this->getAddressSize();
 	    FSR_REFRESH_INTERVAL = par("FSR_REFRESH_INTERVAL");
        //
        // Do some initializations
        willingness_ = &par("Willingness");
        hello_ival_ = &par("Hello_ival");
        tc_ival_ = &par("Tc_ival");
        mid_ival_ = &par("Mid_ival");
        use_mac_ = par("use_mac");


        if ( par("Fish_eye"))
            parameter_.fish_eye() = 1;
        else
            parameter_.fish_eye() = 0;
        parameter_.mpr_algorithm() = par("Mpr_algorithm");
        parameter_.routing_algorithm() = par("routing_algorithm");
        parameter_.link_quality() = par("Link_quality");

        parameter_.tc_redundancy() = par("Tc_redundancy");

        /// Link delay extension
        if (par("Link_delay"))
            parameter_.link_delay() = 1;
        else
            parameter_.link_delay() = 0;
        parameter_.c_alpha() = par("C_alpha");

        // Fish Eye Routing Algorithm for TC message dispatching...
        tc_msg_ttl_index_ = 0;

        tc_msg_ttl_ [0] = 255;
        tc_msg_ttl_ [1] = 3;
        tc_msg_ttl_ [2] = 2;
        tc_msg_ttl_ [3] = 1;
        tc_msg_ttl_ [4] = 2;
        tc_msg_ttl_ [5] = 1;
        tc_msg_ttl_ [6] = 1;
        tc_msg_ttl_ [7] = 3;
        tc_msg_ttl_ [8] = 2;
        tc_msg_ttl_ [9] = 1;
        tc_msg_ttl_ [10] = 2;
        tc_msg_ttl_ [11] = 1;
        tc_msg_ttl_ [12] = 1;

        /// Link delay extension
        cap_sn_ = 0;
        pkt_seq_ = FSR_MAX_SEQ_NUM;
        msg_seq_ = FSR_MAX_SEQ_NUM;
        ansn_ = FSR_MAX_SEQ_NUM;

        // Starts all timers
        helloTimer = new Fsr_HelloTimer(); ///< Timer for sending HELLO messages.
        tcTimer = new Fsr_TcTimer();   ///< Timer for sending TC messages.
        midTimer = new Fsr_MidTimer(); ///< Timer for sending MID messages.
        linkQualityTimer = new Fsr_Etx_LinkQualityTimer();

        state_ptr = state_etx_ptr = new Fsr_Etx_state(&this->parameter_);
        startIfUp();
        useIndex = false;

        if (use_mac())
        {
            linkLayerFeeback();
        }
    }
}

void Fsr_Etx::start()
{
    if (configured)
        return;
    configured = true;
    registerRoutingModule();
    for (int i = 0; i< getNumWlanInterfaces(); i++)
    {
        // Create never expiring interface association tuple entries for our
        // own network interfaces, so that GetMainAddress () works to
        // translate the node's own interface addresses into the main address.
        Fsr_iface_assoc_tuple* tuple = new Fsr_iface_assoc_tuple;
        int index = getWlanInterfaceIndex(i);
        tuple->iface_addr() = getIfaceAddressFromIndex(index);
        tuple->main_addr() = ra_addr();
        tuple->time() = simtime_t::getMaxTime().dbl();
        tuple->local_iface_index() = index;
        add_ifaceassoc_tuple(tuple);
    }
    hello_timer_.resched(hello_ival());
    tc_timer_.resched(hello_ival());
    mid_timer_.resched(hello_ival());
    // rescheduling the link quality timer with zero delay triggers a
    // cRuntimeError because the timer would fire at the current
    // simulation time. Schedule it with a very small positive delay
    // instead so the first execution happens immediately after
    // initialization.
    link_quality_timer_.resched(1e-6);
    scheduleEvent();
}

///
/// \brief  This function is called whenever a event  is received. It identifies
///     the type of the received event and process it accordingly.
///
/// If it is an %FSR packet then it is processed. In other case, if it is a data packet
/// then it is forwarded.
///
/// \param  p the received packet.
/// \param  h a handler (not used).
///

///
/// \brief Processes an incoming %FSR packet following RFC 3626 specification.
/// \param p received packet.
///
void
Fsr_Etx::recv_fsr(Packet* msg)
{
    nsaddr_t src_addr;
    int index;

    // All routing messages are sent from and to port RT_PORT,
    // so we check it.

// Extract information and delete the cantainer without more use
    bool proc = check_packet(msg, src_addr, index);
    if (!proc) {
        delete msg;
        return;
    }
    auto &op = msg->popAtFront<FsrPkt>();


    // If the packet contains no messages must be silently discarded.
    // There could exist a message with an empty body, so the size of
    // the packet would be pkt-hdr-size + msg-hdr-size.


    if (op->getChunkLength() < B(FSR_PKT_HDR_SIZE + FSR_MSG_HDR_SIZE)) {
        delete msg;
        return;
    }


// Process Fsr information
    assert(op->msgArraySize() >= 0 && op->msgArraySize() <= FSR_MAX_MSGS);
    nsaddr_t receiverIfaceAddr = getIfaceAddressFromIndex(index);
    for (int i = 0; i < (int) op->msgArraySize(); i++) {
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
        FSR_ETX_dup_tuple* duplicated = state_.find_dup_tuple(msg.orig_addr(), msg.msg_seq_num());
        if (duplicated == nullptr)
        {
            // Process the message according to its type
            if (msg.msg_type() == FSR_HELLO_MSG)
                process_hello(msg, receiverIfaceAddr, src_addr, op->pkt_seq_num(), index);
            else if (msg.msg_type() == FSR_TC_MSG)
                process_tc(msg, src_addr, index);
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
                if (*it == receiverIfaceAddr)
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
                forward_default(msg, duplicated, receiverIfaceAddr, src_addr);
        }

    }
// Link delay extension
    if (parameter_.link_delay() && op->sn() > 0)
    {
        Fsr_Etx_link_tuple *link_tuple = nullptr;
        Fsr_link_tuple *link_aux = state_.find_link_tuple(src_addr);
        if (link_aux)
        {
            link_tuple = dynamic_cast<Fsr_Etx_link_tuple*> (link_aux);
            if (!link_tuple)
                throw cRuntimeError("\n Error conversion link tuple to link ETX tuple");
        }
        if (link_tuple) {
            FsrPkt opAux = *op;
            link_tuple->link_delay_computation(&opAux);
        }
    }

    // After processing all FSR_ETX messages, we must recompute routing table
    switch (parameter_.routing_algorithm())
    {
    case FSR_ETX_DIJKSTRA_ALGORITHM:
        rtable_dijkstra_computation();
        break;

    default:
    case FSR_ETX_DEFAULT_ALGORITHM:
        rtable_default_computation();
        break;
    }
    delete msg;

}

///
/// \brief Computates MPR set of a node following RFC 3626 hints.
///
// Hinerit
//void
//FSR::mpr_computation()
void
Fsr_Etx::fsr_mpr_computation()
{
    mpr_computation();
}



///
/// \brief Computates MPR set of a node.
///
void
Fsr_Etx::fsr_r1_mpr_computation()
{
    // For further details please refer to paper
    // Quality of Service Routing in Ad Hoc Networks Using FSR


    bool increment;
    state_.clear_mprset();

    nbset_t N; nb2hopset_t N2;
    // N is the subset of neighbors of the node, which are
    // neighbor "of the interface I" and have willigness different
    // from FSR_ETX_WILL_NEVER
    for (auto it = nbset().begin(); it != nbset().end(); it++)
        if ((*it)->getStatus() == FSR_ETX_STATUS_SYM) // I think that we need this check
            N.push_back(*it);

    // N2 is the set of 2-hop neighbors reachable from "the interface
    // I", excluding:
    // (i)   the nodes only reachable by members of N with willingness WILL_NEVER
    // (ii)  the node performing the computation
    // (iii) all the symmetric neighbors: the nodes for which there exists a symmetric
    //       link to this node on some interface.
    for (auto it = nb2hopset().begin(); it != nb2hopset().end(); it++)
    {
        FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>( *it);


        if (!nb2hop_tuple)
            throw cRuntimeError("\n Error conversion nd2hop tuple");



        if (isLocalAddress(nb2hop_tuple->nb2hop_addr()))
        {
            continue;
        }
        // excluding:
        // (i) the nodes only reachable by members of N with willingness WILL_NEVER
        bool ok = false;
        for (nbset_t::const_iterator it2 = N.begin(); it2 != N.end(); it2++)
        {
            Fsr_nb_tuple* neigh = *it2;
            if (neigh->nb_main_addr() == nb2hop_tuple->nb_main_addr())
            {
                if (neigh->willingness() == FSR_WILL_NEVER)
                {
                    ok = false;
                    break;
                }
                else
                {
                    ok = true;
                    break;
                }
            }
        }
        if (!ok)
        {
            continue;
        }

        // excluding:
        // (iii) all the symmetric neighbors: the nodes for which there exists a symmetric
        //       link to this node on some interface.
        for (auto it2 = N.begin(); it2 != N.end(); it2++)
        {
            FSR_ETX_nb_tuple* neigh =dynamic_cast<FSR_ETX_nb_tuple*>(*it2);
            if (neigh == nullptr)
                throw cRuntimeError("Error in tupe");
            if (neigh->nb_main_addr() == nb2hop_tuple->nb2hop_addr())
            {
                ok = false;
                break;
            }
        }

        if (ok)
            N2.push_back(nb2hop_tuple);
    }

    // Start with an MPR set made of all members of N with
    // N_willingness equal to WILL_ALWAYS
    for (auto it = N.begin(); it != N.end(); it++)
    {
        FSR_ETX_nb_tuple* nb_tuple = *it;
        if (nb_tuple->willingness() == FSR_ETX_WILL_ALWAYS)
            state_.insert_mpr_addr(nb_tuple->nb_main_addr());
    }

    // Add to Mi the nodes in N which are the only nodes to provide reachability
    // to a node in N2. Remove the nodes from N2 which are now covered by
    // a node in the MPR set.
    mprset_t foundset;
    std::set<nsaddr_t> deleted_addrs;
    // iterate through all 2 hop neighbors we have
    for (auto it = N2.begin(); it != N2.end();)
    {
        FSR_ETX_nb2hop_tuple* nb2hop_tuple1 = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
        if (!nb2hop_tuple1)
            throw cRuntimeError("\n Error conversion nd2hop tuple");

        increment = true;

        // check if this two hop neighbor has more that one hop neighbor in N
        // it would mean that there is more than one node in N that reaches
        // the current 2 hop node
        auto pos = foundset.find(nb2hop_tuple1->nb2hop_addr());
        if (pos != foundset.end())
        {
            it++;
            continue;
        }

        bool found = false;
        // find the one hop neighbor that provides reachability to the
        // current two hop neighbor.
        for (auto it2 = N.begin(); it2 != N.end(); it2++)
        {
            if ((*it2)->nb_main_addr() == nb2hop_tuple1->nb_main_addr())
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            it++;
            continue;
        }

        found = false;
        // check if there is another one hop neighbor able to provide
        // reachability to the current 2 hop neighbor
        for (auto it2 = it + 1; it2 != N2.end(); it2++)
        {
            FSR_ETX_nb2hop_tuple* nb2hop_tuple2 = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it2);
            if (!nb2hop_tuple2)
                throw cRuntimeError("\n Error conversion nd2hop tuple");

            if (nb2hop_tuple1->nb2hop_addr() == nb2hop_tuple2->nb2hop_addr())
            {
                foundset.insert(nb2hop_tuple1->nb2hop_addr());
                found = true;
                break;
            }
        }
        // if there is only one node, add our one hop neighbor to the MPR set
        if (!found)
        {
            state_.insert_mpr_addr(nb2hop_tuple1->nb_main_addr());

            // erase all 2 hop neighbor nodes that are now reached through this
            // newly added MPR
            for (auto it2 = it + 1; it2 != N2.end();)
            {
                FSR_ETX_nb2hop_tuple* nb2hop_tuple2 = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it2);
                if (!nb2hop_tuple2)
                    throw cRuntimeError("\n Error conversion nd2hop tuple");

                if (nb2hop_tuple1->nb_main_addr() == nb2hop_tuple2->nb_main_addr())
                {
                    deleted_addrs.insert(nb2hop_tuple2->nb2hop_addr());
                    it2 = N2.erase(it2);
                }
                else
                    it2++;
            }
            int distanceFromEnd = std::distance(it, N2.end());
            int distance = std::distance(N2.begin(), it);
            int i = 0;
            for (auto it2 = N2.begin(); i < distance; i++) // check now the first section
            {

                Fsr_nb2hop_tuple* nb2hop_tuple2 = *it2;
                if (nb2hop_tuple1->nb_main_addr() == nb2hop_tuple2->nb_main_addr())
                {
                    deleted_addrs.insert(nb2hop_tuple2->nb2hop_addr());
                    it2 = N2.erase(it2);
                }
                else
                    it2++;

            }
            it = N2.end() - distanceFromEnd; // the standard doesn't guarantee that the iterator is valid if we have delete something in the vector, reload the iterator.

            it = N2.erase(it);
            increment = false;
        }

        // erase all 2 hop neighbor nodes that are now reached through this
        // newly added MPR. We are now looking for the backup links
        for (auto it2 = deleted_addrs.begin();
                it2 != deleted_addrs.end(); it2++)
        {
            for (auto it3 = N2.begin(); it3 != N2.end();)
            {
                if ((*it3)->nb2hop_addr() == *it2)
                {
                    it3 = N2.erase(it3);
                    // I have to reset the external iterator because it
                    // may have been invalidated by the latter deletion
                    it = N2.begin();
                    increment = false;
                }
                else
                    it3++;
            }
        }
        deleted_addrs.clear();
        if (increment)
            it++;
    }

    // While there exist nodes in N2 which are not covered by at
    // least one node in the MPR set:
    while (N2.begin() != N2.end())
    {
        // For each node in N, calculate the reachability, i.e., the
        // number of nodes in N2 that it can reach
        std::map<int, std::vector<FSR_ETX_nb_tuple*> > reachability;
        std::set<int> rs;
        for (auto it = N.begin(); it != N.end(); it++)
        {
            FSR_ETX_nb_tuple* nb_tuple = *it;
            int r = 0;
            for (auto it2 = N2.begin(); it2 != N2.end(); it2++)
            {
                FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it2);
                if (!nb2hop_tuple)
                    throw cRuntimeError("\n Error conversion nd2hop tuple");


                if (nb_tuple->nb_main_addr() == nb2hop_tuple->nb_main_addr())
                    r++;
            }
            rs.insert(r);
            reachability[r].push_back(nb_tuple);
        }

        // Select as a MPR the node with highest N_willingness among
        // the nodes in N with non-zero reachability. In case of
        // multiple choice select the node which provides
        // reachability to the maximum number of nodes in N2. In
        // case of multiple choices select the node with best conectivity
        // to the current node. Remove the nodes from N2 which are now covered
        // by a node in the MPR set.
        FSR_ETX_nb_tuple* max = nullptr;
        int max_r = 0;
        for (auto it = rs.begin(); it != rs.end(); it++)
        {
            int r = *it;
            if (r > 0)
            {
                for (auto it2 = reachability[r].begin();
                        it2 != reachability[r].end(); it2++)
                {
                    FSR_ETX_nb_tuple* nb_tuple = *it2;
                    if (max == nullptr || nb_tuple->willingness() > max->willingness())
                    {
                        max = nb_tuple;
                        max_r = r;
                    }
                    else if (nb_tuple->willingness() == max->willingness())
                    {
                        if (r > max_r)
                        {
                            max = nb_tuple;
                            max_r = r;
                        }
                        else if (r == max_r)
                        {
                            Fsr_Etx_link_tuple *nb_link_tuple = nullptr, *max_link_tuple = nullptr;
                            Fsr_link_tuple * link_tuple_aux;
                            double now = CURRENT_TIME;
                            link_tuple_aux = state_.find_sym_link_tuple(nb_tuple->nb_main_addr(), now);
                            if (link_tuple_aux)
                            {
                                nb_link_tuple = dynamic_cast<Fsr_Etx_link_tuple *>(link_tuple_aux);
                                if (!nb_link_tuple)
                                    throw cRuntimeError("\n Error conversion link tuple");
                            }
                            link_tuple_aux = state_.find_sym_link_tuple(max->nb_main_addr(), now);
                            if (link_tuple_aux)
                            {
                                max_link_tuple = dynamic_cast<Fsr_Etx_link_tuple *>(link_tuple_aux);
                                if (!max_link_tuple)
                                    throw cRuntimeError("\n Error conversion link tuple");
                            }
                            if (parameter_.link_delay())
                            {
                                if (nb_link_tuple == nullptr)
                                    continue;
                                else if (nb_link_tuple != nullptr && max_link_tuple == nullptr)
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                    continue;
                                }
                                if (nb_link_tuple->link_delay() < max_link_tuple->link_delay())
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                }
                            }
                            else
                            {
                                switch (parameter_.link_quality())
                                {
                                case FSR_ETX_BEHAVIOR_ETX:
                                    if (nb_link_tuple == nullptr)
                                        continue;
                                    else if (nb_link_tuple != nullptr && max_link_tuple == nullptr)
                                    {
                                        max = nb_tuple;
                                        max_r = r;
                                        continue;
                                    }
                                    if (nb_link_tuple->etx() < max_link_tuple->etx())
                                    {
                                        max = nb_tuple;
                                        max_r = r;
                                    }
                                    break;
                                case FSR_ETX_BEHAVIOR_ML:
                                    if (nb_link_tuple == nullptr)
                                        continue;
                                    else if (nb_link_tuple != nullptr && max_link_tuple == nullptr)
                                    {
                                        max = nb_tuple;
                                        max_r = r;
                                        continue;
                                    }
                                    if (nb_link_tuple->etx() > max_link_tuple->etx())
                                    {
                                        max = nb_tuple;
                                        max_r = r;
                                    }
                                    break;
                                case FSR_ETX_BEHAVIOR_NONE:
                                default:
                                    // max = nb_tuple;
                                    // max_r = r;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        if (max != nullptr)
        {
            state_.insert_mpr_addr(max->nb_main_addr());
            std::set<nsaddr_t> nb2hop_addrs;
            for (auto it = N2.begin(); it != N2.end();)
            {
                FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
                if (!nb2hop_tuple)
                    throw cRuntimeError("\n Error conversion nd2hop tuple");


                if (nb2hop_tuple->nb_main_addr() == max->nb_main_addr())
                {
                    nb2hop_addrs.insert(nb2hop_tuple->nb2hop_addr());
                    it = N2.erase(it);
                }
                else
                    it++;
            }
            for (auto it = N2.begin(); it != N2.end();)
            {
                FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
                if (!nb2hop_tuple)
                    throw cRuntimeError("\n Error conversion nd2hop tuple");


                auto it2 =
                    nb2hop_addrs.find(nb2hop_tuple->nb2hop_addr());
                if (it2 != nb2hop_addrs.end())
                {
                    it = N2.erase(it);
                }
                else
                    it++;

            }
        }
    }
}
#if 0
///
/// \brief Computates MPR set of a node.
///
void
Fsr_Etx::fsr_r2_mpr_computation()
{
    // For further details please refer to paper
    // Quality of Service Routing in Ad Hoc Networks Using FSR

    state_.clear_mprset();

    nbset_t N; nb2hopset_t N2;

    // N is the subset of neighbors of the node, which are
    // neighbor "of the interface I"
    for (auto it = nbset().begin(); it != nbset().end(); it++)
        if ((*it)->getStatus() == FSR_ETX_STATUS_SYM &&
                (*it)->willingness() != FSR_ETX_WILL_NEVER) // I think that we need this check
            N.push_back(*it);

    // N2 is the set of 2-hop neighbors reachable from "the interface
    // I", excluding:
    // (i)   the nodes only reachable by members of N with willingness WILL_NEVER
    // (ii)  the node performing the computation
    // (iii) all the symmetric neighbors: the nodes for which there exists a symmetric
    //       link to this node on some interface.
    for (auto it = nb2hopset().begin(); it != nb2hopset().end(); it++)
    {
        FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
        if (!nb2hop_tuple)
            throw cRuntimeError("\n Error conversion nd2hop tuple");


        bool ok = true;
        FSR_ETX_nb_tuple* nb_tuple = state_.find_sym_nb_tuple(nb2hop_tuple->nb_main_addr());

        if (nb_tuple == nullptr)
            ok = false;
        else
        {
            nb_tuple = state_.find_nb_tuple(nb2hop_tuple->nb_main_addr(), FSR_ETX_WILL_NEVER);
            if (nb_tuple != nullptr)
                ok = false;
            else
            {
                nb_tuple = state_.find_sym_nb_tuple(nb2hop_tuple->nb2hop_addr());
                if (nb_tuple != nullptr)
                    ok = false;
            }
        }
        if (ok)
            N2.push_back(nb2hop_tuple);
    }
    // While there exist nodes in N2 which are not covered by at
    // least one node in the MPR set:
    while (N2.begin() != N2.end())
    {
        // For each node in N, calculate the reachability, i.e., the
        // number of nodes in N2 that it can reach
        std::map<int, std::vector<FSR_ETX_nb_tuple*> > reachability;
        std::set<int> rs;
        for (auto it = N.begin(); it != N.end(); it++)
        {
            FSR_ETX_nb_tuple* nb_tuple = *it;
            int r = 0;
            for (auto it2 = N2.begin(); it2 != N2.end(); it2++)
            {
                FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it2);
                if (!nb2hop_tuple)
                    throw cRuntimeError("\n Error conversion nd2hop tuple");


                if (nb_tuple->nb_main_addr() == nb2hop_tuple->nb_main_addr())
                    r++;
            }
            rs.insert(r);
            reachability[r].push_back(nb_tuple);
        }

        // Add to Mi the node in N that has the best link to the current
        // node. In case of tie, select tin N2. Remove the nodes from N2
        // which are now covered by a node in the MPR set.
        FSR_ETX_nb_tuple* max = nullptr;
        int max_r = 0;
        for (auto it = rs.begin(); it != rs.end(); it++)
        {
            int r = *it;
            if (r > 0)
            {
                for (auto it2 = reachability[r].begin();
                        it2 != reachability[r].end(); it2++)
                {
                    FSR_ETX_nb_tuple* nb_tuple = *it2;
                    if (max == nullptr)
                    {
                        max = nb_tuple;
                        max_r = r;
                    }
                    else
                    {
                        Fsr_Etx_link_tuple *nb_link_tuple = nullptr, *max_link_tuple = nullptr;
                        Fsr_link_tuple *link_tuple_aux;
                        double now = CURRENT_TIME;

                        link_tuple_aux = state_.find_sym_link_tuple(nb_tuple->nb_main_addr(), now);
                        if (link_tuple_aux)
                        {
                            nb_link_tuple = dynamic_cast<Fsr_Etx_link_tuple *>(link_tuple_aux);
                            if (!nb_link_tuple)
                                throw cRuntimeError("\n Error conversion link tuple");
                        }
                        link_tuple_aux = state_.find_sym_link_tuple(max->nb_main_addr(), now);
                        if (link_tuple_aux)
                        {
                            max_link_tuple = dynamic_cast<Fsr_Etx_link_tuple *>(link_tuple_aux);
                            if (!max_link_tuple)
                                throw cRuntimeError("\n Error conversion link tuple");
                        }
                        if (nb_link_tuple || max_link_tuple)
                            continue;
                        switch (parameter_.link_quality())
                        {
                        case FSR_ETX_BEHAVIOR_ETX:
                            if (nb_link_tuple->etx() < max_link_tuple->etx())
                            {
                                max = nb_tuple;
                                max_r = r;
                            }
                            else if (nb_link_tuple->etx() == max_link_tuple->etx())
                            {
                                if (r > max_r)
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                }
                                else if (r == max_r && degree(nb_tuple) > degree(max))
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                }
                            }
                            break;
                        case FSR_ETX_BEHAVIOR_ML:
                            if (nb_link_tuple->etx() > max_link_tuple->etx())
                            {
                                max = nb_tuple;
                                max_r = r;
                            }
                            else if (nb_link_tuple->etx() == max_link_tuple->etx())
                            {
                                if (r > max_r)
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                }
                                else if (r == max_r && degree(nb_tuple) > degree(max))
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                }
                            }
                            break;
                        case FSR_ETX_BEHAVIOR_NONE:
                        default:
                            if (r > max_r)
                            {
                                max = nb_tuple;
                                max_r = r;
                            }
                            else if (r == max_r && degree(nb_tuple) > degree(max))
                            {
                                max = nb_tuple;
                                max_r = r;
                            }
                            break;
                        }
                    }
                }
            }
        }
        if (max != nullptr)
        {
            state_.insert_mpr_addr(max->nb_main_addr());
            std::set<nsaddr_t> nb2hop_addrs;
            for (auto it = N2.begin(); it != N2.end();)
            {
                FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
                if (!nb2hop_tuple)
                    throw cRuntimeError("\n Error conversion nd2hop tuple");

                if (nb2hop_tuple->nb_main_addr() == max->nb_main_addr())
                {
                    nb2hop_addrs.insert(nb2hop_tuple->nb2hop_addr());
                    it = N2.erase(it);
                }
                else
                    it++;

            }
            for (auto it = N2.begin(); it != N2.end();)
            {
                FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
                if (!nb2hop_tuple)
                    throw cRuntimeError("\n Error conversion nd2hop tuple");

                auto it2 =
                    nb2hop_addrs.find(nb2hop_tuple->nb2hop_addr());
                if (it2 != nb2hop_addrs.end())
                {
                    it = N2.erase(it);
                }
                else
                    it++;

            }
        }
    }
}

///
/// \brief Computates MPR set of a node.
///
void
Fsr_Etx::qfsr_mpr_computation()
{
    // For further details please refer to paper
    // QoS Routing in FSR with Several Classes of Services

    state_.clear_mprset();

    nbset_t N; nb2hopset_t N2;
    // N is the subset of neighbors of the node, which are
    // neighbor "of the interface I"
    for (auto it = nbset().begin(); it != nbset().end(); it++)
        if ((*it)->getStatus() == FSR_ETX_STATUS_SYM &&
                (*it)->willingness() != FSR_ETX_WILL_NEVER) // I think that we need this check
            N.push_back(*it);

    // N2 is the set of 2-hop neighbors reachable from "the interface
    // I", excluding:
    // (i)   the nodes only reachable by members of N with willingness WILL_NEVER
    // (ii)  the node performing the computation
    // (iii) all the symmetric neighbors: the nodes for which there exists a symmetric
    //       link to this node on some interface.
    for (auto it = nb2hopset().begin(); it != nb2hopset().end(); it++)
    {
        FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
        if (!nb2hop_tuple)
            throw cRuntimeError("\n Error conversion nd2hop tuple");

        bool ok = true;
        FSR_ETX_nb_tuple* nb_tuple = state_.find_sym_nb_tuple(nb2hop_tuple->nb_main_addr());
        if (nb_tuple == nullptr)
            ok = false;
        else
        {
            nb_tuple = state_.find_nb_tuple(nb2hop_tuple->nb_main_addr(), FSR_ETX_WILL_NEVER);
            if (nb_tuple != nullptr)
                ok = false;
            else
            {
                nb_tuple = state_.find_sym_nb_tuple(nb2hop_tuple->nb2hop_addr());
                if (nb_tuple != nullptr)
                    ok = false;
            }
        }
        if (ok)
            N2.push_back(nb2hop_tuple);
    }
    // While there exist nodes in N2 which are not covered by at
    // least one node in the MPR set:
    while (N2.begin() != N2.end())
    {
        // For each node in N, calculate the reachability, i.e., the
        // number of nodes in N2 that it can reach
        std::map<int, std::vector<FSR_ETX_nb_tuple*> > reachability;
        std::set<int> rs;
        for (auto it = N.begin(); it != N.end(); it++)
        {
            FSR_ETX_nb_tuple* nb_tuple = *it;
            int r = 0;
            for (auto it2 = N2.begin(); it2 != N2.end(); it2++)
            {
                FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it2);
                if (!nb2hop_tuple)
                    throw cRuntimeError("\n Error conversion nd2hop tuple");

                if (nb_tuple->nb_main_addr() == nb2hop_tuple->nb_main_addr())
                    r++;
            }
            rs.insert(r);
            reachability[r].push_back(nb_tuple);
        }
        // Select a node z from N2
        Fsr_nb2hop_tuple* t_aux = *(N2.begin());
        FSR_ETX_nb2hop_tuple* z = nullptr;
        if (t_aux)
        {
            z = dynamic_cast<FSR_ETX_nb2hop_tuple*> (t_aux);
            if (!z)
                throw cRuntimeError("\n Error conversion nd2hop tuple");
        }

        // Add to Mi, if not yet present, the node in N that provides the
        // shortest-widest path to reach z. In case of tie, select the node
        // that reaches the maximum number of nodes in N2. Remove the nodes from N2
        // which are now covered by a node in the MPR set.
        FSR_ETX_nb_tuple* max = nullptr;
        int max_r = 0;

        // Iterate through all links in nb2hop_set that has the same two hop
        // neighbor as the second point of the link
        for (auto it = N2.begin(); it != N2.end(); it++)
        {
            FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
            if (!nb2hop_tuple)
                throw cRuntimeError("\n Error conversion nd2hop tuple");


            // If the two hop neighbor is not the one we have selected, skip
            if (nb2hop_tuple->nb2hop_addr() != z->nb2hop_addr())
                continue;
            // Compare the one hop neighbor that reaches the two hop neighbor z with
            for (auto it2 = rs.begin(); it2 != rs.end(); it2++)
            {
                int r = *it2;
                if (r > 0)
                {
                    for (auto it3 = reachability[r].begin();
                            it3 != reachability[r].end(); it3++)
                    {
                        FSR_ETX_nb_tuple* nb_tuple = *it3;
                        if (nb2hop_tuple->nb_main_addr() != nb_tuple->nb_main_addr())
                            continue;
                        if (max == nullptr)
                        {
                            max = nb_tuple;
                            max_r = r;
                        }
                        else
                        {
                            Fsr_Etx_link_tuple *nb_link_tuple = nullptr, *max_link_tuple = nullptr;
                            double now = CURRENT_TIME;


                            Fsr_link_tuple *link_tuple_aux = state_.find_sym_link_tuple(nb_tuple->nb_main_addr(), now); /* bug */
                            if (link_tuple_aux)
                            {
                                nb_link_tuple = dynamic_cast<Fsr_Etx_link_tuple *>(link_tuple_aux);
                                if (!nb_link_tuple)
                                    throw cRuntimeError("\n Error conversion link tuple");
                            }
                            link_tuple_aux = state_.find_sym_link_tuple(max->nb_main_addr(), now); /* bug */
                            if (link_tuple_aux)
                            {
                                max_link_tuple = dynamic_cast<Fsr_Etx_link_tuple *>(link_tuple_aux);
                                if (!max_link_tuple)
                                    throw cRuntimeError("\n Error conversion link tuple");
                            }

                            double current_total_etx, max_total_etx;

                            switch (parameter_.link_quality())
                            {
                            case FSR_ETX_BEHAVIOR_ETX:
                                current_total_etx = nb_link_tuple->etx() + nb2hop_tuple->etx();
                                max_total_etx = max_link_tuple->etx() + nb2hop_tuple->etx();
                                if (current_total_etx < max_total_etx)
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                }
                                else if (current_total_etx == max_total_etx)
                                {
                                    if (r > max_r)
                                    {
                                        max = nb_tuple;
                                        max_r = r;
                                    }
                                    else if (r == max_r && degree(nb_tuple) > degree(max))
                                    {
                                        max = nb_tuple;
                                        max_r = r;
                                    }
                                }
                                break;

                            case FSR_ETX_BEHAVIOR_ML:
                                current_total_etx = nb_link_tuple->etx() * nb2hop_tuple->etx();
                                max_total_etx = max_link_tuple->etx() * nb2hop_tuple->etx();
                                if (current_total_etx > max_total_etx)
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                }
                                else if (current_total_etx == max_total_etx)
                                {
                                    if (r > max_r)
                                    {
                                        max = nb_tuple;
                                        max_r = r;
                                    }
                                    else if (r == max_r && degree(nb_tuple) > degree(max))
                                    {
                                        max = nb_tuple;
                                        max_r = r;
                                    }
                                }
                                break;
                            case FSR_ETX_BEHAVIOR_NONE:
                            default:
                                if (r > max_r)
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                }
                                else if (r == max_r && degree(nb_tuple) > degree(max))
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (max != nullptr)
        {
            state_.insert_mpr_addr(max->nb_main_addr());
            std::set<nsaddr_t> nb2hop_addrs;
            for (auto it = N2.begin(); it != N2.end();)
            {
                FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
                if (!nb2hop_tuple)
                    throw cRuntimeError("\n Error conversion nd2hop tuple");

                if (nb2hop_tuple->nb_main_addr() == max->nb_main_addr())
                {
                    nb2hop_addrs.insert(nb2hop_tuple->nb2hop_addr());
                    it = N2.erase(it);
                }
                else
                    it++;

            }
            for (auto it = N2.begin(); it != N2.end();)
            {
                FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
                if (!nb2hop_tuple)
                    throw cRuntimeError("\n Error conversion nd2hop tuple");


                auto it2 =
                    nb2hop_addrs.find(nb2hop_tuple->nb2hop_addr());
                if (it2 != nb2hop_addrs.end())
                {
                    it = N2.erase(it);
                }
                else
                    it++;
            }
        }
    }
}
#else

///
/// \brief Computates MPR set of a node.
///
void
Fsr_Etx::fsr_r2_mpr_computation()
{
    // For further details please refer to paper
    // Quality of Service Routing in Ad Hoc Networks Using FSR

    state_.clear_mprset();

    nbset_t N; nb2hopset_t N2;

    // N is the subset of neighbors of the node, which are
    // neighbor "of the interface I"
    for (auto it = nbset().begin(); it != nbset().end(); it++)
        if ((*it)->getStatus() == FSR_ETX_STATUS_SYM &&
                (*it)->willingness() != FSR_ETX_WILL_NEVER) // I think that we need this check
            N.push_back(*it);

    // N2 is the set of 2-hop neighbors reachable from "the interface
    // I", excluding:
    // (i)   the nodes only reachable by members of N with willingness WILL_NEVER
    // (ii)  the node performing the computation
    // (iii) all the symmetric neighbors: the nodes for which there exists a symmetric
    //       link to this node on some interface.

    for (auto it = nb2hopset().begin(); it != nb2hopset().end(); it++)
    {
        FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
        if (!nb2hop_tuple)
            throw cRuntimeError("\n Error conversion nd2hop tuple");

        // (ii)  the node performing the computation
        if (isLocalAddress(nb2hop_tuple->nb2hop_addr()))
        {
            continue;
        }
        // excluding:
        // (i) the nodes only reachable by members of N with willingness WILL_NEVER
        bool ok = false;
        for (nbset_t::const_iterator it2 = N.begin(); it2 != N.end(); it2++)
        {
            Fsr_nb_tuple* neigh = *it2;
            if (neigh->nb_main_addr() == nb2hop_tuple->nb_main_addr())
            {
                if (neigh->willingness() == FSR_WILL_NEVER)
                {
                    ok = false;
                    break;
                }
                else
                {
                    ok = true;
                    break;
                }
            }
        }
        if (!ok)
        {
            continue;
        }

        // excluding:
        // (iii) all the symmetric neighbors: the nodes for which there exists a symmetric
        //       link to this node on some interface.
        for (auto it2 = N.begin(); it2 != N.end(); it2++)
        {
            Fsr_nb_tuple* neigh = *it2;
            if (neigh->nb_main_addr() == nb2hop_tuple->nb2hop_addr())
            {
                ok = false;
                break;
            }
        }
        if (ok)
            N2.push_back(nb2hop_tuple);
    }
    // 1. Start with an MPR set made of all members of N with
    // N_willingness equal to WILL_ALWAYS
    for (auto it = N.begin(); it != N.end(); it++)
    {
        Fsr_nb_tuple* nb_tuple = *it;
        if (nb_tuple->willingness() == FSR_WILL_ALWAYS)
        {
            state_.insert_mpr_addr(nb_tuple->nb_main_addr());
            // (not in RFC but I think is needed: remove the 2-hop
            // neighbors reachable by the MPR from N2)
            CoverTwoHopNeighbors (nb_tuple->nb_main_addr(), N2);
        }
    }

    // 2. Calculate D(y), where y is a member of N, for all nodes in N.
    // We will do this later.

    // 3. Add to the MPR set those nodes in N, which are the *only*
    // nodes to provide reachability to a node in N2. Remove the
    // nodes from N2 which are now covered by a node in the MPR set.

    std::set<nsaddr_t> coveredTwoHopNeighbors;
    for (auto it = N2.begin(); it != N2.end(); it++)
    {
        Fsr_nb2hop_tuple* twoHopNeigh = *it;
        bool onlyOne = true;
        // try to find another neighbor that can reach twoHopNeigh->twoHopNeighborAddr
        for (nb2hopset_t::const_iterator it2 = N2.begin(); it2 != N2.end(); it2++)
        {
            Fsr_nb2hop_tuple* otherTwoHopNeigh = *it2;
            if (otherTwoHopNeigh->nb2hop_addr() == twoHopNeigh->nb2hop_addr()
                    && otherTwoHopNeigh->nb_main_addr() != twoHopNeigh->nb_main_addr())
            {
                onlyOne = false;
                break;
            }
        }
        if (onlyOne)
        {
            state_.insert_mpr_addr(twoHopNeigh->nb_main_addr());

            // take note of all the 2-hop neighbors reachable by the newly elected MPR
            for (nb2hopset_t::const_iterator it2 = N2.begin(); it2 != N2.end(); it2++)
            {
                Fsr_nb2hop_tuple* otherTwoHopNeigh = *it2;
                if (otherTwoHopNeigh->nb_main_addr() == twoHopNeigh->nb_main_addr())
                {
                    coveredTwoHopNeighbors.insert(otherTwoHopNeigh->nb2hop_addr());
                }
            }
        }
    }

    // While there exist nodes in N2 which are not covered by at
    // least one node in the MPR set:
    while (N2.begin() != N2.end())
    {
        // For each node in N, calculate the reachability, i.e., the
        // number of nodes in N2 that it can reach
        std::map<int, std::vector<FSR_ETX_nb_tuple*> > reachability;
        std::set<int> rs;
        for (auto it = N.begin(); it != N.end(); it++)
        {
            FSR_ETX_nb_tuple* nb_tuple = *it;
            int r = 0;
            for (auto it2 = N2.begin(); it2 != N2.end(); it2++)
            {
                FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it2);
                if (!nb2hop_tuple)
                    throw cRuntimeError("\n Error conversion nd2hop tuple");


                if (nb_tuple->nb_main_addr() == nb2hop_tuple->nb_main_addr())
                    r++;
            }
            rs.insert(r);
            reachability[r].push_back(nb_tuple);
        }

        // Add to Mi the node in N that has the best link to the current
        // node. In case of tie, select tin N2. Remove the nodes from N2
        // which are now covered by a node in the MPR set.
        FSR_ETX_nb_tuple* max = nullptr;
        int max_r = 0;
        for (auto it = rs.begin(); it != rs.end(); it++)
        {
            int r = *it;
            if (r > 0)
            {
                for (auto it2 = reachability[r].begin();
                        it2 != reachability[r].end(); it2++)
                {
                    FSR_ETX_nb_tuple* nb_tuple = *it2;
                    if (max == nullptr)
                    {
                        max = nb_tuple;
                        max_r = r;
                    }
                    else
                    {
                        Fsr_Etx_link_tuple *nb_link_tuple = nullptr, *max_link_tuple = nullptr;
                        Fsr_link_tuple *link_tuple_aux;
                        double now = CURRENT_TIME;

                        link_tuple_aux = state_.find_sym_link_tuple(nb_tuple->nb_main_addr(), now);
                        if (link_tuple_aux)
                        {
                            nb_link_tuple = dynamic_cast<Fsr_Etx_link_tuple *>(link_tuple_aux);
                            if (!nb_link_tuple)
                                throw cRuntimeError("\n Error conversion link tuple");
                        }
                        link_tuple_aux = state_.find_sym_link_tuple(max->nb_main_addr(), now);
                        if (link_tuple_aux)
                        {
                            max_link_tuple = dynamic_cast<Fsr_Etx_link_tuple *>(link_tuple_aux);
                            if (!max_link_tuple)
                                throw cRuntimeError("\n Error conversion link tuple");
                        }
                        switch (parameter_.link_quality())
                        {
                        case FSR_ETX_BEHAVIOR_ETX:
                            if (nb_link_tuple == nullptr)
                                continue;
                            else if (nb_link_tuple != nullptr && max_link_tuple == nullptr)
                            {
                                max = nb_tuple;
                                max_r = r;
                                continue;
                            }
                            if (nb_link_tuple->etx() < max_link_tuple->etx())
                            {
                                max = nb_tuple;
                                max_r = r;
                            }
                            else if (nb_link_tuple->etx() == max_link_tuple->etx())
                            {
                                if (r > max_r)
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                }
                                else if (r == max_r && degree(nb_tuple) > degree(max))
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                }
                            }
                            break;
                        case FSR_ETX_BEHAVIOR_ML:
                            if (nb_link_tuple == nullptr)
                                continue;
                            else if (nb_link_tuple != nullptr && max_link_tuple == nullptr)
                            {
                                max = nb_tuple;
                                max_r = r;
                                continue;
                            }
                            if (nb_link_tuple->etx() > max_link_tuple->etx())
                            {
                                max = nb_tuple;
                                max_r = r;
                            }
                            else if (nb_link_tuple->etx() == max_link_tuple->etx())
                            {
                                if (r > max_r)
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                }
                                else if (r == max_r && degree(nb_tuple) > degree(max))
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                }
                            }
                            break;
                        case FSR_ETX_BEHAVIOR_NONE:
                        default:
                            if (r > max_r)
                            {
                                max = nb_tuple;
                                max_r = r;
                            }
                            else if (r == max_r && degree(nb_tuple) > degree(max))
                            {
                                max = nb_tuple;
                                max_r = r;
                            }
                            break;
                        }
                    }
                }
            }
        }
        if (max != nullptr)
        {
            state_.insert_mpr_addr(max->nb_main_addr());
            std::set<nsaddr_t> nb2hop_addrs;
            for (auto it = N2.begin(); it != N2.end();)
            {
                FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
                if (!nb2hop_tuple)
                    throw cRuntimeError("\n Error conversion nd2hop tuple");

                if (nb2hop_tuple->nb_main_addr() == max->nb_main_addr())
                {
                    nb2hop_addrs.insert(nb2hop_tuple->nb2hop_addr());
                    it = N2.erase(it);
                }
                else
                    it++;

            }
            for (auto it = N2.begin(); it != N2.end();)
            {
                FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
                if (!nb2hop_tuple)
                    throw cRuntimeError("\n Error conversion nd2hop tuple");

                auto it2 =
                    nb2hop_addrs.find(nb2hop_tuple->nb2hop_addr());
                if (it2 != nb2hop_addrs.end())
                {
                    it = N2.erase(it);
                }
                else
                    it++;

            }
        }
    }
}

///
/// \brief Computates MPR set of a node.
///
void
Fsr_Etx::qfsr_mpr_computation()
{
    // For further details please refer to paper
    // QoS Routing in FSR with Several Classes of Services

    state_.clear_mprset();

    nbset_t N; nb2hopset_t N2;
    // N is the subset of neighbors of the node, which are
    // neighbor "of the interface I"
    for (auto it = nbset().begin(); it != nbset().end(); it++)
        if ((*it)->getStatus() == FSR_ETX_STATUS_SYM &&
                (*it)->willingness() != FSR_ETX_WILL_NEVER) // I think that we need this check
            N.push_back(*it);

    // N2 is the set of 2-hop neighbors reachable from "the interface
    // I", excluding:
    // (i)   the nodes only reachable by members of N with willingness WILL_NEVER
    // (ii)  the node performing the computation
    // (iii) all the symmetric neighbors: the nodes for which there exists a symmetric
    //       link to this node on some interface.
    for (auto it = nb2hopset().begin(); it != nb2hopset().end(); it++)
    {
        FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
        if (!nb2hop_tuple)
            throw cRuntimeError("\n Error conversion nd2hop tuple");

        // (ii)  the node performing the computation
        if (isLocalAddress(nb2hop_tuple->nb2hop_addr()))
        {
            continue;
        }
        // excluding:
        // (i) the nodes only reachable by members of N with willingness WILL_NEVER
        bool ok = false;
        for (nbset_t::const_iterator it2 = N.begin(); it2 != N.end(); it2++)
        {
            Fsr_nb_tuple* neigh = *it2;
            if (neigh->nb_main_addr() == nb2hop_tuple->nb_main_addr())
            {
                if (neigh->willingness() == FSR_WILL_NEVER)
                {
                    ok = false;
                    break;
                }
                else
                {
                    ok = true;
                    break;
                }
            }
        }
        if (!ok)
        {
            continue;
        }

        // excluding:
        // (iii) all the symmetric neighbors: the nodes for which there exists a symmetric
        //       link to this node on some interface.
        for (auto it2 = N.begin(); it2 != N.end(); it2++)
        {
            Fsr_nb_tuple* neigh = *it2;
            if (neigh->nb_main_addr() == nb2hop_tuple->nb2hop_addr())
            {
                ok = false;
                break;
            }
        }
        if (ok)
            N2.push_back(nb2hop_tuple);
    }


    // 1. Start with an MPR set made of all members of N with
    // N_willingness equal to WILL_ALWAYS
    for (auto it = N.begin(); it != N.end(); it++)
    {
        Fsr_nb_tuple* nb_tuple = *it;
        if (nb_tuple->willingness() == FSR_WILL_ALWAYS)
        {
            state_.insert_mpr_addr(nb_tuple->nb_main_addr());
            // (not in RFC but I think is needed: remove the 2-hop
            // neighbors reachable by the MPR from N2)
            CoverTwoHopNeighbors (nb_tuple->nb_main_addr(), N2);
        }
    }

    // 2. Calculate D(y), where y is a member of N, for all nodes in N.
    // We will do this later.

    // 3. Add to the MPR set those nodes in N, which are the *only*
    // nodes to provide reachability to a node in N2. Remove the
    // nodes from N2 which are now covered by a node in the MPR set.

    std::set<nsaddr_t> coveredTwoHopNeighbors;
    for (auto it = N2.begin(); it != N2.end(); it++)
    {
        Fsr_nb2hop_tuple* twoHopNeigh = *it;
        bool onlyOne = true;
        // try to find another neighbor that can reach twoHopNeigh->twoHopNeighborAddr
        for (nb2hopset_t::const_iterator it2 = N2.begin(); it2 != N2.end(); it2++)
        {
            Fsr_nb2hop_tuple* otherTwoHopNeigh = *it2;
            if (otherTwoHopNeigh->nb2hop_addr() == twoHopNeigh->nb2hop_addr()
                    && otherTwoHopNeigh->nb_main_addr() != twoHopNeigh->nb_main_addr())
            {
                onlyOne = false;
                break;
            }
        }
        if (onlyOne)
        {
            state_.insert_mpr_addr(twoHopNeigh->nb_main_addr());

            // take note of all the 2-hop neighbors reachable by the newly elected MPR
            for (nb2hopset_t::const_iterator it2 = N2.begin(); it2 != N2.end(); it2++)
            {
                Fsr_nb2hop_tuple* otherTwoHopNeigh = *it2;
                if (otherTwoHopNeigh->nb_main_addr() == twoHopNeigh->nb_main_addr())
                {
                    coveredTwoHopNeighbors.insert(otherTwoHopNeigh->nb2hop_addr());
                }
            }
        }
    }


    // While there exist nodes in N2 which are not covered by at
    // least one node in the MPR set:
    while (N2.begin() != N2.end())
    {
        // For each node in N, calculate the reachability, i.e., the
        // number of nodes in N2 that it can reach
        std::map<int, std::vector<FSR_ETX_nb_tuple*> > reachability;
        std::set<int> rs;
        for (auto it = N.begin(); it != N.end(); it++)
        {
            FSR_ETX_nb_tuple* nb_tuple = *it;
            int r = 0;
            for (auto it2 = N2.begin(); it2 != N2.end(); it2++)
            {
                FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it2);
                if (!nb2hop_tuple)
                    throw cRuntimeError("\n Error conversion nd2hop tuple");

                if (nb_tuple->nb_main_addr() == nb2hop_tuple->nb_main_addr())
                    r++;
            }
            rs.insert(r);
            reachability[r].push_back(nb_tuple);
        }
        // Select a node z from N2
        Fsr_nb2hop_tuple* t_aux = *(N2.begin());
        FSR_ETX_nb2hop_tuple* z = nullptr;
        if (t_aux)
        {
            z = dynamic_cast<FSR_ETX_nb2hop_tuple*> (t_aux);
            if (!z)
                throw cRuntimeError("\n Error conversion nd2hop tuple");
        }

        // Add to Mi, if not yet present, the node in N that provides the
        // shortest-widest path to reach z. In case of tie, select the node
        // that reaches the maximum number of nodes in N2. Remove the nodes from N2
        // which are now covered by a node in the MPR set.
        FSR_ETX_nb_tuple* max = nullptr;
        int max_r = 0;

        // Iterate through all links in nb2hop_set that has the same two hop
        // neighbor as the second point of the link
        for (auto it = N2.begin(); it != N2.end(); it++)
        {
            FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
            if (!nb2hop_tuple)
                throw cRuntimeError("\n Error conversion nd2hop tuple");


            // If the two hop neighbor is not the one we have selected, skip
            if (nb2hop_tuple->nb2hop_addr() != z->nb2hop_addr())
                continue;
            // Compare the one hop neighbor that reaches the two hop neighbor z with
            for (auto it2 = rs.begin(); it2 != rs.end(); it2++)
            {
                int r = *it2;
                if (r > 0)
                {
                    for (auto it3 = reachability[r].begin();
                            it3 != reachability[r].end(); it3++)
                    {
                        FSR_ETX_nb_tuple* nb_tuple = *it3;
                        if (nb2hop_tuple->nb_main_addr() != nb_tuple->nb_main_addr())
                            continue;
                        if (max == nullptr)
                        {
                            max = nb_tuple;
                            max_r = r;
                        }
                        else
                        {
                            Fsr_Etx_link_tuple *nb_link_tuple = nullptr, *max_link_tuple = nullptr;
                            double now = CURRENT_TIME;


                            Fsr_link_tuple *link_tuple_aux = state_.find_sym_link_tuple(nb_tuple->nb_main_addr(), now); /* bug */
                            if (link_tuple_aux)
                            {
                                nb_link_tuple = dynamic_cast<Fsr_Etx_link_tuple *>(link_tuple_aux);
                                if (!nb_link_tuple)
                                    throw cRuntimeError("\n Error conversion link tuple");
                            }
                            link_tuple_aux = state_.find_sym_link_tuple(max->nb_main_addr(), now); /* bug */
                            if (link_tuple_aux)
                            {
                                max_link_tuple = dynamic_cast<Fsr_Etx_link_tuple *>(link_tuple_aux);
                                if (!max_link_tuple)
                                    throw cRuntimeError("\n Error conversion link tuple");
                            }

                            double current_total_etx, max_total_etx;

                            switch (parameter_.link_quality())
                            {
                            case FSR_ETX_BEHAVIOR_ETX:
                                if (nb_link_tuple == nullptr || max_link_tuple == nullptr)
                                    continue;
                                current_total_etx = nb_link_tuple->etx() + nb2hop_tuple->etx();
                                max_total_etx = max_link_tuple->etx() + nb2hop_tuple->etx();
                                if (current_total_etx < max_total_etx)
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                }
                                else if (current_total_etx == max_total_etx)
                                {
                                    if (r > max_r)
                                    {
                                        max = nb_tuple;
                                        max_r = r;
                                    }
                                    else if (r == max_r && degree(nb_tuple) > degree(max))
                                    {
                                        max = nb_tuple;
                                        max_r = r;
                                    }
                                }
                                break;

                            case FSR_ETX_BEHAVIOR_ML:
                                if (nb_link_tuple == nullptr || max_link_tuple == nullptr)
                                    continue;
                                current_total_etx = nb_link_tuple->etx() * nb2hop_tuple->etx();
                                max_total_etx = max_link_tuple->etx() * nb2hop_tuple->etx();
                                if (current_total_etx > max_total_etx)
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                }
                                else if (current_total_etx == max_total_etx)
                                {
                                    if (r > max_r)
                                    {
                                        max = nb_tuple;
                                        max_r = r;
                                    }
                                    else if (r == max_r && degree(nb_tuple) > degree(max))
                                    {
                                        max = nb_tuple;
                                        max_r = r;
                                    }
                                }
                                break;
                            case FSR_ETX_BEHAVIOR_NONE:
                            default:
                                if (r > max_r)
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                }
                                else if (r == max_r && degree(nb_tuple) > degree(max))
                                {
                                    max = nb_tuple;
                                    max_r = r;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (max != nullptr)
        {
            state_.insert_mpr_addr(max->nb_main_addr());
            std::set<nsaddr_t> nb2hop_addrs;
            for (auto it = N2.begin(); it != N2.end();)
            {
                FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
                if (!nb2hop_tuple)
                    throw cRuntimeError("\n Error conversion nd2hop tuple");

                if (nb2hop_tuple->nb_main_addr() == max->nb_main_addr())
                {
                    nb2hop_addrs.insert(nb2hop_tuple->nb2hop_addr());
                    it = N2.erase(it);
                }
                else
                    it++;

            }
            for (auto it = N2.begin(); it != N2.end();)
            {
                FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
                if (!nb2hop_tuple)
                    throw cRuntimeError("\n Error conversion nd2hop tuple");


                auto it2 =
                    nb2hop_addrs.find(nb2hop_tuple->nb2hop_addr());
                if (it2 != nb2hop_addrs.end())
                {
                    it = N2.erase(it);
                }
                else
                    it++;
            }
        }
    }
}
#endif
///
/// \brief Computates MPR set of a node.
///
void
Fsr_Etx::fsrd_mpr_computation()
{
    // MPR computation algorithm according to fsrd project: all nodes will be selected
    // as MPRs, since they have WILLIGNESS different of WILL_NEVER

    state_.clear_mprset();

    // Build a MPR set made of all members of N with
    // N_willingness different of WILL_NEVER
    for (auto it = nbset().begin(); it != nbset().end(); it++)
    {
        FSR_ETX_nb_tuple* nb_tuple = *it;
        if (nb_tuple->willingness() != FSR_ETX_WILL_NEVER &&
                nb_tuple->getStatus() == FSR_ETX_STATUS_SYM)
            state_.insert_mpr_addr(nb_tuple->nb_main_addr());
    }
}


///
/// \brief Creates the routing table of the node following RFC 3626 hints.
///
void
Fsr_Etx::rtable_default_computation()
{
    rtable_computation();
    // 1. All the entries from the routing table are removed.
}




///
/// \brief Creates the routing table of the node using dijkstra algorithm
///
void
Fsr_Etx::rtable_dijkstra_computation()
{
    nsaddr_t netmask (Ipv4Address::ALLONES_ADDRESS);
    // Declare a class that will run the dijkstra algorithm
    Dijkstra *dijkstra = new Dijkstra();

    // All the entries from the routing table are removed.
    if (par("DelOnlyRtEntriesInrtable_").boolValue())
    {
        for (rtable_t::const_iterator itRtTable = rtable_.getInternalTable()->begin();itRtTable != rtable_.getInternalTable()->begin();++itRtTable)
        {
            nsaddr_t addr = itRtTable->first;
            omnet_chg_rte(addr, addr,netmask,1, true, addr);
        }
    }
    else
        omnet_clean_rte(); // clean IP tables
    rtable_.clear();


    debug("Current node %s:\n", getNodeId(ra_addr()).c_str());
    // Iterate through all out 1 hop neighbors
    for (auto it = nbset().begin(); it != nbset().end(); it++)
    {
        FSR_ETX_nb_tuple* nb_tuple = *it;

        // Get the best link we have to the current neighbor..
        Fsr_Etx_link_tuple* best_link =
            state_.find_best_sym_link_tuple(nb_tuple->nb_main_addr(), CURRENT_TIME);
        // Add this edge to the graph we are building
        if (best_link)
        {
            debug("nb_tuple: %s (local) ==> %s , delay %lf, quality %lf\n", getNodeId(best_link->local_iface_addr()).c_str(),
                    getNodeId(nb_tuple->nb_main_addr()).c_str(), best_link->nb_link_delay(), best_link->etx());
            dijkstra->add_edge(nb_tuple->nb_main_addr(), best_link->local_iface_addr(),
                                best_link->nb_link_delay(), best_link->etx(), true);
        }
    }

    // N (set of 1-hop neighbors) is the set of nodes reachable through a symmetric
    // link with willingness different of WILL_NEVER. The vector at each position
    // is a list of the best links connecting the one hop neighbor to a 2 hop neighbor
    // Note: we are not our own two hop neighbor
    std::map<nsaddr_t, std::vector<FSR_ETX_nb2hop_tuple*> > N;
    std::set<nsaddr_t> N_index;
    for (auto it = nb2hopset().begin(); it != nb2hopset().end(); it++)
    {
        FSR_ETX_nb2hop_tuple* nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(*it);
        if (!nb2hop_tuple)
            throw cRuntimeError("\n Error conversion nd2hop tuple");

        nsaddr_t nb2hop_main_addr = nb2hop_tuple->nb2hop_addr();
        nsaddr_t nb_main_addr = nb2hop_tuple->nb_main_addr();

        if (nb2hop_main_addr == ra_addr())
            continue;
        // do we have a symmetric link to the one hop neighbor?
        FSR_ETX_nb_tuple* nb_tuple = state_.find_sym_nb_tuple(nb_main_addr);
        if (nb_tuple == nullptr)
            continue;
        // one hop neighbor has willingness different from FSR_ETX_WILL_NEVER?
        nb_tuple = state_.find_nb_tuple(nb_main_addr, FSR_ETX_WILL_NEVER);
        if (nb_tuple != nullptr)
            continue;
        // Retrieve the link that connect us to this 2 hop neighbor
        FSR_ETX_nb2hop_tuple* best_link = nullptr;
        Fsr_nb2hop_tuple * nb2hop_tuple_aux = state_.find_nb2hop_tuple(nb_main_addr, nb2hop_main_addr);
        if (nb2hop_tuple_aux)
        {
            best_link = dynamic_cast<FSR_ETX_nb2hop_tuple *>(nb2hop_tuple_aux);
            if (!best_link)
                throw cRuntimeError("\n Error conversion nb2tuple tuple");
        }

        bool found = false;
        for (auto it2 = N[nb_main_addr].begin();
                it2 != N[nb_main_addr].end(); it2++)
        {
            FSR_ETX_nb2hop_tuple* current_link = *it2;
            if (current_link->nb_main_addr() == nb_main_addr &&
                    current_link->nb2hop_addr() == nb2hop_main_addr)
            {
                found = true;
                break;
            }
        }
        if (!found)
            N[nb_main_addr].push_back(best_link);
        N_index.insert(nb_main_addr);
    }
    // we now have the best link to all of our 2 hop neighbors. Add this information
    // for each 2 hop neighbor to the edge vector...
    for (auto it = N_index.begin(); it != N_index.end(); it++)
    {
        nsaddr_t nb_main_addr = *it;

        for (auto it2 = N[nb_main_addr].begin();
                it2 != N[nb_main_addr].end(); it2++)
        {
            FSR_ETX_nb2hop_tuple* nb2hop_tuple = *it2;
            // Add this edge to the graph we are building. The last hop is our 1 hop
            // neighbor that has the best link to the current two hop neighbor. And
            // nb2hop_addr is not directly connected to this node
            debug("nb2hop_tuple: %s (local) ==> %s , delay %lf, quality %lf\n", getNodeId(nb_main_addr).c_str(),
                    getNodeId(nb2hop_tuple->nb2hop_addr()).c_str(), nb2hop_tuple->nb_link_delay(), nb2hop_tuple->etx());
            dijkstra->add_edge(nb2hop_tuple->nb2hop_addr(), nb_main_addr,
                                nb2hop_tuple->nb_link_delay(), nb2hop_tuple->etx(), false);
        }
    }

    // here we rely on the fact that in TC messages only the best links are published
    for (auto it = topologyset().begin();
            it != topologyset().end(); it++)
    {
        FSR_ETX_topology_tuple* topology_tuple = dynamic_cast<FSR_ETX_topology_tuple*>(*it);
        if (!topology_tuple)
            throw cRuntimeError("\n Error conversion topology tuple");



        if (topology_tuple->dest_addr() == ra_addr())
            continue;
        // Add this edge to the graph we are building. The last hop is our 1 hop
        // neighbor that has the best link to the current two hop. And dest_addr
        // is not directly connected to this node
        debug("topology_tuple: %s (local) ==> %sd , delay %lf, quality %lf\n", getNodeId(topology_tuple->last_addr()).c_str(),
                getNodeId(topology_tuple->dest_addr()).c_str(), topology_tuple->nb_link_delay(), topology_tuple->etx());
        dijkstra->add_edge(topology_tuple->dest_addr(), topology_tuple->last_addr(),
                            topology_tuple->nb_link_delay(), topology_tuple->etx(), false);
    }

    // Run the dijkstra algorithm
    dijkstra->run();

    // Now all we have to do is inserting routes according to hop count
#if 1
    std::multimap<int,nsaddr_t> processed_nodes;
    for (auto it = dijkstra->dijkstraMap.begin(); it != dijkstra->dijkstraMap.end(); it++)
    {
        // store the nodes in hop order, the multimap order the values in function of number of hops
        processed_nodes.insert(std::pair<int,nsaddr_t>(it->second.hop_count(), it->first));
    }
    while (!processed_nodes.empty())
    {

        auto it = processed_nodes.begin();
        auto itDij = dijkstra->dijkstraMap.find(it->second);
        if (itDij==dijkstra->dijkstraMap.end())
            throw cRuntimeError("node not found in DijkstraMap");
        int hopCount = it->first;
        if (hopCount == 1)
        {
            // add route...
            rtable_.add_entry(it->second, it->second, itDij->second.link().last_node(), 1, -1,itDij->second.link().quality(),itDij->second.link().getDelay());
            omnet_chg_rte(it->second, it->second, netmask, hopCount, false, itDij->second.link().last_node());
        }
        else if (it->first > 1)
        {
            // add route...
            FSR_ETX_rt_entry* entry = rtable_.lookup(itDij->second.link().last_node());
            if (entry==nullptr)
                throw cRuntimeError("entry not found");
            rtable_.add_entry(it->second, entry->next_addr(), entry->iface_addr(), hopCount, entry->local_iface_index(),itDij->second.link().quality(),itDij->second.link().getDelay());
            omnet_chg_rte (it->second, entry->next_addr(), netmask, hopCount, false, entry->iface_addr());
        }
        processed_nodes.erase(processed_nodes.begin());
        dijkstra->dijkstraMap.erase(itDij);
    }
    dijkstra->dijkstraMap.clear();
#else
    std::set<nsaddr_t> processed_nodes;
    for (auto it = dijkstra->all_nodes()->begin(); it != dijkstra->all_nodes()->end(); it++)
    {
        if (dijkstra->D(*it).hop_count() == 1)
        {
            // add route...
            rtable_.add_entry(*it, *it, dijkstra->D(*it).link().last_node(), 1, -1);
            omnet_chg_rte(*it, *it, netmask, 1, false, dijkstra->D(*it).link().last_node());
            processed_nodes.insert(*it);
        }
    }
    for (auto it = processed_nodes.begin(); it != processed_nodes.end(); it++)
        dijkstra->all_nodes()->erase(*it);
    processed_nodes.clear();
    for (auto it = dijkstra->all_nodes()->begin(); it != dijkstra->all_nodes()->end(); it++)
    {
        if (dijkstra->D(*it).hop_count() == 2)
        {
            // add route...
            FSR_ETX_rt_entry* entry = rtable_.lookup(dijkstra->D(*it).link().last_node());
            assert(entry != nullptr);
            rtable_.add_entry(*it, dijkstra->D(*it).link().last_node(), entry->iface_addr(), 2, entry->local_iface_index());
            omnet_chg_rte(*it, dijkstra->D(*it).link().last_node(), netmask, 2, false, entry->iface_addr());
            processed_nodes.insert(*it);
        }
    }
    for (auto it = processed_nodes.begin(); it != processed_nodes.end(); it++)
        dijkstra->all_nodes()->erase(*it);
    processed_nodes.clear();
    for (int i = 3; i <= dijkstra->highest_hop(); i++)
    {
        for (auto it = dijkstra->all_nodes()->begin(); it != dijkstra->all_nodes()->end(); it++)
        {
            if (dijkstra->D(*it).hop_count() == i)
            {
                // add route...
                FSR_ETX_rt_entry* entry = rtable_.lookup(dijkstra->D(*it).link().last_node());
                assert(entry != nullptr);
                rtable_.add_entry(*it, entry->next_addr(), entry->iface_addr(), i, entry->local_iface_index());
                omnet_chg_rte(*it, entry->next_addr(), netmask, i, false, entry->iface_addr());
                processed_nodes.insert(*it);
            }
        }
        for (auto it = processed_nodes.begin(); it != processed_nodes.end(); it++)
            dijkstra->all_nodes()->erase(*it);
        processed_nodes.clear();
    }
#endif
    // 5. For each entry in the multiple interface association base
    // where there exists a routing entry such that:
    //  R_dest_addr  == I_main_addr  (of the multiple interface association entry)
    // AND there is no routing entry such that:
    //  R_dest_addr  == I_iface_addr
    // then a route entry is created in the routing table
    for (auto it = ifaceassocset().begin(); it != ifaceassocset().end(); it++)
    {
        FSR_ETX_iface_assoc_tuple* tuple = *it;
        FSR_ETX_rt_entry* entry1 = rtable_.lookup(tuple->main_addr());
        FSR_ETX_rt_entry* entry2 = rtable_.lookup(tuple->iface_addr());
        if (entry1 != nullptr && entry2 == nullptr)
        {
            rtable_.add_entry(tuple->iface_addr(),
                              entry1->next_addr(), entry1->iface_addr(), entry1->dist(), entry1->local_iface_index(),entry1->quality,entry1->delay);
            omnet_chg_rte(tuple->iface_addr(), entry1->next_addr(), netmask, entry1->dist(), false, entry1->iface_addr());

        }
    }
    // rtable_.print_debug(this);
    // destroy the dijkstra class we've created
    // dijkstra->clear ();
    delete dijkstra;
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
Fsr_Etx::process_hello(FsrMsg& msg, const nsaddr_t &receiver_iface, const nsaddr_t &sender_iface, uint16_t pkt_seq_num, const int &index)
{
    assert(msg.msg_type() == FSR_ETX_HELLO_MSG);
    link_sensing(msg, receiver_iface, sender_iface, pkt_seq_num, index);
    populate_nbset(msg);
    populate_nb2hopset(msg);
    switch (parameter_.mpr_algorithm())
    {
    case FSR_ETX_MPR_R1:
        fsr_r1_mpr_computation();
        break;
    case FSR_ETX_MPR_R2:
        fsr_r2_mpr_computation();
        break;
    case FSR_ETX_MPR_QFSR:
        qfsr_mpr_computation();
        break;
    case FSR_ETX_MPR_FSRD:
        fsrd_mpr_computation();
        break;
    case FSR_ETX_DEFAULT_MPR:
    default:
        fsr_mpr_computation();
        break;
    }
    populate_mprselset(msg);
    return false;
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
Fsr_Etx::process_tc(FsrMsg& msg, const nsaddr_t &sender_iface, const int &index)
{
    assert(msg.msg_type() == FSR_ETX_TC_MSG);
    double now = CURRENT_TIME;
    Fsr_tc& tc = msg.tc();

    // 1. If the sender interface of this message is not in the symmetric
    // 1-hop neighborhood of this node, the message MUST be discarded.
    Fsr_Etx_link_tuple* link_tuple = nullptr;
    Fsr_link_tuple *tuple_aux = state_.find_sym_link_tuple(sender_iface, now);
    if (tuple_aux)
    {
        link_tuple = dynamic_cast<Fsr_Etx_link_tuple*> (tuple_aux);
        if (!link_tuple)
            throw cRuntimeError("\n Error conversion link tuple");
    }

    if (link_tuple == nullptr)
        return false;
    // 2. If there exist some tuple in the topology set where:
    //   T_last_addr == originator address AND
    //   T_seq       >  ANSN,
    // then further processing of this TC message MUST NOT be
    // performed. This might be a message received out of order.
    FSR_ETX_topology_tuple* topology_tuple = nullptr;
    Fsr_topology_tuple* topology_tuple_aux = state_.find_newer_topology_tuple(msg.orig_addr(), tc.ansn());
    if (topology_tuple_aux)
    {
        topology_tuple = dynamic_cast<FSR_ETX_topology_tuple*> (topology_tuple_aux);
        if (!topology_tuple)
            throw cRuntimeError("\n error conversion Topology tuple");
    }

    if (topology_tuple != nullptr)
        return false;

    // 3. All tuples in the topology set where:
    //  T_last_addr == originator address AND
    //  T_seq       <  ANSN
    // MUST be removed from the topology set.
    state_.erase_older_topology_tuples(msg.orig_addr(), tc.ansn());

    // 4. For each of the advertised neighbor main address received in
    // the TC message:
    for (int i = 0; i < tc.count; i++)
    {
        assert(i >= 0 && i < FSR_ETX_MAX_ADDRS);
        nsaddr_t addr = tc.nb_etx_main_addr(i).iface_address();
        // 4.1. If there exist some tuple in the topology set where:
        //   T_dest_addr == advertised neighbor main address, AND
        //   T_last_addr == originator address,
        // then the holding time of that tuple MUST be set to:
        //   T_time      =  current time + validity time.
        FSR_ETX_topology_tuple* topology_tuple = nullptr;
        Fsr_topology_tuple* topology_tuple_aux = state_.find_topology_tuple(addr, msg.orig_addr());
        if (topology_tuple_aux)
        {
            topology_tuple = dynamic_cast<FSR_ETX_topology_tuple*> (topology_tuple_aux);
            if (!topology_tuple)
                throw cRuntimeError("\n error conversion Topology tuple");
        }


        if (topology_tuple != nullptr)
            topology_tuple->time() = now + Fsr::emf_to_seconds(msg.vtime());
        // 4.2. Otherwise, a new tuple MUST be recorded in the topology
        // set where:
        //  T_dest_addr = advertised neighbor main address,
        //  T_last_addr = originator address,
        //  T_seq       = ANSN,
        //  T_time      = current time + validity time.
        else
        {
            topology_tuple = new FSR_ETX_topology_tuple;
            topology_tuple->dest_addr() = addr;
            topology_tuple->last_addr() = msg.orig_addr();
            topology_tuple->seq() = tc.ansn();
            topology_tuple->set_qos_behaviour(parameter_.link_quality());
            topology_tuple->time() = now + Fsr::emf_to_seconds(msg.vtime());
            add_topology_tuple(topology_tuple);
            // Schedules topology tuple deletion
            Fsr_TopologyTupleTimer* topology_timer =
                new Fsr_TopologyTupleTimer(this, topology_tuple);
            topology_timer->resched(DELAY(topology_tuple->time()));
        }
        // Update link quality and link delay information

        topology_tuple->update_link_quality(tc.nb_etx_main_addr(i).link_quality(),
                                            tc.nb_etx_main_addr(i).nb_link_quality());

        topology_tuple->update_link_delay(tc.nb_etx_main_addr(i).link_delay(),
                                          tc.nb_etx_main_addr(i).nb_link_delay());
    }
    return false;
}

#if 0
void
Fsr::forward_data(Packet* p)
{
    struct hdr_cmn* ch = HDR_CMN(p);
    struct hdr_ip* ih = HDR_IP(p);

    if (ch->direction() == hdr_cmn::UP &&
            ((uint32_t)ih->daddr() == IP_BROADCAST || ih->daddr() == ra_addr()))
    {
        dmux_->recv(p, 0);
        return;
    }
    else
    {
        ch->direction() = hdr_cmn::DOWN;
        ch->addr_type() = NS_AF_INET;
        if ((uint32_t)ih->daddr() == IP_BROADCAST)
            ch->next_hop() = IP_BROADCAST;
        else
        {
            Fsr_rt_entry* entry = rtable_.lookup(ih->daddr());
            if (entry == nullptr)
            {
                debug("%f: Node %d can not forward a packet destined to %d\n",
                      CURRENT_TIME,
                      Fsr::node_id(ra_addr()),
                      Fsr::node_id(ih->daddr()));
                drop(p, DROP_RTR_NO_ROUTE);
                return;
            }
            else
            {
                entry = rtable_.find_send_entry(entry);
                assert(entry != nullptr);
                ch->next_hop() = entry->next_addr();
                if (use_mac())
                {
                    ch->xmit_failure_ = fsr_mac_failed_callback;
                    ch->xmit_failure_data_ = (void*)this;
                }
            }
        }

        Scheduler::getInstance().schedule(target_, p, 0.0);
    }
}
#endif

void
Fsr_Etx::send_pkt()
{
    int num_msgs = msgs_.size();
    if (num_msgs == 0)
        return;

    L3Address destAdd;
    destAdd = L3Address(Ipv4Address::ALLONES_ADDRESS);
    // Calculates the number of needed packets
    int num_pkts = (num_msgs % FSR_ETX_MAX_MSGS == 0) ? num_msgs / FSR_ETX_MAX_MSGS :
                   (num_msgs / FSR_ETX_MAX_MSGS + 1);

    for (int i = 0; i < num_pkts; i++)
    {
        /// Link delay extension
        if (parameter_.link_delay())
        {
            // We are going to use link delay extension
            // to define routes to be selected
            const auto& op1 = makeShared<FsrPkt>();

            // Duplicated packet...
            Ptr<FsrPkt> op2 = nullptr;
            op1->setPkt_seq_num( pkt_seq());
            if (i == 0)
            {
                op1->setChunkLength(B(FSR_ETX_CAPPROBE_PACKET_SIZE));
                op1->setSn(cap_sn());

                // Allocate room for a duplicated packet...
                op2 = makeShared<FsrPkt>();

                op2->setChunkLength(B(FSR_ETX_CAPPROBE_PACKET_SIZE));
                // duplicated packet sequence no ...
                op2->setPkt_seq_num(op1->pkt_seq_num());
                // but different cap sequence no
                op2->setSn(cap_sn());
            }
            else
            {
                op1->setChunkLength(B(FSR_ETX_PKT_HDR_SIZE));
                op1->setSn(0);
            }

            int j = 0;
            for (auto it = msgs_.begin(); it != msgs_.end();)
            {
                if (j == FSR_ETX_MAX_MSGS)
                    break;
                op1->setMsgArraySize(j+1);
                op1->setMsg(j++, *it);

                if (i != 0)
                    op1->setChunkLength(op1->getChunkLength()+B((*it).size()));
                else /* if (i == 0) */
                {
                    op2->setMsgArraySize(j+1);
                    op2->setMsg(j++, *it);
                }
                it = msgs_.erase(it);
            }


            // Marking packet timestamp
            op1->setSend_time(CURRENT_TIME);

            if (i == 0)
                op2->setSend_time(op1->send_time());
            // Sending packet pair
            Packet *outPacket = new Packet("FSR Pkt");
            outPacket->insertAtBack(op1);
            sendToIp(outPacket, par("UdpPort"), destAdd, par("UdpPort"), IP_DEF_TTL, 0.0, L3Address());
            //sendToIp(op1, RT_PORT, destAdd, RT_PORT, IP_DEF_TTL, 0.0, L3Address());
            if (i == 0) {
                Packet *outPacket = new Packet("FSR Pkt");
                outPacket->insertAtBack(op2);
                sendToIp(outPacket, par("UdpPort"), destAdd, par("UdpPort"), IP_DEF_TTL, 0.0, L3Address());
            }
        }
        else
        {
            const auto& op = makeShared<FsrPkt>();

            op->setChunkLength(B(FSR_ETX_PKT_HDR_SIZE));
            op->setPkt_seq_num( pkt_seq());
            int j = 0;
            for (auto it = msgs_.begin(); it != msgs_.end();)
            {
                if (j == FSR_MAX_MSGS)
                    break;

                op->setMsgArraySize(j+1);
                op->setMsg(j++, *it);
                op->setChunkLength(op->getChunkLength()+B((*it).size()));

                it = msgs_.erase(it);
            }
            Packet *outPacket = new Packet("FSR Pkt");
            outPacket->insertAtBack(op);
            sendToIp(outPacket, par("UdpPort"), destAdd, par("UdpPort"), IP_DEF_TTL, 0.0, L3Address());
            //sendToIp(op, RT_PORT, destAdd, RT_PORT, IP_DEF_TTL, 0.0, L3Address());

        }
    }
}

///
/// \brief Creates a new %FSR HELLO message which is buffered to be sent later on.
///
void
Fsr_Etx::send_hello()
{
    FsrMsg msg;
    double now = CURRENT_TIME;
    msg.msg_type() = FSR_HELLO_MSG;
    msg.vtime() = Fsr::seconds_to_emf(FSR_NEIGHB_HOLD_TIME);
    msg.orig_addr() = ra_addr();
    msg.ttl() = 1;
    msg.hop_count() = 0;
    msg.msg_seq_num() = msg_seq();

    msg.hello().reserved() = 0;
    msg.hello().htime() = Fsr::seconds_to_emf(hello_ival());
    msg.hello().willingness() = willingness();
    msg.hello().count = 0;


    std::map<uint8_t, int> linkcodes_count;
    for (auto it = linkset().begin(); it != linkset().end(); it++)
    {
        Fsr_Etx_link_tuple* link_tuple = dynamic_cast<Fsr_Etx_link_tuple*>(*it);
        if (!link_tuple)
            throw cRuntimeError("\n Error conversion link tuple");


        if (link_tuple->local_iface_addr() == ra_addr() && link_tuple->time() >= now)
        {
            uint8_t link_type, nb_type, link_code;

            // Establishes link type
            if (use_mac() && link_tuple->lost_time() >= now)
                link_type = FSR_LOST_LINK;
            else if (link_tuple->sym_time() >= now)
                link_type = FSR_SYM_LINK;
            else if (link_tuple->asym_time() >= now)
                link_type = FSR_ASYM_LINK;
            else
                link_type = FSR_LOST_LINK;
            // Establishes neighbor type.
            if (state_.find_mpr_addr(get_main_addr(link_tuple->nb_iface_addr())))
                nb_type = FSR_MPR_NEIGH;
            else
            {
                bool ok = false;
                for (auto nb_it = nbset().begin();
                        nb_it != nbset().end();
                        nb_it++)
                {
                    Fsr_nb_tuple* nb_tuple = *nb_it;
                    if (nb_tuple->nb_main_addr() == get_main_addr(link_tuple->nb_iface_addr()))
                    {
                        if (nb_tuple->getStatus() == FSR_STATUS_SYM)
                            nb_type = FSR_SYM_NEIGH;
                        else if (nb_tuple->getStatus() == FSR_STATUS_NOT_SYM)
                            nb_type = FSR_NOT_NEIGH;
                        else
                        {
                            throw cRuntimeError("There is a neighbor tuple with an unknown status!");
                        }
                        ok = true;
                        break;
                    }
                }
                if (!ok)
                {
                    EV_INFO << "I don't know the neighbor " << get_main_addr(link_tuple->nb_iface_addr()) << "!!! \n";
                    continue;
                }
            }

            int count = msg.hello().count;

            link_code = (link_type & 0x03) | ((nb_type << 2) & 0x0f);
            auto pos = linkcodes_count.find(link_code);
            if (pos == linkcodes_count.end())
            {
                linkcodes_count[link_code] = count;
                assert(count >= 0 && count < FSR_MAX_HELLOS);
                msg.hello().hello_msg(count).count = 0;
                msg.hello().hello_msg(count).link_code() = link_code;
                msg.hello().hello_msg(count).reserved() = 0;
                msg.hello().hello_msg(count).set_qos_behaviour(parameter_.link_quality());
                msg.hello().count++;
            }
            else
                count = (*pos).second;

            int i = msg.hello().hello_msg(count).count;
            assert(count >= 0 && count < FSR_MAX_HELLOS);
            assert(i >= 0 && i < FSR_MAX_ADDRS);

            msg.hello().hello_msg(count).nb_iface_addr(i) =
                link_tuple->nb_iface_addr();

            msg.hello().hello_msg(count).nb_etx_iface_addr(i).link_quality() =
                link_tuple->link_quality();
            msg.hello().hello_msg(count).nb_etx_iface_addr(i).nb_link_quality() =
                link_tuple->nb_link_quality();
            /// Link delay extension
            msg.hello().hello_msg(count).nb_etx_iface_addr(i).link_delay() =
                link_tuple->link_delay();
            msg.hello().hello_msg(count).nb_etx_iface_addr(i).nb_link_delay() =
                link_tuple->nb_link_delay();

            msg.hello().hello_msg(count).count++;
            msg.hello().hello_msg(count).link_msg_size() =
                msg.hello().hello_msg(count).size();



        }
    }

    msg.msg_size() = msg.size();

    enque_msg(msg, JITTER);
}

void Fsr_Etx::send_tc()
{
    FsrMsg msg;
    msg.msg_type() = FSR_ETX_TC_MSG;
    msg.vtime() = Fsr::seconds_to_emf(FSR_ETX_TOP_HOLD_TIME);
    msg.orig_addr() = ra_addr();
    if (parameter_.fish_eye())
    {
        msg.ttl() = tc_msg_ttl_[tc_msg_ttl_index_];
        tc_msg_ttl_index_ = (tc_msg_ttl_index_ + 1) % (MAX_TC_MSG_TTL);
    }
    else
    {
        msg.ttl() = 255;
    }
    msg.hop_count() = 0;
    msg.msg_seq_num() = msg_seq();

    msg.tc().ansn() = ansn_;
    msg.tc().reserved() = 0;
    msg.tc().count = 0;
    msg.tc().set_qos_behaviour(parameter_.link_quality());

    switch (parameter_.mpr_algorithm())
    {
        case FSR_ETX_MPR_FSRD:
            // Report all 1 hop neighbors we have
            for (auto it = nbset().begin(); it != nbset().end(); it++)
            {
                Fsr_nb_tuple* nb_tuple = *it;
                int count = msg.tc().count;
                Fsr_Etx_link_tuple *link_tuple;

                if (nb_tuple->getStatus() == FSR_STATUS_SYM)
                {
                    assert(count >= 0 && count < FSR_MAX_ADDRS);
                    link_tuple = state_.find_best_sym_link_tuple(nb_tuple->nb_main_addr(), CURRENT_TIME);
                    if (link_tuple != nullptr)
                    {
                        msg.tc().nb_etx_main_addr(count).iface_address() = nb_tuple->nb_main_addr();

                        // Report link quality and link link delay of the best link
                        // that we have to this node.
                        msg.tc().nb_etx_main_addr(count).link_quality() = link_tuple->link_quality();
                        msg.tc().nb_etx_main_addr(count).nb_link_quality() = link_tuple->nb_link_quality();
                        msg.tc().nb_etx_main_addr(count).link_delay() = link_tuple->link_delay();
                        msg.tc().nb_etx_main_addr(count).nb_link_delay() = link_tuple->nb_link_delay();

                        msg.tc().count++;
                    }
                }
            }
            break;

        default:

            switch (parameter_.tc_redundancy())
            {
                case FSR_ETX_TC_REDUNDANCY_MPR_SEL_SET:
                    for (auto it = mprselset().begin(); it != mprselset().end(); it++)
                    {
                        FSR_ETX_mprsel_tuple* mprsel_tuple = *it;
                        int count = msg.tc().count;
                        Fsr_Etx_link_tuple *link_tuple;

                        assert(count >= 0 && count < FSR_MAX_ADDRS);
                        link_tuple = state_.find_best_sym_link_tuple(mprsel_tuple->main_addr(), CURRENT_TIME);
                        if (link_tuple != nullptr)
                        {
                            msg.tc().nb_etx_main_addr(count).iface_address() = mprsel_tuple->main_addr();

                            msg.tc().nb_etx_main_addr(count).link_quality() = link_tuple->link_quality();
                            msg.tc().nb_etx_main_addr(count).nb_link_quality() = link_tuple->nb_link_quality();
                            msg.tc().nb_etx_main_addr(count).link_delay() = link_tuple->link_delay();
                            msg.tc().nb_etx_main_addr(count).nb_link_delay() = link_tuple->nb_link_delay();

                            msg.tc().count++;
                        }
                    }

                    break;

                case FSR_ETX_TC_REDUNDANCY_MPR_SEL_SET_PLUS_MPR_SET:
                    for (auto it = mprselset().begin(); it != mprselset().end(); it++)
                    {
                        Fsr_mprsel_tuple* mprsel_tuple = *it;
                        int count = msg.tc().count;
                        Fsr_Etx_link_tuple *link_tuple;

                        assert(count >= 0 && count < FSR_MAX_ADDRS);
                        link_tuple = state_.find_best_sym_link_tuple(mprsel_tuple->main_addr(), CURRENT_TIME);
                        if (link_tuple != nullptr)
                        {
                            msg.tc().nb_etx_main_addr(count).iface_address() = mprsel_tuple->main_addr();

                            msg.tc().nb_etx_main_addr(count).link_quality() = link_tuple->link_quality();
                            msg.tc().nb_etx_main_addr(count).nb_link_quality() = link_tuple->nb_link_quality();
                            msg.tc().nb_etx_main_addr(count).link_delay() = link_tuple->link_delay();
                            msg.tc().nb_etx_main_addr(count).nb_link_delay() = link_tuple->nb_link_delay();

                            msg.tc().count++;
                        }
                    }

                    for (auto it = mprset().begin(); it != mprset().end(); it++)
                    {
                        nsaddr_t mpr_addr = *it;
                        int count = msg.tc().count;
                        Fsr_Etx_link_tuple *link_tuple;

                        assert(count >= 0 && count < FSR_MAX_ADDRS);
                        link_tuple = state_.find_best_sym_link_tuple(mpr_addr, CURRENT_TIME);
                        if (link_tuple != nullptr)
                        {
                            msg.tc().nb_etx_main_addr(count).iface_address() = mpr_addr;

                            msg.tc().nb_etx_main_addr(count).link_quality() = link_tuple->link_quality();
                            msg.tc().nb_etx_main_addr(count).nb_link_quality() = link_tuple->nb_link_quality();
                            msg.tc().nb_etx_main_addr(count).link_delay() = link_tuple->link_delay();
                            msg.tc().nb_etx_main_addr(count).nb_link_delay() = link_tuple->nb_link_delay();

                            msg.tc().count++;
                        }
                    }

                    break;

                case FSR_ETX_TC_REDUNDANCY_FULL:
                    for (auto it = nbset().begin(); it != nbset().end(); it++)
                    {
                        FSR_ETX_nb_tuple* nb_tuple = *it;
                        int count = msg.tc().count;
                        Fsr_Etx_link_tuple *link_tuple;

                        if (nb_tuple->getStatus() == FSR_STATUS_SYM)
                        {
                            assert(count >= 0 && count < FSR_MAX_ADDRS);
                            link_tuple = state_.find_best_sym_link_tuple(nb_tuple->nb_main_addr(), CURRENT_TIME);
                            if (link_tuple != nullptr)
                            {
                                msg.tc().nb_etx_main_addr(count).iface_address() = nb_tuple->nb_main_addr();

                                msg.tc().nb_etx_main_addr(count).link_quality() = link_tuple->link_quality();
                                msg.tc().nb_etx_main_addr(count).nb_link_quality() = link_tuple->nb_link_quality();
                                msg.tc().nb_etx_main_addr(count).link_delay() = link_tuple->link_delay();
                                msg.tc().nb_etx_main_addr(count).nb_link_delay() = link_tuple->nb_link_delay();

                                msg.tc().count++;
                            }
                        }
                    }

                    break;
                case FSR_ETX_TC_REDUNDANCY_MPR_SET:
                    for (auto it = mprset().begin(); it != mprset().end(); it++)
                    {
                        nsaddr_t mpr_addr = *it;
                        int count = msg.tc().count;
                        Fsr_Etx_link_tuple *link_tuple;

                        assert(count >= 0 && count < FSR_MAX_ADDRS);
                        link_tuple = state_.find_best_sym_link_tuple(mpr_addr, CURRENT_TIME);
                        if (link_tuple != nullptr)
                        {
                            msg.tc().nb_etx_main_addr(count).iface_address() = mpr_addr;

                            msg.tc().nb_etx_main_addr(count).link_quality() = link_tuple->link_quality();
                            msg.tc().nb_etx_main_addr(count).nb_link_quality() = link_tuple->nb_link_quality();
                            msg.tc().nb_etx_main_addr(count).link_delay() = link_tuple->link_delay();
                            msg.tc().nb_etx_main_addr(count).nb_link_delay() = link_tuple->nb_link_delay();

                            msg.tc().count++;
                        }
                    }
                    break;
            }
            break;
    }

    msg.msg_size() = msg.size();
    enque_msg(msg, JITTER);
}


bool
Fsr_Etx::link_sensing
(FsrMsg& msg, const nsaddr_t &receiver_iface, const nsaddr_t &sender_iface,
 uint16_t pkt_seq_num, const int & index)
{
    Fsr_hello& hello = msg.hello();
    double now = CURRENT_TIME;
    bool updated = false;
    bool created = false;

    Fsr_Etx_link_tuple* link_tuple = nullptr;
    Fsr_link_tuple* link_tuple_aux = state_.find_link_tuple(sender_iface);
    if (link_tuple_aux)
    {
        link_tuple = dynamic_cast<Fsr_Etx_link_tuple*>(link_tuple_aux);
        if (link_tuple == nullptr)
            throw cRuntimeError("\n Error conversion link tuple");
    }
    if (link_tuple == nullptr)
    {
        link_tuple = new Fsr_Etx_link_tuple;
        link_tuple->set_qos_behaviour(parameter_);
        link_tuple->set_owner(this);
        link_tuple->nb_iface_addr() = sender_iface;
        link_tuple->local_iface_addr() = receiver_iface;
        link_tuple->sym_time() = now - 1;
        link_tuple->lost_time() = 0.0;
        link_tuple->time() = now + Fsr::emf_to_seconds(msg.vtime());
        // Init link quality information struct for this link tuple
        link_tuple->link_quality_init(pkt_seq_num, DEFAULT_LOSS_WINDOW_SIZE);
        /// Link delay extension
        link_tuple->link_delay_init();
        // This call will be also in charge of creating a new tuple in
        // the neighbor set
        add_link_tuple(link_tuple, hello.willingness());
        created = true;
    }
    else
        updated = true;

    // Account link quality information for this link
    link_tuple->receive(pkt_seq_num, Fsr::emf_to_seconds(hello.htime()));


    link_tuple->asym_time() = now + Fsr::emf_to_seconds(msg.vtime());
    assert(hello.count >= 0 && hello.count <= FSR_ETX_MAX_HELLOS);
    for (int i = 0; i < hello.count; i++)
    {
        FSR_ETX_hello_msg& hello_msg = hello.hello_msg(i);
        int lt = hello_msg.link_code() & 0x03;
        int nt = hello_msg.link_code() >> 2;

        // We must not process invalid advertised links
        if ((lt == FSR_ETX_SYM_LINK && nt == FSR_ETX_NOT_NEIGH) ||
                (nt != FSR_ETX_SYM_NEIGH && nt != FSR_ETX_MPR_NEIGH
                 && nt != FSR_ETX_NOT_NEIGH))
            continue;

        assert(hello_msg.count >= 0 && hello_msg.count <= FSR_ETX_MAX_ADDRS);
        for (int j = 0; j < hello_msg.count; j++)
        {
            if (hello_msg.nb_etx_iface_addr(j).iface_address() == receiver_iface)
            {
                if (lt == FSR_ETX_LOST_LINK)
                {
                    link_tuple->sym_time() = now - 1;
                    updated = true;
                }
                else if (lt == FSR_ETX_SYM_LINK || lt == FSR_ETX_ASYM_LINK)
                {
                    link_tuple->sym_time() =
                        now + Fsr::emf_to_seconds(msg.vtime());
                    link_tuple->time() =
                        link_tuple->sym_time() + FSR_ETX_NEIGHB_HOLD_TIME;
                    link_tuple->lost_time() = 0.0;
                    updated = true;
                }
                link_tuple->update_link_quality(hello_msg.nb_etx_iface_addr(j).link_quality());
                link_tuple->update_link_delay(hello_msg.nb_etx_iface_addr(j).link_delay());

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
    return false;
}

bool
Fsr_Etx::populate_nb2hopset(FsrMsg& msg)
{
    Fsr_hello& hello = msg.hello();
    double now = CURRENT_TIME;

    // Upon receiving a HELLO message, the "validity time" MUST be computed
    // from the Vtime field of the message header (see section 3.3.2).
    double validity_time = now + Fsr::emf_to_seconds(msg.vtime());

    //  If the Originator Address is the main address of a
    //  L_neighbor_iface_addr from a link tuple included in the Link Set with
    //         L_SYM_time >= current time (not expired)
    //  then the 2-hop Neighbor Set SHOULD be updated as follows:
    for (auto it_lt = linkset().begin(); it_lt != linkset().end(); it_lt++)
    {
        Fsr_Etx_link_tuple* link_tuple = dynamic_cast<Fsr_Etx_link_tuple*>(*it_lt);
        if (!link_tuple)
            throw cRuntimeError("\n Error conversion link tuple");



        if (get_main_addr(link_tuple->nb_iface_addr()) == msg.orig_addr() &&
                link_tuple->sym_time() >= now)
        {
            assert(hello.count >= 0 && hello.count <= FSR_ETX_MAX_HELLOS);

            for (int i = 0; i < hello.count; i++)
            {
                FSR_ETX_hello_msg& hello_msg = hello.hello_msg(i);
                int nt = hello_msg.link_code() >> 2;
                assert(hello_msg.count >= 0 && hello_msg.count <= FSR_ETX_MAX_ADDRS);

                if (nt == FSR_ETX_SYM_NEIGH || nt == FSR_ETX_MPR_NEIGH)
                {

                    for (int j = 0; j < hello_msg.count; j++)
                    {
                        nsaddr_t nb2hop_addr = get_main_addr(hello_msg.nb_etx_iface_addr(j).iface_address());

                        if (nb2hop_addr == ra_addr())
                            continue;
                        FSR_ETX_nb2hop_tuple* nb2hop_tuple = nullptr;
                        Fsr_nb2hop_tuple* nb2hop_tuple_aux =
                            state_.find_nb2hop_tuple(msg.orig_addr(), nb2hop_addr);
                        if (nb2hop_tuple_aux)
                        {
                            nb2hop_tuple = dynamic_cast<FSR_ETX_nb2hop_tuple*>(nb2hop_tuple_aux);
                            if (!nb2hop_tuple)
                                throw cRuntimeError("\n Error conversion nd2hop tuple");
                        }

                        if (nb2hop_tuple == nullptr)
                        {
                            nb2hop_tuple = new FSR_ETX_nb2hop_tuple;
                            nb2hop_tuple->nb_main_addr() = msg.orig_addr();
                            nb2hop_tuple->nb2hop_addr() = nb2hop_addr;
                            nb2hop_tuple->set_qos_behaviour(parameter_.link_quality());

                            nb2hop_tuple->update_link_quality(0.0, 0.0);
                            nb2hop_tuple->update_link_delay(1.0, 1.0);

                            add_nb2hop_tuple(nb2hop_tuple);
                            nb2hop_tuple->time() = validity_time;
                            Fsr_Nb2hopTupleTimer* nb2hop_timer =
                                new Fsr_Nb2hopTupleTimer(this, nb2hop_tuple);
                            nb2hop_timer->resched(DELAY(nb2hop_tuple->time()));
                        }
                        else
                            nb2hop_tuple->time() = validity_time;

                        switch (parameter_.link_quality())
                        {
                        case FSR_ETX_BEHAVIOR_ETX:
                            if (hello_msg.nb_etx_iface_addr(j).etx() < nb2hop_tuple->etx())
                            {
                                nb2hop_tuple->update_link_quality(
                                    hello_msg.nb_etx_iface_addr(j).link_quality(),
                                    hello_msg.nb_etx_iface_addr(j).nb_link_quality());

                            }
                            break;

                        case FSR_ETX_BEHAVIOR_ML:
                            if (hello_msg.nb_etx_iface_addr(j).etx() > nb2hop_tuple->etx())
                            {
                                nb2hop_tuple->update_link_quality(
                                    hello_msg.nb_etx_iface_addr(j).link_quality(),
                                    hello_msg.nb_etx_iface_addr(j).nb_link_quality());
                            }
                            break;

                        case FSR_ETX_BEHAVIOR_NONE:
                        default:
                            //
                            break;
                        }
                        if (hello_msg.nb_etx_iface_addr(j).nb_link_delay() < nb2hop_tuple->nb_link_delay())
                        {
                            nb2hop_tuple->update_link_delay(
                                hello_msg.nb_etx_iface_addr(j).link_delay(),
                                hello_msg.nb_etx_iface_addr(j).nb_link_delay());
                        }
                    }
                }
                else if (nt == FSR_ETX_NOT_NEIGH)
                {

                    for (int j = 0; j < hello_msg.count; j++)
                    {
                        nsaddr_t nb2hop_addr = get_main_addr(hello_msg.nb_etx_iface_addr(j).iface_address());

                        state_.erase_nb2hop_tuples(msg.orig_addr(), nb2hop_addr);
                    }
                }
            }
            break;
        }
    }
    return false;
}

void
Fsr_Etx::nb_loss(Fsr_link_tuple* tuple)
{
    debug("%f: Node %s detects neighbor %s loss\n", CURRENT_TIME,
            getNodeId(ra_addr()).c_str(), getNodeId(tuple->nb_iface_addr()).c_str());

    updated_link_tuple(tuple, FSR_WILL_DEFAULT);
    state_.erase_nb2hop_tuples(get_main_addr(tuple->nb_iface_addr()));
    state_.erase_mprsel_tuples(get_main_addr(tuple->nb_iface_addr()));

    switch (parameter_.mpr_algorithm())
    {
    case FSR_ETX_MPR_R1:
        fsr_r1_mpr_computation();
        break;

    case FSR_ETX_MPR_R2:
        fsr_r2_mpr_computation();
        break;

    case FSR_ETX_MPR_QFSR:
        qfsr_mpr_computation();
        break;

    case FSR_ETX_MPR_FSRD:
        fsrd_mpr_computation();
        break;

    case FSR_ETX_DEFAULT_MPR:
    default:
        fsr_mpr_computation();
        break;
    }
    switch (parameter_.routing_algorithm())
    {
    case FSR_ETX_DIJKSTRA_ALGORITHM:
        rtable_dijkstra_computation();
        break;

    default:
    case FSR_ETX_DEFAULT_ALGORITHM:
        rtable_default_computation();
        break;
    }
}

void Fsr_Etx::finish()
{
    rtable_.clear();
    msgs_.clear();
    delete state_etx_ptr;
    state_ptr = state_etx_ptr = nullptr;
    helloTimer = nullptr;   ///< Timer for sending HELLO messages.
    tcTimer = nullptr;  ///< Timer for sending TC messages.
    midTimer = nullptr;    ///< Timer for sending MID messages.
    linkQualityTimer = nullptr;
}

Fsr_Etx::Fsr_Etx() :
    Fsr(),
    tc_msg_ttl_(),
    tc_msg_ttl_index_(-1),
    cap_sn_(-1),
    state_etx_ptr(nullptr),
    linkQualityTimer(nullptr)
{
}

Fsr_Etx::~Fsr_Etx()
{

    rtable_.clear();
    msgs_.clear();
    if (state_etx_ptr)
    {
        delete state_etx_ptr;
        state_ptr = state_etx_ptr = nullptr;
    }
    if (getTimerMultimMap())
    {
        while (getTimerMultimMap()->size()>0)
        {
            Fsr_Timer * timer = check_and_cast<Fsr_Timer *>(getTimerMultimMap()->begin()->second);
            getTimerMultimMap()->erase(getTimerMultimMap()->begin());
            timer->setTuple(nullptr);
            if (helloTimer==timer)
                helloTimer = nullptr;
            else if (tcTimer==timer)
                tcTimer = nullptr;
            else if (midTimer==timer)
                midTimer = nullptr;
            else if (linkQualityTimer==timer)
                linkQualityTimer = nullptr;
            delete timer;
        }
    }

    if (helloTimer)
    {
        delete helloTimer;
        helloTimer = nullptr;
    }
    if (tcTimer)
    {
        delete tcTimer;
        tcTimer = nullptr;
    }
    if (midTimer)
    {
        delete midTimer;
        midTimer = nullptr;
    }
    if (linkQualityTimer)
    {
        delete linkQualityTimer;
        linkQualityTimer = nullptr;
    }
}

void
Fsr_Etx::link_quality()
{
    double now = CURRENT_TIME;
    for (auto it = state_.linkset_.begin(); it != state_.linkset_.end(); it++)
    {
        Fsr_Etx_link_tuple* tuple = dynamic_cast<Fsr_Etx_link_tuple*>(*it);
        if (tuple->next_hello() < now)
            tuple->packet_timeout();
    }
}

bool Fsr_Etx::getNextHop(const L3Address &dest, L3Address &add, int &iface, double &cost)
{
    FSR_ETX_rt_entry* rt_entry = rtable_.lookup(dest);
    if (!rt_entry)
    {
        L3Address apAddr;
        if (getAp(dest, apAddr))
        {

            FSR_ETX_rt_entry* rt_entry = rtable_.lookup(apAddr);
            if (!rt_entry)
                return false;
            if (rt_entry->route.size())
                add = rt_entry->route[0];
            else
                add = rt_entry->next_addr();
            Fsr_rt_entry* rt_entry_aux = rtable_.find_send_entry(rt_entry);
            if (rt_entry_aux->next_addr() != add)
                throw cRuntimeError("FSR Data base error");

            NetworkInterface * ie = getInterfaceWlanByAddress(rt_entry->iface_addr());
            iface = ie->getInterfaceId();
            if (parameter_.link_delay())
               cost = rt_entry->delay;
            else
                cost = rt_entry->quality;
            return true;
        }
        return false;
    }

    if (rt_entry->route.size())
        add = rt_entry->route[0];
    else
        add = rt_entry->next_addr();
    Fsr_rt_entry* rt_entry_aux = rtable_.find_send_entry(rt_entry);
    if (rt_entry_aux->next_addr() != add)
        throw cRuntimeError("FSR Data base error");

    NetworkInterface * ie = getInterfaceWlanByAddress(rt_entry->iface_addr());
    iface = ie->getInterfaceId();
    if (parameter_.link_delay())
       cost = rt_entry->delay;
    else
        cost = rt_entry->quality;
    return true;
}

} // namespace inetmanet

} // namespace inet


