

#ifndef __FSR_omnet_h__
#define __FSR_omnet_h__
#include <map>
#include <vector>
#include <assert.h>
#include "inet/common/INETUtils.h"
#include "inet/routing/extras/base/ManetRoutingBase.h"
#include "inet/routing/extras/fsr/Fsr_repositories.h"
#include "inet/routing/extras/fsr/Fsr_rtable.h"
#include "inet/routing/extras/fsr/Fsr_state.h"
#include "inet/routing/extras/fsr/FsrPkt_m.h"

namespace inet {

namespace inetmanet {


#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif


#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif


#ifndef nullptr
#define nullptr 0
#endif

#define IP_BROADCAST     ((uint32_t) 0xffffffff)


#ifndef CURRENT_TIME
#define CURRENT_TIME    SIMTIME_DBL(simTime())
#endif

#ifndef CURRENT_TIME_T
#define CURRENT_TIME_T  SIMTIME_DBL(simTime())
#endif

#define debug  EV << utils::stringf



#define DELAY(time) (((time) < (CURRENT_TIME)) ? (0.000001) : \
    (time - CURRENT_TIME + 0.000001))

#define DELAY_T(time) (((time) < (CURRENT_TIME_T)) ? (0.000001) : \
    (time - CURRENT_TIME_T + 0.000001))



#define FSR_C      0.0625


	/********** Holding times **********/

	/// Neighbor holding time.
#define FSR_NEIGHB_HOLD_TIME   3 * FSR_REFRESH_INTERVAL
	/// Top holding time.
#define FSR_TOP_HOLD_TIME  3 * tc_ival()
	/// Dup holding time.
#define FSR_DUP_HOLD_TIME  30
	/// MID holding time.
#define FSR_MID_HOLD_TIME  3 * mid_ival()

/********** Link types **********/

/// Unspecified link type.
#define FSR_UNSPEC_LINK    0
/// Asymmetric link type.
#define FSR_ASYM_LINK      1
/// Symmetric link type.
#define FSR_SYM_LINK       2
/// Lost link type.
#define FSR_LOST_LINK      3

/********** Neighbor types **********/

/// Not neighbor type.
#define FSR_NOT_NEIGH      0
/// Symmetric neighbor type.
#define FSR_SYM_NEIGH      1
/// Asymmetric neighbor type.
#define FSR_MPR_NEIGH      2


/********** Willingness **********/

/// Willingness for forwarding packets from other nodes: never.
#define FSR_WILL_NEVER     0
/// Willingness for forwarding packets from other nodes: low.
#define FSR_WILL_LOW       1
/// Willingness for forwarding packets from other nodes: medium.
#define FSR_WILL_DEFAULT   3
/// Willingness for forwarding packets from other nodes: high.
#define FSR_WILL_HIGH      6
/// Willingness for forwarding packets from other nodes: always.
#define FSR_WILL_ALWAYS    7


/********** Miscellaneous constants **********/

/// Maximum allowed jitter.
#define FSR_MAXJITTER      hello_ival()/4
/// Maximum allowed sequence number.
#define FSR_MAX_SEQ_NUM    65535
/// Used to set status of an FSR_nb_tuple as "not symmetric".
#define FSR_STATUS_NOT_SYM 0
/// Used to set status of an FSR_nb_tuple as "symmetric".
#define FSR_STATUS_SYM     1
/// Random number between [0-FSR_MAXJITTER] used to jitter FSR packet transmission.
//#define JITTER            (Random::uniform()*FSR_MAXJITTER)

class Fsr;         // forward declaration

/********** Timers **********/

/// Basic timer class

class Fsr_Timer :  public ManetTimer /*cMessage*/
{
  protected:
    cObject* tuple_ = nullptr;
  public:
    Fsr_Timer(ManetRoutingBase* agent) : ManetTimer(agent) {}
    Fsr_Timer() : ManetTimer() {}
    virtual void setTuple(cObject *tuple) {tuple_ = tuple;}
};


