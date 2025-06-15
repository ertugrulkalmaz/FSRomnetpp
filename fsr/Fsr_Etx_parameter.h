#ifndef __FSR_ETX_parameter_h__
#define __FSR_ETX_parameter_h__

#include "inet/common/INETDefs.h"
#include "inet/common/INETMath.h"

namespace inet {

namespace inetmanet {

#define FSR_ETX_CAPPROBE_PACKET_SIZE 1000
#define CAPPROBE_MAX_ARRAY              16

class Fsr_Etx_parameter : public cObject
{
#define FSR_ETX_DEFAULT_MPR         1
#define FSR_ETX_MPR_R1              2
#define FSR_ETX_MPR_R2              3
#define FSR_ETX_MPR_QFSR           4
#define FSR_ETX_MPR_FSRD           5
        int mpr_algorithm_ = 0;
#define FSR_ETX_DEFAULT_ALGORITHM   1
#define FSR_ETX_DIJKSTRA_ALGORITHM  2
        int routing_algorithm_ = 0;
#define FSR_ETX_BEHAVIOR_NONE       1
#define FSR_ETX_BEHAVIOR_ETX        2
#define FSR_ETX_BEHAVIOR_ML         3
        int link_quality_ = 0;
        int fish_eye_ = -1;
#define FSR_ETX_TC_REDUNDANCY_MPR_SEL_SET               0
#define FSR_ETX_TC_REDUNDANCY_MPR_SEL_SET_PLUS_MPR_SET  1
#define FSR_ETX_TC_REDUNDANCY_FULL                      2
#define FSR_ETX_TC_REDUNDANCY_MPR_SET                   3
        int tc_redundancy_ = -1;
        int link_delay_ = -1;
        double c_alpha_ = NaN;

  public:
    inline Fsr_Etx_parameter() {}

    inline int&     mpr_algorithm() { return mpr_algorithm_; }
    inline int&     routing_algorithm() { return routing_algorithm_; }
    inline int&     link_quality() { return link_quality_; }
    inline int&     fish_eye() { return fish_eye_; }
    inline int&     tc_redundancy() { return tc_redundancy_; }
    inline int&     link_delay() { return link_delay_; }
    inline double&  c_alpha() { return c_alpha_; }

};

} // namespace inetmanet

} // namespace inet

#endif

