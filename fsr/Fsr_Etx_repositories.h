#ifndef __FSR_ETX_repositories_h__
#define __FSR_ETX_repositories_h__

#include <assert.h>
#include "inet/routing/extras/fsr/Fsr_Etx_parameter.h"
#include "inet/routing/extras/fsr/Fsr_repositories.h"
#include "inet/routing/extras/fsr/FsrPkt_m.h"

namespace inet {

namespace inetmanet {

#ifndef CURRENT_TIME_OWER
#define CURRENT_TIME_OWER   SIMTIME_DBL(simTime())
#endif


typedef Fsr_rt_entry FSR_ETX_rt_entry;

typedef Fsr_iface_assoc_tuple FSR_ETX_iface_assoc_tuple;

#define DEFAULT_LOSS_WINDOW_SIZE   10

#define FSR_ETX_MAX_SEQ_NUM     65535

typedef struct Fsr_Etx_link_tuple : public Fsr_link_tuple
{
    cSimpleModule * owner_;
    inline void  set_owner(cSimpleModule *p) {owner_ = p;}

    unsigned char   loss_bitmap_ [16];
    uint16_t       loss_seqno_;
    int             loss_index_;
    int             loss_window_size_;
    int             lost_packets_;
    int             total_packets_;
    int             loss_missed_hellos_;
    double          next_hello_;
    double          hello_ival_;

    double          link_quality_;
    double          nb_link_quality_;
    double          etx_;

    Fsr_Etx_parameter parameter_;

    inline void set_qos_behaviour(Fsr_Etx_parameter parameter) {parameter_ = parameter;}
    inline void link_quality_init(uint16_t seqno, int loss_window_size)
    {
        assert(loss_window_size > 0);

        memset(loss_bitmap_, 0, sizeof(loss_bitmap_));
        loss_window_size_ = loss_window_size;
        loss_seqno_ = seqno;

        total_packets_ = 0;
        lost_packets_ = 0;
        loss_index_ = 0;
        loss_missed_hellos_ = 0;

        link_quality_ = 0;
        nb_link_quality_ = 0;
        etx_ = 0;
    }

#if 0
    inline double link_quality()
    {
        double result = (double)(total_packets_ - lost_packets_) /
                        (double)(loss_window_size_);
        return result;
    }
#endif

    inline double link_quality() { return link_quality_; }
    inline double nb_link_quality() { return nb_link_quality_; }
    inline double etx() { return etx_; }
#if 0
    inline double  etx()
    {
        double mult = link_quality() * nb_link_quality();
        switch (parameter_.link_delay())
        {
        case FSR_ETX_BEHAVIOR_ETX:
            return (mult < 0.01) ? 100.0 : 1.0 / mult;
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
#endif

    inline void update_link_quality(double nb_link_quality)
    {
        nb_link_quality_ = nb_link_quality;

        double mult = link_quality_ * nb_link_quality_;
        switch (parameter_.link_quality())
        {
        case FSR_ETX_BEHAVIOR_ETX:
            etx_ = (mult < 0.01) ? 100.0 : 1.0 / mult;
            break;

        case FSR_ETX_BEHAVIOR_ML:
            etx_ = mult;
            break;

        case FSR_ETX_BEHAVIOR_NONE:
        default:
            etx_ = 0.0;
            break;
        }
    }


    inline double& next_hello() { return next_hello_; }

    inline double& hello_ival() { return hello_ival_; }
#if 0
    inline void process_packet(bool received, double htime)
    {
        unsigned char mask = 1 << (loss_index_ & 7);
        int index = loss_index_ >> 3;
        if (received)
        {
            if ((loss_bitmap_[index] & mask) != 0)
            {
                loss_bitmap_[index] &= ~mask;
                lost_packets_--;
            }
            hello_ival_ = htime;
            next_hello_ = CURRENT_TIME_OWER + hello_ival_;
        }
        else
        {
            if ((loss_bitmap_[index] & mask ) == 0)
            {
                loss_bitmap_[index] |= mask;
                lost_packets_++;
            }
        }
        loss_index_ = (loss_index_ + 1) % (loss_window_size_);
        if (total_packets_ < loss_window_size_)
            total_packets_++;
    }

#endif

    inline void process_packet(bool received, double htime)
    {
        unsigned char mask = 1 << (loss_index_ & 7);
        int index = loss_index_ >> 3;
        if (received)
        {
            // packet not lost
            if ((loss_bitmap_[index] & mask) != 0)
            {
                loss_bitmap_[index] &= ~mask;
                lost_packets_--;
            }
            hello_ival_ = htime;
            next_hello_ = CURRENT_TIME_OWER + hello_ival_;
        }
        else   // if (!received)
        {
            // packet lost
            if ((loss_bitmap_[index] & mask ) == 0)
            {
                loss_bitmap_[index] |= mask;
                lost_packets_++;
            }
        }
        loss_index_ = (loss_index_ + 1) % (loss_window_size_);
        if (total_packets_ < loss_window_size_)
            total_packets_++;

        link_quality_ = (double)(total_packets_ - lost_packets_) /
                        (double)(loss_window_size_);

        double mult = link_quality_ * nb_link_quality_;
        switch (parameter_.link_quality())
        {
        case FSR_ETX_BEHAVIOR_ETX:
            etx_ = (mult < 0.01) ? 100.0 : 1.0 / mult;
            break;

        case FSR_ETX_BEHAVIOR_ML:
            etx_ = mult;
            break;
        case FSR_ETX_BEHAVIOR_NONE:
        default:
            etx_ = 0.0;
            break;
        }
    }

    inline void receive(uint16_t seqno, double htime)
    {
        while (seqno != loss_seqno_)
        {
            if (loss_missed_hellos_ == 0)
                process_packet(false, htime);
            else
                loss_missed_hellos_--;
            loss_seqno_ = (loss_seqno_ + 1) % (FSR_ETX_MAX_SEQ_NUM + 1);
        }
        process_packet(true, htime);
        loss_missed_hellos_ = 0;
        loss_seqno_ = (seqno + 1) % (FSR_ETX_MAX_SEQ_NUM + 1);
    }

    inline void packet_timeout()
    {
        process_packet(false, 0.0);
        loss_missed_hellos_++;
        next_hello_ = CURRENT_TIME_OWER + hello_ival_;
    }

    double link_delay_;
    double nb_link_delay_;
    double recv1_ [CAPPROBE_MAX_ARRAY];
    double recv2_ [CAPPROBE_MAX_ARRAY];

    inline void link_delay_init()
    {

        link_delay_ = 4;
        nb_link_delay_ = 6;
        for (int i = 0; i < CAPPROBE_MAX_ARRAY; i++)
            recv1_ [i] = recv2_ [i] = -1;
    }

    inline void link_delay_computation(FsrPkt* pkt)
    {
        double c_alpha = parameter_.c_alpha();
        int i;

        if (pkt->sn() % CAPPROBE_MAX_ARRAY % 2 == 0)
        {
            i = ((pkt->sn() % CAPPROBE_MAX_ARRAY) - 1) / 2;
            recv2_[i] = CURRENT_TIME_OWER - pkt->send_time();
            if (recv1_[i] > 0)
            {
                double disp = recv2_[i] - recv1_[i];
                if (disp > 0)
                    link_delay_ = c_alpha * disp +
                            (1 - c_alpha) * (link_delay_);
            }
            recv1_[i] = -1;
            recv2_[i] = -1;
        }
        else
        {
            i = pkt->sn() % CAPPROBE_MAX_ARRAY / 2;

            recv1_[i] = CURRENT_TIME_OWER - pkt->send_time();
            recv2_[i] = 0;
        }
    }

    inline double link_delay() { return link_delay_; }
    inline double nb_link_delay() { return nb_link_delay_; }

    inline void update_link_delay(double nb_link_delay)
    {
        nb_link_delay_ = nb_link_delay;
    }

    inline void mac_failed()
    {
        memset(loss_bitmap_, 0, sizeof(loss_bitmap_));

        total_packets_ = 0;
        lost_packets_ = 0;
        loss_index_ = 0;
        loss_missed_hellos_ = 0;

        link_delay_ = 10;
        nb_link_delay_ = 10;
        for (int i = 0; i < CAPPROBE_MAX_ARRAY; i++)
            recv1_ [i] = recv2_ [i] = -1;
    }
    Fsr_Etx_link_tuple() {asocTimer = nullptr;}
    Fsr_Etx_link_tuple(const Fsr_Etx_link_tuple * e)
    {
        memcpy((void*)this, (void*)e, sizeof(Fsr_Etx_link_tuple));
        asocTimer = nullptr;
    }
    virtual Fsr_Etx_link_tuple *dup() const override {return new Fsr_Etx_link_tuple(this);}

} FSR_ETX_link_tuple;

typedef Fsr_nb_tuple FSR_ETX_nb_tuple;

typedef struct FSR_ETX_nb2hop_tuple : public Fsr_nb2hop_tuple
{
    double link_quality_;
    double nb_link_quality_;
    double etx_;

    inline double  link_quality() { return link_quality_; }
    inline double  nb_link_quality() { return nb_link_quality_; }
    inline double  etx() { return etx_; }
    int parameter_qos_;

    inline void set_qos_behaviour(int bh) {parameter_qos_ = bh;}

    inline void update_link_quality(double link_quality, double nb_link_quality)
    {
        // update link quality information
        link_quality_ = link_quality;
        nb_link_quality_ = nb_link_quality;

        // calculate the new etx value
        double mult = link_quality_ * nb_link_quality_;
        switch (parameter_qos_)
        {
        case FSR_ETX_BEHAVIOR_ETX:
            etx_ = (mult < 0.01) ? 100.0 : 1.0 / mult;
            break;

        case FSR_ETX_BEHAVIOR_ML:
            etx_ = mult;
            break;

        case FSR_ETX_BEHAVIOR_NONE:
        default:
            etx_ = 0.0;
            break;
        }
    }
    /// Link delay extension
    double  link_delay_;
    double nb_link_delay_;
    inline double  link_delay() { return link_delay_; }
    inline double  nb_link_delay() {return nb_link_delay_;}

    inline void update_link_delay(double link_delay, double nb_link_delay)
    {
        link_delay_ = link_delay;
        nb_link_delay_ = nb_link_delay;
    }
    inline double&  time()    { return time_; }

    FSR_ETX_nb2hop_tuple() {asocTimer = nullptr;}
    FSR_ETX_nb2hop_tuple(const FSR_ETX_nb2hop_tuple *e)
    {
        memcpy((void*)this, (void*)e, sizeof(FSR_ETX_nb2hop_tuple));
        asocTimer = nullptr;
    }
    virtual FSR_ETX_nb2hop_tuple *dup() const override{return new FSR_ETX_nb2hop_tuple(this);}

} FSR_ETX_nb2hop_tuple;

/// An MPR-Selector Tuple.
typedef Fsr_mprsel_tuple FSR_ETX_mprsel_tuple;

/// A Duplicate Tuple
typedef Fsr_dup_tuple FSR_ETX_dup_tuple;

/// A Topology Tuple
typedef struct FSR_ETX_topology_tuple : public Fsr_topology_tuple
{
    /// Link delay extension
    double  link_quality_;
    double nb_link_quality_;
    double etx_;

    int parameter_qos_;

    inline void set_qos_behaviour(int bh) {parameter_qos_ = bh;}
#if 0
    inline double  etx()
    {
        double mult = link_quality() * nb_link_quality();
        switch (parameter_qos_)
        {
        case FSR_ETX_BEHAVIOR_ETX:
            return (mult < 0.01) ? 100.0 : 1.0 / mult;
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
#endif


    inline double  link_quality() { return link_quality_; }
    inline double  nb_link_quality() { return nb_link_quality_; }
    inline double  etx() { return etx_; }

    inline void update_link_quality(double link_quality, double nb_link_quality)
    {
        // update link quality information
        link_quality_ = link_quality;
        nb_link_quality_ = nb_link_quality;

        // calculate the new etx value
        double mult = link_quality_ * nb_link_quality_;
        switch (parameter_qos_)
        {
        case FSR_ETX_BEHAVIOR_ETX:
            etx_ = (mult < 0.01) ? 100.0 : 1.0 / mult;
            break;

        case FSR_ETX_BEHAVIOR_ML:
            etx_ = mult;
            break;
        case FSR_ETX_BEHAVIOR_NONE:
        default:
            etx_ = 0.0;
            break;
        }
    }

/// Link delay extension
    double  link_delay_;
    double nb_link_delay_;


    inline double  link_delay() { return link_delay_; }
    inline double  nb_link_delay() { return nb_link_delay_; }

    inline void update_link_delay(double link_delay, double nb_link_delay)
    {
        link_delay_ = link_delay;
        nb_link_delay_ = nb_link_delay;
    }

    inline uint16_t&  seq()    { return seq_; }
    inline double&    time()    { return time_; }

    FSR_ETX_topology_tuple() {asocTimer = nullptr;}
    FSR_ETX_topology_tuple(const FSR_ETX_topology_tuple *e)
    {
        memcpy((void*)this, (void*)e, sizeof(FSR_ETX_topology_tuple));
        asocTimer = nullptr;
    }
    virtual FSR_ETX_topology_tuple *dup() const override {return new FSR_ETX_topology_tuple(this);}

} FSR_ETX_topology_tuple;

} // namespace inetmanet

} // namespace inet

#endif