/// Timer for sending an enqued message.
class Fsr_MsgTimer : public Fsr_Timer
{
  public:
    Fsr_MsgTimer(ManetRoutingBase* agent) : Fsr_Timer(agent) {}
    Fsr_MsgTimer():Fsr_Timer() {}
    void expire() override;
};

/// Timer for sending HELLO messages.
class Fsr_HelloTimer : public Fsr_Timer
{
  public:
    Fsr_HelloTimer(ManetRoutingBase* agent) : Fsr_Timer(agent) {}
    Fsr_HelloTimer():Fsr_Timer() {}
    void expire() override;
};


/// Timer for sending TC messages.
class Fsr_TcTimer : public Fsr_Timer
{
  public:
    Fsr_TcTimer(ManetRoutingBase* agent) : Fsr_Timer(agent) {}
    Fsr_TcTimer():Fsr_Timer() {}
    void expire() override;
};


/// Timer for sending MID messages.
class Fsr_MidTimer : public Fsr_Timer
{
  public:
    Fsr_MidTimer(ManetRoutingBase* agent) : Fsr_Timer(agent) {}
    Fsr_MidTimer():Fsr_Timer() {}
    virtual void expire() override;
};



/// Timer for removing duplicate tuples: FSR_dup_tuple.
class Fsr_DupTupleTimer : public Fsr_Timer
{
//  protected:
//  FSR_dup_tuple* tuple_;
  public:
    Fsr_DupTupleTimer(ManetRoutingBase* agent, Fsr_dup_tuple* tuple) : Fsr_Timer(agent)
    {
        tuple_ = tuple;
    }
    void setTuple(Fsr_dup_tuple* tuple) {tuple_ = tuple; tuple->asocTimer = this;}
    ~Fsr_DupTupleTimer();
    virtual void expire() override;
};

/// Timer for removing link tuples: FSR_link_tuple.
class Fsr_LinkTupleTimer : public Fsr_Timer
{
  public:
    Fsr_LinkTupleTimer(ManetRoutingBase* agent, Fsr_link_tuple* tuple);

    void setTuple(Fsr_link_tuple* tuple) {tuple_ = tuple; tuple->asocTimer = this;}
    ~Fsr_LinkTupleTimer();
    virtual void expire() override;
  protected:
    //FSR_link_tuple*  tuple_; ///< FSR_link_tuple which must be removed.
    bool            first_time_;

};


/// Timer for removing nb2hop tuples: FSR_nb2hop_tuple.

class Fsr_Nb2hopTupleTimer : public Fsr_Timer
{
  public:
    Fsr_Nb2hopTupleTimer(ManetRoutingBase* agent, Fsr_nb2hop_tuple* tuple) : Fsr_Timer(agent)
    {
        tuple_ = tuple;
    }

    void setTuple(Fsr_nb2hop_tuple* tuple) {tuple_ = tuple; tuple->asocTimer = this;}
    ~Fsr_Nb2hopTupleTimer();
    virtual void expire() override;
//  protected:
//  FSR_nb2hop_tuple*  tuple_; ///< FSR_link_tuple which must be removed.

};




/// Timer for removing MPR selector tuples: FSR_mprsel_tuple.
class Fsr_MprSelTupleTimer : public Fsr_Timer
{
  public:
    Fsr_MprSelTupleTimer(ManetRoutingBase* agent, Fsr_mprsel_tuple* tuple) : Fsr_Timer(agent)
    {
        tuple_ = tuple;
    }

    void setTuple(Fsr_mprsel_tuple* tuple) {tuple_ = tuple; tuple->asocTimer = this;}
    ~Fsr_MprSelTupleTimer();
    virtual void expire() override;

//  protected:
//  FSR_mprsel_tuple*  tuple_; ///< FSR_link_tuple which must be removed.

};


