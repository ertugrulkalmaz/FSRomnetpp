
#ifndef __FSR_printer_h__
#define __FSR_printer_h__

#include "inet/routing/extras/fsr/Fsr.h"
#include "inet/routing/extras/fsr/Fsr_repositories.h"
#include "inet/routing/extras/fsr/FsrPkt_m.h"

#if 0
#include <packet.h>
#include <ip.h>
#include <trace.h>
#endif

namespace inet {

namespace inetmanet {

/// Encapsulates all printing functions for FSR data structures and messages.
class FSR_printer
{
    friend class Fsr;

  protected:
    static void print_linkset(Trace*, linkset_t&);
    static void print_nbset(Trace*, nbset_t&);
    static void print_nb2hopset(Trace*, nb2hopset_t&);
    static void print_mprset(Trace*, mprset_t&);
    static void print_mprselset(Trace*, mprselset_t&);
    static void print_topologyset(Trace*, topologyset_t&);

    static void print_fsr_pkt(FILE*, FsrPkt*);
    static void print_fsr_msg(FILE*, FsrMsg&);
    static void print_fsr_hello(FILE*, Fsr_hello&);
    static void print_fsr_tc(FILE*, Fsr_tc&);
    static void print_fsr_mid(FILE*, Fsr_mid&);
#if 0
  public:
    static void print_cmn_hdr(FILE*, struct hdr_cmn*);
    static void print_ip_hdr(FILE*, struct hdr_ip*);
#endif
};

} // namespace inetmanet

} // namespace inet

#endif

