
#include "inet/routing/extras/fsr/Fsr_state.h"

#include "inet/routing/extras/fsr/Fsr.h"

namespace inet {

namespace inetmanet {

/********** MPR Selector Set Manipulation **********/

Fsr_mprsel_tuple*
Fsr_state::find_mprsel_tuple(const nsaddr_t &main_addr)
{
    for (auto it = mprselset_.begin(); it != mprselset_.end(); it++)
    {
        Fsr_mprsel_tuple* tuple = *it;
        if (tuple->main_addr() == main_addr)
            return tuple;
    }
    return nullptr;
}

void
Fsr_state::erase_mprsel_tuple(Fsr_mprsel_tuple* tuple)
{
    for (auto it = mprselset_.begin(); it != mprselset_.end(); it++)
    {
        if (*it == tuple)
        {
            mprselset_.erase(it);
            break;
        }
    }
}

bool
Fsr_state::erase_mprsel_tuples(const nsaddr_t & main_addr)
{
    bool topologyChanged = false;
    for (auto it = mprselset_.begin(); it != mprselset_.end();)
    {
        Fsr_mprsel_tuple* tuple = *it;
        if (tuple->main_addr() == main_addr)
        {
            it = mprselset_.erase(it);
            topologyChanged = true;
            if (mprselset_.empty())
                break;
        }
        else
            it++;

    }
    return topologyChanged;
}

void
Fsr_state::insert_mprsel_tuple(Fsr_mprsel_tuple* tuple)
{
    mprselset_.push_back(tuple);
}

/********** Neighbor Set Manipulation **********/

Fsr_nb_tuple*
Fsr_state::find_nb_tuple(const nsaddr_t & main_addr)
{
    for (auto it = nbset_.begin(); it != nbset_.end(); it++)
    {
        Fsr_nb_tuple* tuple = *it;
        if (tuple->nb_main_addr() == main_addr)
            return tuple;
    }
    return nullptr;
}

Fsr_nb_tuple*
Fsr_state::find_sym_nb_tuple(const nsaddr_t & main_addr)
{
    for (auto it = nbset_.begin(); it != nbset_.end(); it++)
    {
        Fsr_nb_tuple* tuple = *it;
        if (tuple->nb_main_addr() == main_addr && tuple->getStatus() == FSR_STATUS_SYM)
            return tuple;
    }
    return nullptr;
}

Fsr_nb_tuple*
Fsr_state::find_nb_tuple(const nsaddr_t & main_addr, uint8_t willingness)
{
    for (auto it = nbset_.begin(); it != nbset_.end(); it++)
    {
        Fsr_nb_tuple* tuple = *it;
        if (tuple->nb_main_addr() == main_addr && tuple->willingness() == willingness)
            return tuple;
    }
    return nullptr;
}

void
Fsr_state::erase_nb_tuple(Fsr_nb_tuple* tuple)
{
    for (auto it = nbset_.begin(); it != nbset_.end(); it++)
    {
        if (*it == tuple)
        {
            nbset_.erase(it);
            break;
        }
    }
}

void
Fsr_state::erase_nb_tuple(const nsaddr_t & main_addr)
{
    for (auto it = nbset_.begin(); it != nbset_.end(); it++)
    {
        Fsr_nb_tuple* tuple = *it;
        if (tuple->nb_main_addr() == main_addr)
        {
            it = nbset_.erase(it);
            break;
        }
    }
}

void
Fsr_state::insert_nb_tuple(Fsr_nb_tuple* tuple)
{
    nbset_.push_back(tuple);
}

/********** Neighbor 2 Hop Set Manipulation **********/

Fsr_nb2hop_tuple*
Fsr_state::find_nb2hop_tuple(const nsaddr_t & nb_main_addr, const nsaddr_t & nb2hop_addr)
{
    for (auto it = nb2hopset_.begin(); it != nb2hopset_.end(); it++)
    {
        Fsr_nb2hop_tuple* tuple = *it;
        if (tuple->nb_main_addr() == nb_main_addr && tuple->nb2hop_addr() == nb2hop_addr)
            return tuple;
    }
    return nullptr;
}

void
Fsr_state::erase_nb2hop_tuple(Fsr_nb2hop_tuple* tuple)
{
    for (auto it = nb2hopset_.begin(); it != nb2hopset_.end(); it++)
    {
        if (*it == tuple)
        {
            nb2hopset_.erase(it);
            break;
        }
    }
}

bool
Fsr_state::erase_nb2hop_tuples(const nsaddr_t & nb_main_addr, const nsaddr_t & nb2hop_addr)
{
    bool returnValue = false;
    for (auto it = nb2hopset_.begin(); it != nb2hopset_.end();)
    {
        Fsr_nb2hop_tuple* tuple = *it;
        if (tuple->nb_main_addr() == nb_main_addr && tuple->nb2hop_addr() == nb2hop_addr)
        {
            it = nb2hopset_.erase(it);
            returnValue = true;
            if (nb2hopset_.empty())
                break;
        }
        else
            it++;

    }
    return returnValue;
}

bool
Fsr_state::erase_nb2hop_tuples(const nsaddr_t & nb_main_addr)
{
    bool topologyChanged = false;
    for (auto it = nb2hopset_.begin(); it != nb2hopset_.end();)
    {
        Fsr_nb2hop_tuple* tuple = *it;
        if (tuple->nb_main_addr() == nb_main_addr)
        {
            it = nb2hopset_.erase(it);
            topologyChanged = true;
            if (nb2hopset_.empty())
                break;
        }
        else
            it++;

    }
    return topologyChanged;
}

void
Fsr_state::insert_nb2hop_tuple(Fsr_nb2hop_tuple* tuple)
{
    nb2hopset_.push_back(tuple);
}

/********** MPR Set Manipulation **********/

bool
Fsr_state::find_mpr_addr(const nsaddr_t & addr)
{
    auto it = mprset_.find(addr);
    return (it != mprset_.end());
}

void
Fsr_state::insert_mpr_addr(const nsaddr_t & addr)
{
    mprset_.insert(addr);
}

void
Fsr_state::clear_mprset()
{
    mprset_.clear();
}

/********** Duplicate Set Manipulation **********/

Fsr_dup_tuple*
Fsr_state::find_dup_tuple(const nsaddr_t & addr, uint16_t seq_num)
{
    for (auto it = dupset_.begin(); it != dupset_.end(); it++)
    {
        Fsr_dup_tuple* tuple = *it;
        if (tuple->getAddr() == addr && tuple->seq_num() == seq_num)
            return tuple;
    }
    return nullptr;
}

void
Fsr_state::erase_dup_tuple(Fsr_dup_tuple* tuple)
{
    for (auto it = dupset_.begin(); it != dupset_.end(); it++)
    {
        if (*it == tuple)
        {
            dupset_.erase(it);
            break;
        }
    }
}

void
Fsr_state::insert_dup_tuple(Fsr_dup_tuple* tuple)
{
    dupset_.push_back(tuple);
}

/********** Link Set Manipulation **********/

Fsr_link_tuple*
Fsr_state::find_link_tuple(const nsaddr_t & iface_addr)
{
    for (auto it = linkset_.begin(); it != linkset_.end(); it++)
    {
        Fsr_link_tuple* tuple = *it;
        if (tuple->nb_iface_addr() == iface_addr)
            return tuple;
    }
    return nullptr;
}

Fsr_link_tuple*
Fsr_state::find_sym_link_tuple(const nsaddr_t & iface_addr, double now)
{
    for (auto it = linkset_.begin(); it != linkset_.end(); it++)
    {
        Fsr_link_tuple* tuple = *it;
        if (tuple->nb_iface_addr() == iface_addr)
        {
            if (tuple->sym_time() > now)
                return tuple;
            else
                break;
        }
    }
    return nullptr;
}

void
Fsr_state::erase_link_tuple(Fsr_link_tuple* tuple)
{
    for (auto it = linkset_.begin(); it != linkset_.end(); it++)
    {
        if (*it == tuple)
        {
            linkset_.erase(it);
            break;
        }
    }
}

void
Fsr_state::insert_link_tuple(Fsr_link_tuple* tuple)
{
    linkset_.push_back(tuple);
}

/********** Topology Set Manipulation **********/

Fsr_topology_tuple*
Fsr_state::find_topology_tuple(const nsaddr_t & dest_addr, const nsaddr_t & last_addr)
{
    for (auto it = topologyset_.begin(); it != topologyset_.end(); it++)
    {
        Fsr_topology_tuple* tuple = *it;
        if (tuple->dest_addr() == dest_addr && tuple->last_addr() == last_addr)
            return tuple;
    }
    return nullptr;
}

Fsr_topology_tuple*
Fsr_state::find_newer_topology_tuple(const nsaddr_t &last_addr, uint16_t ansn)
{
    for (auto it = topologyset_.begin(); it != topologyset_.end(); it++)
    {
        Fsr_topology_tuple* tuple = *it;
        if (tuple->last_addr() == last_addr && tuple->seq() > ansn)
            return tuple;
    }
    return nullptr;
}

void
Fsr_state::erase_topology_tuple(Fsr_topology_tuple* tuple)
{
    for (auto it = topologyset_.begin(); it != topologyset_.end(); it++)
    {
        if (*it == tuple)
        {
            topologyset_.erase(it);
            break;
        }
    }
}
std::ostream& operator<<(std::ostream& out, const Fsr_topology_tuple& tuple)
{
    out << "Tuple index: " << tuple.index;
    out << " dest: "<< tuple.dest_addr_;
    out << " last: " << tuple.last_addr_;
    out << " seq: " << tuple.seq_;
    out << " time: " << tuple.time_;
    out << std::endl;
    return out;
}
void
Fsr_state::print_topology_tuples_to(const nsaddr_t & dest_addr)
{
    for (auto it = topologyset_.begin(); it != topologyset_.end(); it++)
    {
        Fsr_topology_tuple* tuple = *it;
        if (tuple->dest_addr() == dest_addr)
            std::cout << " -- " << *tuple;
    }
}


void
Fsr_state::print_topology_tuples_across(const nsaddr_t & last_addr)
{
    for (auto it = topologyset_.begin(); it != topologyset_.end(); it++)
    {
        Fsr_topology_tuple* tuple = *it;
        if (tuple->last_addr() == last_addr)
            std::cout << " -- " << *tuple;
    }
}
void
Fsr_state::erase_older_topology_tuples(const nsaddr_t & last_addr, uint16_t ansn)
{
    for (auto it = topologyset_.begin(); it != topologyset_.end();)
    {
        Fsr_topology_tuple* tuple = *it;
        if (tuple->last_addr() == last_addr && tuple->seq() < ansn)
        {
            it = topologyset_.erase(it);
            if (topologyset_.empty())
                break;
        }
        else
            it++;

    }
}

void
Fsr_state::insert_topology_tuple(Fsr_topology_tuple* tuple)
{
    topologyset_.push_back(tuple);
}

/********** Interface Association Set Manipulation **********/

Fsr_iface_assoc_tuple*
Fsr_state::find_ifaceassoc_tuple(const nsaddr_t & iface_addr)
{
    for (auto it = ifaceassocset_.begin();
            it != ifaceassocset_.end();
            it++)
    {
        Fsr_iface_assoc_tuple* tuple = *it;
        if (tuple->iface_addr() == iface_addr)
            return tuple;
    }
    return nullptr;
}

void
Fsr_state::erase_ifaceassoc_tuple(Fsr_iface_assoc_tuple* tuple)
{
    for (auto it = ifaceassocset_.begin();
            it != ifaceassocset_.end();
            it++)
    {
        if (*it == tuple)
        {
            ifaceassocset_.erase(it);
            break;
        }
    }
}

void
Fsr_state::insert_ifaceassoc_tuple(Fsr_iface_assoc_tuple* tuple)
{
    ifaceassocset_.push_back(tuple);
}

void Fsr_state::clear_all()
{

    for (auto it = linkset_.begin(); it != linkset_.end(); it++)
        delete (*it);
    linkset_.clear();

    for (auto it = nbset_.begin(); it != nbset_.end(); it++)
        delete (*it);
    nbset_.clear();
    for (auto it = nb2hopset_.begin(); it != nb2hopset_.end(); it++)
        delete (*it);
    nb2hopset_.clear();
    for (auto it = topologyset_.begin(); it != topologyset_.end(); it++)
        delete (*it);
    topologyset_.clear();

    for (auto it = mprselset_.begin(); it != mprselset_.end(); it++)
        delete (*it);
    mprselset_.clear();
    for (auto it = dupset_.begin(); it != dupset_.end(); it++)
        delete (*it);

    dupset_.clear();
    for (auto it = ifaceassocset_.begin(); it != ifaceassocset_.end(); it++)
        delete (*it);
    ifaceassocset_.clear();
    mprset_.clear();

}

Fsr_state::Fsr_state(const Fsr_state& st)
{
    for (auto it = st.linkset_.begin(); it != st.linkset_.end(); it++)
    {
        Fsr_link_tuple* tuple = *it;
        linkset_.push_back(tuple->dup());
    }

    for (auto it = st.nbset_.begin(); it != st.nbset_.end(); it++)
    {
        Fsr_nb_tuple* tuple = *it;
        nbset_.push_back(tuple->dup());
    }

    for (auto it = st.nb2hopset_.begin(); it != st.nb2hopset_.end(); it++)
    {
        Fsr_nb2hop_tuple* tuple = *it;
        nb2hopset_.push_back(tuple->dup());
    }

    for (auto it = st.topologyset_.begin(); it != st.topologyset_.end(); it++)
    {
        Fsr_topology_tuple* tuple = *it;
        topologyset_.push_back(tuple->dup());
    }

    for (auto it = st.mprset_.begin(); it != st.mprset_.end(); it++)
    {
        mprset_.insert(*it);
    }

    for (auto it = st.mprselset_.begin(); it != st.mprselset_.end(); it++)
    {
        Fsr_mprsel_tuple* tuple = *it;
        mprselset_.push_back(tuple->dup());
    }

    for (auto it = st.dupset_.begin(); it != st.dupset_.end(); it++)
    {
        Fsr_dup_tuple* tuple = *it;
        dupset_.push_back(tuple->dup());
    }

    for (auto it = st.ifaceassocset_.begin(); it != st.ifaceassocset_.end(); it++)
    {
        Fsr_iface_assoc_tuple* tuple = *it;
        ifaceassocset_.push_back(tuple->dup());
    }
}


Fsr_state::~Fsr_state()
{
    clear_all();
}

} // namespace inetmanet

} // namespace inet