/// Timer for removing topology tuples: FSR_topology_tuple.

class Fsr_TopologyTupleTimer : public Fsr_Timer
{
  public:
    Fsr_TopologyTupleTimer(ManetRoutingBase* agent, Fsr_topology_tuple* tuple) : Fsr_Timer(agent)
    {
        tuple_ = tuple;
    }

    void setTuple(Fsr_topology_tuple* tuple) {tuple_ = tuple; tuple->asocTimer = this;}
    ~Fsr_TopologyTupleTimer();
    virtual void expire() override;
//  protected:
//  FSR_topology_tuple*    tuple_; ///< FSR_link_tuple which must be removed.

};

/// Timer for removing interface association tuples: FSR_iface_assoc_tuple.
class Fsr_IfaceAssocTupleTimer : public Fsr_Timer
{
  public:
    Fsr_IfaceAssocTupleTimer(ManetRoutingBase* agent, Fsr_iface_assoc_tuple* tuple) : Fsr_Timer(agent)
    {
        tuple_ = tuple;
    }

    void setTuple(Fsr_iface_assoc_tuple* tuple) {tuple_ = tuple; tuple->asocTimer = this;}
    ~Fsr_IfaceAssocTupleTimer();
    virtual void expire() override;
//  protected:
//  FSR_iface_assoc_tuple* tuple_; ///< FSR_link_tuple which must be removed.

};

/********** FSR Agent **********/


///
/// \brief Routing agent which implements %FSR protocol following RFC 3626.
///
/// Interacts with TCL interface through command() method. It implements all
/// functionalities related to sending and receiving packets and managing
/// internal state.
///

typedef std::set<Fsr_Timer *> TimerPendingList;

class Fsr : public ManetRoutingBase
{
  protected:
    /********** Intervals **********/
    ///
    /// \brief Period at which a node must cite every link and every neighbor.
    ///
    /// We only use this value in order to define FSR_NEIGHB_HOLD_TIME.
    ///
    double FSR_REFRESH_INTERVAL;//   2

    double jitter() {return uniform(0,(double)FSR_MAXJITTER);}
#define JITTER jitter()

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

    bool configured = false;
    virtual void start() override;

    //std::priority_queue<TimerQueueElem> *timerQueuePtr;
    bool topologyChange = false;
    virtual void setTopologyChanged(bool p) {topologyChange = p;}
    virtual bool getTopologyChanged() {return topologyChange;}

// must be protected and used for dereved class FSR_ETX
    /// A list of pending messages which are buffered awaiting for being sent.
    std::vector<FsrMsg>   msgs_;
    /// Routing table.
    Fsr_rtable     rtable_;

    typedef std::map<nsaddr_t,Fsr_rtable*> GlobalRtable;
    GlobalRtable &globalRtable = SIMULATION_SHARED_VARIABLE(globalRtable);
    typedef std::map<nsaddr_t,std::vector<nsaddr_t> > DistributionPath;
    DistributionPath &distributionPath = SIMULATION_SHARED_VARIABLE(distributionPath);;
    bool computed = false;
    /// Internal state with all needed data structs.

    Fsr_state      *state_ptr = nullptr;

    /// Packets sequence number counter.
    uint16_t    pkt_seq_ = FSR_MAX_SEQ_NUM;
    /// Messages sequence number counter.
    uint16_t    msg_seq_ = FSR_MAX_SEQ_NUM;
    /// Advertised Neighbor Set sequence number.
    uint16_t    ansn_ = FSR_MAX_SEQ_NUM;

    /// HELLO messages' emission interval.
    cPar     *hello_ival_ = nullptr;
    /// TC messages' emission interval.
    cPar     *tc_ival_ = nullptr;
    /// MID messages' emission interval.
    cPar     *mid_ival_ = nullptr;
    /// Willingness for forwarding packets on behalf of other nodes.
    cPar     *willingness_ = nullptr;
    /// Determines if layer 2 notifications are enabled or not.
    int  use_mac_ = false;
    bool useIndex = false;

