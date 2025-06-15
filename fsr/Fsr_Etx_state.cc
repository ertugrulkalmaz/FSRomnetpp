#include "inet/common/INETDefs.h"
#include "inet/routing/extras/fsr/Fsr_Etx_state.h"
#include "inet/routing/extras/fsr/Fsr_Etx.h"

namespace inet {

namespace inetmanet {

Fsr_Etx_state::Fsr_Etx_state(Fsr_Etx_parameter *p)
{
    parameter = p;
}

Fsr_Etx_link_tuple*  Fsr_Etx_state::find_best_sym_link_tuple(const nsaddr_t &main_addr, double now)
{
    Fsr_Etx_link_tuple* best = nullptr;

    for (auto it = ifaceassocset_.begin();
            it != ifaceassocset_.end(); it++)
    {
        FSR_ETX_iface_assoc_tuple* iface_assoc_tuple = *it;
        if (iface_assoc_tuple->main_addr() == main_addr)
        {
            Fsr_link_tuple *tupleAux = find_sym_link_tuple(iface_assoc_tuple->iface_addr(), now);
            if (tupleAux == nullptr)
                continue;
            Fsr_Etx_link_tuple* tuple =
                dynamic_cast<Fsr_Etx_link_tuple*> (tupleAux);
            if (best == nullptr)
                best = tuple;
            else
            {
                if (parameter->link_delay())
                {
                    if (tuple->nb_link_delay() < best->nb_link_delay())
                        best = tuple;
                }
                else
                {
                    switch (parameter->link_quality())
                    {
                    case FSR_ETX_BEHAVIOR_ETX:
                        if (tuple->etx() < best->etx())
                            best = tuple;
                        break;

                    case FSR_ETX_BEHAVIOR_ML:
                        if (tuple->etx() > best->etx())
                            best = tuple;
                        break;
                    case FSR_ETX_BEHAVIOR_NONE:
                    default:
                        // best = tuple;
                        break;
                    }
                }
            }
        }
    }
    if (best == nullptr)
    {
        Fsr_link_tuple *tuple = find_sym_link_tuple(main_addr, now);
        if (tuple!=nullptr)
            best = check_and_cast<Fsr_Etx_link_tuple*>(tuple);
    }
    return best;
}

} // namespace inetmanet

} // namespace inet


