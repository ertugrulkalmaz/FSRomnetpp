package inet.routing.extras;

import inet.routing.extras.base.BaseRouting;

simple Fsr_Etx extends BaseRouting
{
    parameters:
        @class(Fsr_Etx);

        UdpPort = default(698);
        int Willingness = default(3);
        double Hello_ival @unit(s) = default(2s);
        double Tc_ival @unit(s) = default(5s);
        double Mid_ival @unit(s) = default(5s);
        int use_mac = default(0);
        bool UseIndex = default(false);
        bool reduceFuncionality = default(false);
        int Mpr_algorithm = default(1);
        int routing_algorithm = default(2);
        int Link_quality = default(2);
        bool Fish_eye = true;
        int Tc_redundancy = default(1);
        bool Link_delay = default(true);
        double C_alpha = default(0.6);
        double FSR_REFRESH_INTERVAL @unit("s") = default(2s);
        bool PublicRoutingTables = default(false);

        bool optimizedMid = default(false);
        bool DelOnlyRtEntriesInrtable_ = default(false);
}