    bool optimizedMid = false;

  protected:
// Omnet INET vaiables and functions
    char nodeName[50];

    virtual void socketDataArrived(UdpSocket *socket, Packet *packet) override;
    virtual void socketErrorArrived(UdpSocket *socket, Indication *indication) override;

    bool check_packet(Packet*, nsaddr_t &, int &);

    // PortClassifier*  dmux_;      ///< For passing packets up to agents.
    // Trace*       logtarget_; ///< For logging.

    Fsr_HelloTimer *helloTimer = nullptr;    ///< Timer for sending HELLO messages.
    Fsr_TcTimer    *tcTimer = nullptr;   ///< Timer for sending TC messages.
    Fsr_MidTimer   *midTimer = nullptr;  ///< Timer for sending MID messages.

#define hello_timer_  (*helloTimer)
#define  tc_timer_  (*tcTimer)
#define mid_timer_  (*midTimer)

    /// Increments packet sequence number and returns the new value.
    inline uint16_t pkt_seq()
    {
        pkt_seq_ = (pkt_seq_ + 1)%(FSR_MAX_SEQ_NUM + 1);
        return pkt_seq_;
    }
    /// Increments message sequence number and returns the new value.
    inline uint16_t msg_seq()
    {
        msg_seq_ = (msg_seq_ + 1)%(FSR_MAX_SEQ_NUM + 1);
        return msg_seq_;
    }

    inline nsaddr_t    ra_addr()   { return getAddress();}

    inline double     hello_ival()    { return hello_ival_->doubleValue();}
    inline double     tc_ival()   { return tc_ival_->doubleValue();}
    inline double     mid_ival()  { return mid_ival_->doubleValue();}
    inline int     willingness()   { return willingness_->intValue();}
    inline int     use_mac()   { return use_mac_;}

    inline linkset_t&   linkset()   { return state_ptr->linkset(); }
    inline mprset_t&    mprset()    { return state_ptr->mprset(); }
    inline mprselset_t& mprselset() { return state_ptr->mprselset(); }
    inline nbset_t&     nbset()     { return state_ptr->nbset(); }
    inline nb2hopset_t& nb2hopset() { return state_ptr->nb2hopset(); }
    inline topologyset_t&   topologyset()   { return state_ptr->topologyset(); }
    inline dupset_t&    dupset()    { return state_ptr->dupset(); }
    inline ifaceassocset_t& ifaceassocset() { return state_ptr->ifaceassocset(); }

    virtual void        recv_fsr(Packet*);

    virtual void        CoverTwoHopNeighbors(const nsaddr_t &neighborMainAddr, nb2hopset_t & N2);
    virtual void        mpr_computation();
    virtual void        rtable_computation();

    virtual bool        process_hello(FsrMsg&, const nsaddr_t &, const nsaddr_t &, const int &);
    virtual bool        process_tc(FsrMsg&, const nsaddr_t &, const int &);
    virtual void        process_mid(FsrMsg&, const nsaddr_t &, const int &);

    virtual void        forward_default(FsrMsg&, Fsr_dup_tuple*, const nsaddr_t &, const nsaddr_t &);
    virtual void        forward_data(cMessage* p) {}

    virtual void        enque_msg(FsrMsg&, double);
    virtual void        send_hello();
    virtual void        send_tc();
    virtual void        send_mid();
    virtual void        send_pkt();

    virtual bool        link_sensing(FsrMsg&, const nsaddr_t &, const nsaddr_t &, const int &);
    virtual bool        populate_nbset(FsrMsg&);
    virtual bool        populate_nb2hopset(FsrMsg&);
    virtual void        populate_mprselset(FsrMsg&);

    virtual void        set_hello_timer();
    virtual void        set_tc_timer();
    virtual void        set_mid_timer();

