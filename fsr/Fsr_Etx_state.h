#ifndef __FSR_ETX_state_h__
#define __FSR_ETX_state_h__

#include "inet/routing/extras/fsr/Fsr_Etx_repositories.h"
#include "inet/routing/extras/fsr/Fsr_state.h"

namespace inet {

namespace inetmanet {

class Fsr_Etx_state : public Fsr_state
{
    friend class Fsr_Etx;
    Fsr_Etx_parameter *parameter;
  protected:
    Fsr_Etx_link_tuple*  find_best_sym_link_tuple(const nsaddr_t &main_addr, double now);
    Fsr_Etx_state(Fsr_Etx_parameter *);
};

} // namespace inetmanet

} // namespace inet

#endif
