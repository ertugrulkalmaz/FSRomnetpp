import inet.common.INETDefs;
import inet.common.packet.chunk.Chunk;
import inet.networklayer.common.L3Address;
import inet.routing.extras.fsr.FsrMsg;



namespace inet::inetmanet;

class FsrPkt extends FieldsChunk
{
    @omitGetVerb(true);
    @fieldNameSuffix("_var");
    bool reduceFuncionality=false;
    short pkt_seq_num;          ///< Packet sequence number.
    long sn;                    /// CapProbe packet sequence number
    double send_time;           /// Packet sending timestamp
    FsrMsg msg[];             ///< Packet body.
}

