#include "inet/routing/extras/fsr/Fsr_rtable.h"

#include "inet/routing/extras/fsr/Fsr.h"
#include "inet/routing/extras/fsr/Fsr_repositories.h"

namespace inet {

namespace inetmanet {


Fsr_rtable::Fsr_rtable()
{
}

Fsr_rtable::Fsr_rtable(const Fsr_rtable& rtable)
{
    for (rtable_t::const_iterator itRtTable = rtable.getInternalTable()->begin();itRtTable != rtable.getInternalTable()->begin();++itRtTable)
    {
        rt_[itRtTable->first] = itRtTable->second->dup();
    }
}

Fsr_rtable::~Fsr_rtable()
{
    // Iterates over the routing table deleting each FSR_rt_entry*.
    for (auto it = rt_.begin(); it != rt_.end(); it++)
        delete (*it).second;
}

void
Fsr_rtable::clear()
{
    // Iterates over the routing table deleting each FSR_rt_entry*.
    for (auto it = rt_.begin(); it != rt_.end(); it++)
        delete (*it).second;

    // Cleans routing table.
    rt_.clear();
}

void
Fsr_rtable::rm_entry(const nsaddr_t &dest)
{
    // Remove the pair whose key is dest
    rt_.erase(dest);
}

Fsr_rt_entry*
Fsr_rtable::lookup(const nsaddr_t &dest)
{
    // Get the iterator at "dest" position
    auto it = rt_.find(dest);
    // If there is no route to "dest", return nullptr
    if (it == rt_.end())
        return nullptr;

    // Returns the rt entry (second element of the pair)
    return (*it).second;
}

Fsr_rt_entry*
Fsr_rtable::find_send_entry(Fsr_rt_entry* entry)
{
    Fsr_rt_entry* e = entry;
    while (e != nullptr && e->dest_addr() != e->next_addr())
        e = lookup(e->next_addr());
    return e;
}

Fsr_rt_entry*
Fsr_rtable::add_entry(const nsaddr_t &dest, const nsaddr_t & next, const nsaddr_t & iface, uint32_t dist, const int &index, double quality, double delay)
{
    // Creates a new rt entry with specified values
    Fsr_rt_entry* entry = new Fsr_rt_entry();
    entry->dest_addr() = dest;
    entry->next_addr() = next;
    entry->iface_addr() = iface;
    entry->dist() = dist;
    entry->quality = quality;
    entry->delay = delay;
    entry->local_iface_index() = index;
    if (entry->dist()==2)
    {
        entry->route.push_back(next);
    }

    // Inserts the new entry
    auto it = rt_.find(dest);
    if (it != rt_.end())
        delete (*it).second;
    rt_[dest] = entry;

    // Returns the new rt entry
    return entry;
}

Fsr_rt_entry*
Fsr_rtable::add_entry(const nsaddr_t & dest, const nsaddr_t & next, const nsaddr_t & iface, uint32_t dist, const int &index, Fsr_rt_entry *entryAux, double quality, double delay)
{
    // Creates a new rt entry with specified values
    Fsr_rt_entry* entry = new Fsr_rt_entry();
    entry->dest_addr() = dest;
    entry->next_addr() = next;
    entry->iface_addr() = iface;
    entry->dist() = dist;
    entry->quality = quality;
    entry->delay = delay;
    entry->local_iface_index() = index;
    entry->route = entryAux->route;
    if (entryAux->dist()==(entry->dist()-1))
        entry->route.push_back(entryAux->dest_addr());

    // Inserts the new entry
    auto it = rt_.find(dest);
    if (it != rt_.end())
        delete (*it).second;
    rt_[dest] = entry;

    // Returns the new rt entry
    return entry;
}

uint32_t
Fsr_rtable::size()
{
    return rt_.size();
}


std::string Fsr_rtable::str() const
{
    std::stringstream out;

    for (auto it = rt_.begin(); it != rt_.end(); it++)
    {
        Fsr_rt_entry* entry = dynamic_cast<Fsr_rt_entry *> ((*it).second);
        out << "dest:"<< Fsr::node_id(entry->dest_addr()) << " ";
        out << "gw:" << Fsr::node_id(entry->next_addr()) << " ";
        out << "iface:" << Fsr::node_id(entry->iface_addr()) << " ";
        out << "dist:" << entry->dist() << " ";
        out <<"\n";
    }
    return out.str();
}

} // namespace inetmanet

} // namespace inet

