#ifndef __FSRMsg_h__
#define __FSRMsg_h__

#include <string.h>
#include "inet/common/INETDefs.h"
#include "inet/networklayer/common/L3Address.h"
#include "inet/routing/extras/fsr/Fsr_Etx_parameter.h"

namespace inet {

namespace inetmanet {


#ifndef nsaddr_t
typedef L3Address nsaddr_t;
#endif

/********** Message types **********/
/// %FSR HELLO message type.
#define FSR_HELLO_MSG      1
/// %FSR TC message type.
#define FSR_TC_MSG     2
/// %FSR MID message type.
#define FSR_MID_MSG        3


/// %FSR HELLO message type.
#define FSR_ETX_HELLO_MSG  FSR_HELLO_MSG
/// %FSR TC message type.
#define FSR_ETX_TC_MSG     FSR_TC_MSG
/// %FSR MID message type.
#define FSR_ETX_MID_MSG    FSR_MID_MSG



#ifdef FSR_IPv6
#define ADDR_SIZE_DEFAULT   16
#else
#define ADDR_SIZE_DEFAULT   4
#endif

class FsrAddressSize
{
    public:
      static uint32_t ADDR_SIZE;
};

/// Maximum number of messages per packet.
#define FSR_MAX_MSGS       4
#define FSR_ETX_MAX_MSGS FSR_MAX_MSGS
/// Maximum number of hellos per message (4 possible link types * 3 possible nb types).
#define FSR_MAX_HELLOS     12
#define FSR_ETX_MAX_HELLOS FSR_MAX_HELLOS
/// Maximum number of addresses advertised on a message.
#define FSR_MAX_ADDRS      64
#define FSR_ETX_MAX_ADDRS FSR_MAX_ADDRS

/// Size (in bytes) of packet header.
#define FSR_PKT_HDR_SIZE   4
#define FSR_ETX_PKT_HDR_SIZE FSR_PKT_HDR_SIZE

/// Size (in bytes) of message header.
#define FSR_MSG_HDR_SIZE   8 + FsrAddressSize::ADDR_SIZE
#define FSR_ETX_MSG_HDR_SIZE FSR_MSG_HDR_SIZE

/// Size (in bytes) of hello header.
#define FSR_HELLO_HDR_SIZE 4
#define FSR_ETX_HELLO_HDR_SIZE FSR_HELLO_HDR_SIZE

/// Size (in bytes) of hello_msg header.
#define FSR_HELLO_MSG_HDR_SIZE 4
#define FSR_ETX_HELLO_MSG_HDR_SIZE FSR_HELLO_MSG_HDR_SIZE

/// Size (in bytes) of tc header.
#define FSR_TC_HDR_SIZE    4
#define FSR_ETX_TC_HDR_SIZE FSR_TC_HDR_SIZE


typedef struct Fsr_Etx_iface_address {

  /// Interface Address
  nsaddr_t  iface_address_;

  /// Link quality extension
  double  link_quality_;
  double  nb_link_quality_;

  /// Link delay extension
  double  link_delay_;
  double  nb_link_delay_;
  int parameter_qos_;

  inline nsaddr_t& iface_address() { return iface_address_; }

  /// Link quality extension
  inline double& link_quality() { return link_quality_; }
  inline double& nb_link_quality() { return nb_link_quality_; }

  inline double  etx()  {
    double mult = (double) (link_quality() * nb_link_quality());
    switch (parameter_qos_) {
    case FSR_ETX_BEHAVIOR_ETX:
      return (mult < 0.01) ? 100.0 : (double) 1.0 / (double) mult;
      break;

    case FSR_ETX_BEHAVIOR_ML:
      return mult;
      break;

    case FSR_ETX_BEHAVIOR_NONE:
    default:
      return 0.0;
      break;
    }
  }

  /// Link delay extension
  inline double& link_delay() { return link_delay_; }
  inline double& nb_link_delay() { return nb_link_delay_; }

} Fsr_Etx_iface_address;


/// Auxiliary struct which is part of the %FSR_ETX HELLO message (struct FSR_ETX_hello).
typedef struct Fsr_hello_msg {

  /// Link code.
  uint8_t  link_code_;
  /// Reserved.
  uint8_t  reserved_;
  /// Size of this link message.
  uint16_t  link_msg_size_;
  /// List of interface addresses of neighbor nodes.
  Fsr_Etx_iface_address  nb_iface_addrs_[FSR_MAX_ADDRS];
  /// Number of interface addresses contained in nb_iface_addrs_.
  int    count;

  inline uint8_t&  link_code()    { return link_code_; }
  inline uint8_t&  reserved()    { return reserved_; }
  inline uint16_t&  link_msg_size()    { return link_msg_size_; }
  inline Fsr_Etx_iface_address&  nb_etx_iface_addr(int i)  { return nb_iface_addrs_[i]; }
  inline nsaddr_t  & nb_iface_addr(int i)  { return nb_iface_addrs_[i].iface_address_;}
  inline void set_qos_behaviour(int bh) {for(int i=0;i<FSR_MAX_ADDRS;i++) nb_iface_addrs_[i].parameter_qos_=bh;}

  inline uint32_t size() { return FSR_HELLO_MSG_HDR_SIZE + count*FsrAddressSize::ADDR_SIZE; }

} FSR_ETX_hello_msg;

#if 0
/// Auxiliary struct which is part of the %FSR HELLO message (struct FSR_hello).
typedef struct Fsr_hello_msg {

        /// Link code.
    uint8_t link_code_;
    /// Reserved.
    uint8_t reserved_;
    /// Size of this link message.
    uint16_t    link_msg_size_;
    /// List of interface addresses of neighbor nodes.
    nsaddr_t    nb_iface_addrs_[FSR_MAX_ADDRS];
    /// Number of interface addresses contained in nb_iface_addrs_.
    int     count;

    inline uint8_t& link_code()     { return link_code_; }
    inline uint8_t& reserved()      { return reserved_; }
    inline uint16_t&    link_msg_size()     { return link_msg_size_; }
    inline nsaddr_t&    nb_iface_addr(int i)    { return nb_iface_addrs_[i]; }

    inline uint32_t size() { return FSR_HELLO_MSG_HDR_SIZE + count*FsrAddressSize::ADDR_SIZE; }

} Fsr_hello_msg;
#endif

/// %FSR HELLO message.

typedef struct Fsr_hello :cObject {

    /// Reserved.
    uint16_t    reserved_;
    /// HELLO emission interval in mantissa/exponent format.
    uint8_t htime_;
    /// Willingness of a node for forwarding packets on behalf of other nodes.
    uint8_t willingness_;
    /// List of FSR_hello_msg.
    Fsr_hello_msg  hello_body_[FSR_MAX_HELLOS];
    /// Number of FSR_hello_msg contained in hello_body_.
    int     count;

    inline uint16_t&    reserved()      { return reserved_; }
    inline uint8_t& htime()         { return htime_; }
    inline uint8_t& willingness()       { return willingness_; }
    inline Fsr_hello_msg&  hello_msg(int i)    { return hello_body_[i]; }

    inline uint32_t size() {
        uint32_t sz = FSR_HELLO_HDR_SIZE;
        for (int i = 0; i < count; i++)
            sz += hello_msg(i).size();
        return sz;
    }
} Fsr_hello;


/// %FSR TC message.
typedef struct Fsr_tc :cObject{

        /// Advertised Neighbor Sequence Number.
    uint16_t    ansn_;
    /// Reserved.
    uint16_t    reserved_;
    /// List of neighbors' main addresses.
    Fsr_Etx_iface_address  nb_main_addrs_[FSR_MAX_ADDRS];
    /// Number of neighbors' main addresses contained in nb_main_addrs_.
    int     count;

    inline  uint16_t&   ansn()          { return ansn_; }
    inline  uint16_t&   reserved()      { return reserved_; }
    inline  nsaddr_t&   nb_main_addr(int i) { return nb_main_addrs_[i].iface_address_; }
    inline  Fsr_Etx_iface_address& nb_etx_main_addr(int i) { return nb_main_addrs_[i]; }
    inline void set_qos_behaviour(int bh) {for(int i=0;i<FSR_MAX_ADDRS;i++) nb_main_addrs_[i].parameter_qos_=bh;}
    inline  uint32_t size() { return FSR_TC_HDR_SIZE + count*FsrAddressSize::ADDR_SIZE; }
} Fsr_tc;

#if 0
/// %FSR TC message.
typedef struct Fsr_tc :cObject{

        /// Advertised Neighbor Sequence Number.
    uint16_t    ansn_;
    /// Reserved.
    uint16_t    reserved_;
    /// List of neighbors' main addresses.
    nsaddr_t    nb_main_addrs_[FSR_MAX_ADDRS];
    /// Number of neighbors' main addresses contained in nb_main_addrs_.
    int     count;

    inline  uint16_t&   ansn()          { return ansn_; }
    inline  uint16_t&   reserved()      { return reserved_; }
    inline  nsaddr_t&   nb_main_addr(int i) { return nb_main_addrs_[i]; }

    inline  uint32_t size() { return FSR_TC_HDR_SIZE + count*FsrAddressSize::ADDR_SIZE; }

} Fsr_tc;
#endif

/// %FSR MID message.
typedef struct Fsr_mid :cObject{

    /// List of interface addresses.
    nsaddr_t    iface_addrs_[FSR_MAX_ADDRS];
    /// Number of interface addresses contained in iface_addrs_.
    int     count;

    inline nsaddr_t & iface_addr(int i) { return iface_addrs_[i]; }
    void    setIface_addr(int i,nsaddr_t a) {iface_addrs_[i]=a; }

    inline uint32_t size()          { return count*FsrAddressSize::ADDR_SIZE; }

} Fsr_mid;

#define MidSize sizeof (FSR_mid)
#define HelloSize sizeof (FSR_hello)
#define TcSize sizeof(FSR_tc)

#define MAXBODY (HelloSize > TcSize ?  (HelloSize > MidSize? HelloSize: MidSize):(TcSize > MidSize? TcSize:MidSize))


class   MsgBody {
public:
    Fsr_hello  hello_;
    Fsr_tc     tc_;
    Fsr_mid    mid_;


