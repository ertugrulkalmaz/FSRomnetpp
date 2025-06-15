package inet.routing.extras;
import inet.routing.extras.base.BaseRouting;

simple Fsr extends BaseRouting
{
    parameters:
        @class(Fsr);
        UdpPort = default(698);
        int Willingness = default(3);
        double Hello_ival @unit(s) = default(2s);
        double Tc_ival @unit(s) = default(5s);
        double Mid_ival @unit(s) = default(5s); 
        int use_mac = default(0);
        bool UseIndex = default(false);
        bool reduceFuncionality = default(false);
        double FSR_REFRESH_INTERVAL @unit("s") = default(2s);
        bool PublicRoutingTables = default(false);

        bool optimizedMid = default(false);
        bool DelOnlyRtEntriesInrtable_ = default(false);
}



