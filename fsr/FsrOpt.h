

#ifndef __FSROPT_omnet_h__

#define __FSROPT_omnet_h__
#include "inet/routing/extras/fsr/Fsr.h"

namespace inet {

namespace inetmanet {

class FsrOpt : public Fsr
{
  private:
    friend class Fsr_HelloTimer;
    friend class Fsr_TcTimer;
    friend class Fsr_MidTimer;
    friend class Fsr_DupTupleTimer;
    friend class Fsr_LinkTupleTimer;
    friend class Fsr_Nb2hopTupleTimer;
    friend class Fsr_MprSelTupleTimer;
    friend class Fsr_TopologyTupleTimer;
    friend class Fsr_IfaceAssocTupleTimer;
    friend class Fsr_MsgTimer;
    friend class Fsr_Timer;

  protected:

    virtual bool        link_sensing(FsrMsg&, const nsaddr_t &, const nsaddr_t &, const int &) override;
    virtual bool        populate_nbset(FsrMsg&) override;
    virtual bool        populate_nb2hopset(FsrMsg&) override;

    virtual void        recv_fsr(Packet *) override;

    virtual bool        process_hello(FsrMsg&, const nsaddr_t &, const nsaddr_t &, const int &) override;
    virtual bool        process_tc(FsrMsg&, const nsaddr_t &, const int &) override;
    virtual int         update_topology_tuples(FsrMsg& msg, int index);
    virtual void nb_loss(Fsr_link_tuple* tuple) override;

};

} // namespace inetmanet

} // namespace inet

#endif