    MsgBody(MsgBody &other){
            memcpy((void*)&hello_,(void*)&other.hello_,sizeof(Fsr_hello));
            memcpy((void*)&tc_,(void*)&other.tc_,sizeof(Fsr_tc));
            memcpy((void*)&mid_,(void*)&other.mid_,sizeof(Fsr_mid));
        }
    MsgBody(){
            memset((void*)&hello_,0,sizeof(Fsr_hello));
            memset((void*)&tc_,0,sizeof(Fsr_tc));
            memset((void*)&mid_,0,sizeof(Fsr_mid));
          }
    ~MsgBody(){}
    MsgBody & operator =  (const MsgBody &other){
            if (this==&other) return *this;
                            memcpy((void*)&hello_,(void*)&other.hello_,sizeof(Fsr_hello));
                            memcpy((void*)&tc_,(void*)&other.tc_,sizeof(Fsr_tc));
                            memcpy((void*)&mid_,(void*)&other.mid_,sizeof(Fsr_mid));
                return *this;
            }
    Fsr_hello  * hello(){return &hello_;}
    Fsr_tc * tc(){return &tc_;}
    Fsr_mid* mid(){return &mid_;}


};

/// %FSR message.
class FsrMsg {
public:
    uint8_t msg_type_;  ///< Message type.
    uint8_t vtime_;     ///< Validity time.
    uint16_t    msg_size_;  ///< Message size (in bytes).
    nsaddr_t    orig_addr_; ///< Main address of the node which generated this message.
    uint8_t ttl_;       ///< Time to live (in hops).
    uint8_t hop_count_; ///< Number of hops which the message has taken.
    uint16_t    msg_seq_num_;   ///< Message sequence number.

