#ifndef __FSR_ETX_h__

#define __FSR_ETX_h__

#include "inet/routing/extras/fsr/Fsr.h"
#include "inet/routing/extras/fsr/Fsr_Etx_parameter.h"
#include "inet/routing/extras/fsr/Fsr_Etx_repositories.h"
#include "inet/routing/extras/fsr/Fsr_Etx_state.h"

namespace inet {

namespace inetmanet {

#define FSR_ETX_C FSR_C


/********** Intervals **********/

/// HELLO messages emission interval.
#define FSR_ETX_HELLO_INTERVAL FSR_HELLO_INTERVAL

/// TC messages emission interval.
#define FSR_ETX_TC_INTERVAL FSR_TC_INTERVAL

/// MID messages emission interval.
#define FSR_ETX_MID_INTERVA FSR_MID_INTERVA

#define FSR_ETX_REFRESH_INTERVAL FSR_REFRESH_INTERVAL




#define FSR_ETX_NEIGHB_HOLD_TIME   FSR_NEIGHB_HOLD_TIME

#define FSR_ETX_TOP_HOLD_TIME  FSR_TOP_HOLD_TIME

#define FSR_ETX_DUP_HOLD_TIME  FSR_DUP_HOLD_TIME

#define FSR_ETX_MID_HOLD_TIME  FSR_MID_HOLD_TIME




#define FSR_ETX_UNSPEC_LINK    FSR_UNSPEC_LINK

#define FSR_ETX_ASYM_LINK      FSR_ASYM_LINK

#define FSR_ETX_SYM_LINK       FSR_SYM_LINK

#define FSR_ETX_LOST_LINK      FSR_LOST_LINK



#define FSR_ETX_NOT_NEIGH      FSR_NOT_NEIGH

#define FSR_ETX_SYM_NEIGH      FSR_SYM_NEIGH

#define FSR_ETX_MPR_NEIGH      FSR_MPR_NEIGH


/// Willingness for forwarding packets from other nodes: never.
#define FSR_ETX_WILL_NEVER     FSR_WILL_NEVER
/// Willingness for forwarding packets from other nodes: low.
#define FSR_ETX_WILL_LOW       FSR_WILL_LOW
/// Willingness for forwarding packets from other nodes: medium.
#define FSR_ETX_WILL_DEFAULT   FSR_WILL_DEFAULT
/// Willingness for forwarding packets from other nodes: high.
#define FSR_ETX_WILL_HIGH      FSR_WILL_HIGH
/// Willingness for forwarding packets from other nodes: always.
#define FSR_ETX_WILL_ALWAYS    FSR_WILL_ALWAYS


/// Maximum allowed jitter.
#define FSR_ETX_MAXJITTER      FSR_MAXJITTER
/// Maximum allowed sequence number.
//#define FSR_ETX_MAX_SEQ_NUM  FSR_ETX_MAX_SEQ_NUM
/// Used to set status of an FSR_nb_tuple as "not symmetric".
#define FSR_ETX_STATUS_NOT_SYM FSR_ETX_STATUS_NOT_SYM
/// Used to set status of an FSR_nb_tuple as "symmetric".
#define FSR_ETX_STATUS_SYM     FSR_STATUS_SYM

class Fsr_Etx;         // forward declaration

class Fsr_Etx_LinkQualityTimer : public Fsr_Timer
{
  public:
    Fsr_Etx_LinkQualityTimer(ManetRoutingBase* agent) : Fsr_Timer(agent) {}
    Fsr_Etx_LinkQualityTimer():Fsr_Timer() {}
    virtual void expire() override;
};



typedef FsrMsg FSR_ETX_msg; // FSR_msg defined in FSRpkt.msg
class Fsr_Etx : public Fsr
{
        friend class Fsr_Etx_LinkQualityTimer;
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
        friend class Fsr_Etx_state;
        friend class Dijkstra;

        Fsr_Etx_parameter parameter_;
#define MAX_TC_MSG_TTL  13
        int tc_msg_ttl_[MAX_TC_MSG_TTL];
        int tc_msg_ttl_index_;

        long cap_sn_;

    protected:
        virtual void start() override;

        Fsr_Etx_state *state_etx_ptr;

        Fsr_Etx_LinkQualityTimer *linkQualityTimer;
        inline long& cap_sn()
        {
            cap_sn_++;
            return cap_sn_;
        }

#define link_quality_timer_  (*linkQualityTimer)

        virtual void recv_fsr(Packet*) override;
        virtual void fsr_mpr_computation();
        virtual void fsr_r1_mpr_computation();
        virtual void fsr_r2_mpr_computation();
        virtual void qfsr_mpr_computation();
        virtual void fsrd_mpr_computation();

        virtual void rtable_default_computation();
        virtual void rtable_dijkstra_computation();

        virtual bool process_hello(FsrMsg&, const nsaddr_t &, const nsaddr_t &, uint16_t, const int &);
        virtual bool process_tc(FsrMsg&, const nsaddr_t &, const int &) override;
        virtual void forward_data(cMessage* p) override {}
        virtual void send_hello() override;
        virtual void send_tc() override;
        virtual void send_pkt() override;

        virtual bool link_sensing(FsrMsg&, const nsaddr_t &, const nsaddr_t &, uint16_t, const int &);
        virtual bool populate_nb2hopset(FsrMsg&) override;

        virtual void nb_loss(Fsr_link_tuple*) override;

        static bool seq_num_bigger_than(uint16_t, uint16_t);
        virtual int numInitStages() const override { return NUM_INIT_STAGES; }
        virtual void initialize(int stage) override;
        virtual void recv(cMessage *p) override {};
        virtual void finish() override;
        virtual void link_quality();

    public:
        bool getNextHop(const L3Address &dest, L3Address &add, int &iface, double &cost) override;
        Fsr_Etx();
        ~Fsr_Etx();
        static double emf_to_seconds(uint8_t);
        static uint8_t seconds_to_emf(double);
        static int node_id(const nsaddr_t&);

};

} // namespace inetmanet

} // namespace inet

#endif

