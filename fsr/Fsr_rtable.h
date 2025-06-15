
#ifndef __FSR_rtable_h__
#define __FSR_rtable_h__

#include <map>

#include "inet/common/INETDefs.h"

#include "inet/routing/extras/fsr/Fsr_repositories.h"

namespace inet {

namespace inetmanet {

typedef std::map<nsaddr_t, Fsr_rt_entry*> rtable_t;


class Fsr_rtable : public cObject
{

  public:
    rtable_t    rt_;    ///< Data structure for the routing table.

    Fsr_rtable(const Fsr_rtable& other);
    Fsr_rtable();
    ~Fsr_rtable();
    const rtable_t * getInternalTable() const { return &rt_; }


    void        clear();
    void        rm_entry(const nsaddr_t &dest);
    Fsr_rt_entry*  add_entry(const nsaddr_t &dest, const nsaddr_t &next, const nsaddr_t &iface, uint32_t dist, const int &, double quality = -1, double delay = -1);
    Fsr_rt_entry*  add_entry(const nsaddr_t &dest, const nsaddr_t &next, const nsaddr_t &iface, uint32_t dist, const int &, PathVector path, double quality = -1, double delay = -1);
    Fsr_rt_entry*  add_entry(const nsaddr_t &dest, const nsaddr_t &next, const nsaddr_t &iface, uint32_t dist, const int &, Fsr_rt_entry *entry, double quality = -1, double delay = -1);
    Fsr_rt_entry*  lookup(const nsaddr_t &dest);
    Fsr_rt_entry*  find_send_entry(Fsr_rt_entry*);
    uint32_t    size();

    virtual std::string str() const;

    virtual Fsr_rtable *dup() const { return new Fsr_rtable(*this); }

//  void        print(Trace*);
};

} // namespace inetmanet

} // namespace inet

#endif

