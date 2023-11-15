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
          m_packet_id(0),
          m_payload_size(1448 - VCA_APP_PROT_HEADER_LENGTH),
          m_src_id(0),
          m_ul_target_rate_bps(1e4){};

    VcaAppProtHeader::VcaAppProtHeader(uint16_t frame_id, uint16_t packet_id)
        : m_frame_id(frame_id),
          m_packet_id(packet_id),
          m_payload_size(1448 - VCA_APP_PROT_HEADER_LENGTH),
          m_src_id(0),
          m_ul_target_rate_bps(1e4){};

    VcaAppProtHeader::~VcaAppProtHeader(){};

    void
    VcaAppProtHeader::Serialize(Buffer::Iterator start) const
    {
        start.WriteHtonU16(m_frame_id);
        start.WriteHtonU16(m_packet_id);
        start.WriteHtonU32(m_payload_size);
        start.WriteHtonU32(m_src_id);
        start.WriteHtonU32(m_ul_target_rate_bps);
    };

    uint32_t
    VcaAppProtHeader::Deserialize(Buffer::Iterator start)
    {
        m_frame_id = start.ReadNtohU16();
        m_packet_id = start.ReadNtohU16();
        m_payload_size = start.ReadNtohU32();
        m_src_id = start.ReadNtohU32();
        m_ul_target_rate_bps = start.ReadNtohU32();
        return GetSerializedSize();
    };

    uint32_t
    VcaAppProtHeader::GetSerializedSize(void) const
    {
        return sizeof(m_frame_id) + sizeof(m_packet_id) + sizeof(m_src_id) + sizeof(m_ul_target_rate_bps) + sizeof(m_payload_size);
    };

    void
    VcaAppProtHeader::Print(std::ostream &os) const
    {
        os << "FrameId= " << m_frame_id << " PacketId= " << m_packet_id << " PayloadSize= " << m_payload_size << " SrcId= " << m_src_id << " TargetFrameSize= " << m_ul_target_rate_bps;
    };

    void
    VcaAppProtHeader::SetFrameId(uint16_t frame_id)
    {
        m_frame_id = frame_id;
    };

    void
    VcaAppProtHeader::SetPacketId(uint16_t packet_id)
    {
        m_packet_id = packet_id;
    };

    void
    VcaAppProtHeader::SetUlTargetRate(uint32_t new_factor)
    {
        m_ul_target_rate_bps = new_factor;
    };

    void
    VcaAppProtHeader::SetPayloadSize(uint32_t payload_size)
    {
        m_payload_size = payload_size;
    };

    void
    VcaAppProtHeader::SetSrcId(uint32_t src_id)
    {
        m_src_id = src_id;
    };

    uint16_t
    VcaAppProtHeader::GetFrameId(void)
    {
        return m_frame_id;
    };

    uint16_t
    VcaAppProtHeader::GetPacketId(void)
    {
        return m_packet_id;
    };

    uint32_t
    VcaAppProtHeader::GetUlTargetRate(void)
    {
        return m_ul_target_rate_bps;
    };

    uint32_t
    VcaAppProtHeader::GetPayloadSize(void)
    {
        return m_payload_size;
    };

    uint32_t
    VcaAppProtHeader::GetSrcId(void)
    {
        return m_src_id;
    };

    void
    VcaAppProtHeader::Reset()
    {
        m_frame_id = 0;
        m_packet_id = 0;
        m_payload_size = 1448 - VCA_APP_PROT_HEADER_LENGTH;
        m_src_id = 0;
        m_ul_target_rate_bps = 1e4;
    };

} // namespace ns3