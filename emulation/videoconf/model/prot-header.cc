#include "prot-header.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("VcaAppProtHeader");

    TypeId VcaAppProtHeader::GetTypeId()
    {
        static TypeId tid = TypeId("ns3::VcaAppProtHeader")
                                .SetParent<Header>()
                                .SetGroupName("videoconf")
                                .AddConstructor<VcaAppProtHeader>();
        return tid;
    };

    TypeId VcaAppProtHeader::GetInstanceTypeId(void) const
    {
        return GetTypeId();
    };

    VcaAppProtHeader::VcaAppProtHeader()
        : m_frame_id(0),
          m_packet_id(0){};

    VcaAppProtHeader::VcaAppProtHeader(uint16_t frame_id, uint32_t packet_id)
        : m_frame_id(frame_id),
          m_packet_id(packet_id){};

    VcaAppProtHeader::~VcaAppProtHeader(){};

    void
    VcaAppProtHeader::Serialize(Buffer::Iterator start) const
    {
        start.WriteHtonU16(m_frame_id);
        start.WriteHtonU32(m_packet_id);
    };

    uint32_t
    VcaAppProtHeader::Deserialize(Buffer::Iterator start)
    {
        m_frame_id = start.ReadNtohU16();
        m_packet_id = start.ReadNtohU32();
        return GetSerializedSize();
    };

    uint32_t
    VcaAppProtHeader::GetSerializedSize(void) const
    {
        return sizeof(m_frame_id) + sizeof(m_packet_id);
    };

    void
    VcaAppProtHeader::Print(std::ostream &os) const
    {
        os << "FrameId= " << m_frame_id << " PacketId= " << m_packet_id;
    };

    void
    VcaAppProtHeader::SetFrameId(uint16_t frame_id)
    {
        m_frame_id = frame_id;
    };

    void
    VcaAppProtHeader::SetPacketId(uint32_t packet_id)
    {
        m_packet_id = packet_id;
    };

    uint16_t
    VcaAppProtHeader::GetFrameId(void)
    {
        return m_frame_id;
    };

    uint32_t
    VcaAppProtHeader::GetPacketId(void)
    {
        return m_packet_id;
    };

} // namespace ns3