    virtual void        nb_loss(Fsr_link_tuple*);
    virtual void        add_dup_tuple(Fsr_dup_tuple*);
    virtual void        rm_dup_tuple(Fsr_dup_tuple*);
    virtual void        add_link_tuple(Fsr_link_tuple*, uint8_t);
    virtual void        rm_link_tuple(Fsr_link_tuple*);
    virtual void        updated_link_tuple(Fsr_link_tuple*, uint8_t willingness);
    virtual void        add_nb_tuple(Fsr_nb_tuple*);
    virtual void        rm_nb_tuple(Fsr_nb_tuple*);
    virtual void        add_nb2hop_tuple(Fsr_nb2hop_tuple*);
    virtual void        rm_nb2hop_tuple(Fsr_nb2hop_tuple*);
    virtual void        add_mprsel_tuple(Fsr_mprsel_tuple*);
    virtual void        rm_mprsel_tuple(Fsr_mprsel_tuple*);
    virtual void        add_topology_tuple(Fsr_topology_tuple*);
    virtual void        rm_topology_tuple(Fsr_topology_tuple*);
    virtual void        add_ifaceassoc_tuple(Fsr_iface_assoc_tuple*);
    virtual void        rm_ifaceassoc_tuple(Fsr_iface_assoc_tuple*);
    virtual Fsr_nb_tuple*    find_or_add_nb(Fsr_link_tuple*, uint8_t willingness);

    const nsaddr_t  & get_main_addr(const nsaddr_t&) const;
    virtual int     degree(Fsr_nb_tuple*);

    static bool seq_num_bigger_than(uint16_t, uint16_t);
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void mac_failed(Packet*);
    virtual void    recv(cMessage *p) {}

    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;
    //virtual void processPromiscuous(const cObject *details){};
    virtual void processLinkBreak(const Packet *) override;
    virtual void processLinkBreakManagement(const Packet *details) override {return;}
    virtual void processLinkBreakCsma(const Packet *details) override {return;}
    virtual void processLinkBreakManagementCsma(const Packet *details) override {return;}

    L3Address getIfaceAddressFromIndex(int index);

    std::string getNodeId(const nsaddr_t &addr);

    void computeDistributionPath(const nsaddr_t &initNode);

  public:
    Fsr() {}
    virtual ~Fsr();


    static double       emf_to_seconds(uint8_t);
    static uint8_t      seconds_to_emf(double);
    static int      node_id(const nsaddr_t&);

    // Routing information access
    virtual bool supportGetRoute() override {return true;}
    virtual uint32_t getRoute(const L3Address &, std::vector<L3Address> &) override;
    virtual bool getNextHop(const L3Address &, L3Address &add, int &iface, double &) override;
    virtual bool isProactive() override;
    virtual void setRefreshRoute(const L3Address &destination, const L3Address & nextHop,bool isReverse) override {}
    virtual bool isOurType(const Packet *) override;
    virtual bool getDestAddress(Packet *, L3Address &) override;
    virtual int getRouteGroup(const AddressGroup &gr, std::vector<L3Address>&) override;
    virtual bool getNextHopGroup(const AddressGroup &gr, L3Address &add, int &iface, L3Address&) override;
    virtual int  getRouteGroup(const L3Address&, std::vector<L3Address> &, L3Address&, bool &, int group = 0) override;
    virtual bool getNextHopGroup(const L3Address&, L3Address &add, int &iface, L3Address&, bool &, int group = 0) override;

    //
    virtual void getDistributionPath(const L3Address&, std::vector<L3Address> &path);

    virtual bool isNodeCandidate(const L3Address&);

    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;


    virtual Result ensureRouteForDatagram(Packet *datagram) override {throw cRuntimeError("ensureRouteForDatagram called with FSR protocol");}

};

} // namespace inetmanet

} // namespace inet

#endif