    MsgBody msg_body_;          ///< Message body.
    inline  uint8_t&    msg_type()  { return msg_type_; }
    inline  uint8_t&    vtime()     { return vtime_; }
    inline  uint16_t&   msg_size()  { return msg_size_; }
    inline  nsaddr_t    & orig_addr()   { return orig_addr_; }
    inline  void    setOrig_addr(nsaddr_t a)    {orig_addr_=a; }
    inline  uint8_t&    ttl()       { return ttl_; }
    inline  uint8_t&    hop_count() { return hop_count_; }
    inline  uint16_t&   msg_seq_num()   { return msg_seq_num_; }
    inline  Fsr_hello& hello()     { return *(msg_body_.hello()); }
    inline  Fsr_tc&    tc()        { return *(msg_body_.tc()); }
    inline  Fsr_mid&   mid()       { return *(msg_body_.mid()); }

    std::string str() const {
        std::stringstream out;
        out << "type :"<< msg_type_ << " vtime :" << vtime_ << "orig_addr :" << orig_addr_;
        return out.str();
    }


    inline uint32_t size() {
        uint32_t sz = FSR_MSG_HDR_SIZE;
        if (msg_type() == FSR_HELLO_MSG)
            sz += hello().size();
        else if (msg_type() == FSR_TC_MSG)
            sz += tc().size();
        else if (msg_type() == FSR_MID_MSG)
            sz += mid().size();
        return sz;
    }
    FsrMsg(){}
    FsrMsg(const FsrMsg &other)
    {
        msg_type_=other.msg_type_;  ///< Message type.
        vtime_=other.vtime_;        ///< Validity time.
        msg_size_=other.msg_size_;  ///< Message size (in bytes).
        orig_addr_=other.orig_addr_;    ///< Main address of the node which generated this message.
        ttl_=other.ttl_;        ///< Time to live (in hops).
        hop_count_=other.hop_count_;    ///< Number of hops which the message has taken.
        msg_seq_num_=other.msg_seq_num_;    ///< Message sequence number.
        msg_body_=other.msg_body_;          ///< Message body.
    }

    FsrMsg & operator = (const FsrMsg &other)
    {
        if (this==&other) return *this;
        msg_type_=other.msg_type_;  ///< Message type.
        vtime_=other.vtime_;        ///< Validity time.
        msg_size_=other.msg_size_;  ///< Message size (in bytes).
        orig_addr_=other.orig_addr_;    ///< Main address of the node which generated this message.
        ttl_=other.ttl_;        ///< Time to live (in hops).
        hop_count_=other.hop_count_;    ///< Number of hops which the message has taken.
        msg_seq_num_=other.msg_seq_num_;    ///< Message sequence number.
        msg_body_=other.msg_body_;          ///< Message body.
        return *this;
    }
};

}
}

#endif
