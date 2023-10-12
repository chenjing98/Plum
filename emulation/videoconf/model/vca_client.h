#ifndef VCA_CLIENT_H
#define VCA_CLIENT_H

#include "ns3/application.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/simulator.h"
#include "ns3/assert.h"
#include "ns3/socket.h"
#include <deque>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <map>

#include <fstream>

#include "prot-header.h"

enum POLICY
{
    VANILLA,
    YONGYULE,
    SERVINIT
};

enum YONGYULE_REALIZATION
{
    YONGYULE_RWND,
    YONGYULE_APPRATE
};

enum PROBE_STATE
{
    YIELD,
    PROBING,
    NATURAL
};

namespace ns3
{
    class PktInfo : public Object
    {
    public:
        PktInfo();
        ~PktInfo();


        // Decode self-defined header in TCP payload
        uint8_t set_header;
        uint8_t read_status;
        /*
            m_status = 0   start to read header
            m_status = 1   continue to read header
            m_status = 2   start to read payload
            m_status = 3   continue to read payload
            m_status = 4   ready to send
        */
        uint32_t payload_size;
        Ptr<Packet> half_header;
        Ptr<Packet> half_payload;
        VcaAppProtHeader app_header;

    }; // class ClientInfo

    class VcaClient : public Application
    {
    public:
        static TypeId GetTypeId(void);
        VcaClient();
        ~VcaClient();

        void SetLocalAddress(Ipv4Address local);
        void SetLocalAddress(Ipv4Address local_ul, Ipv4Address local_dl);
        void SetPeerAddress(std::vector<Ipv4Address> peer_list);
        void SetLocalUlPort(uint16_t port);
        void SetLocalDlPort(uint16_t port);
        void SetPeerPort(uint16_t port);

        Ptr<Socket> GetSocketDl(void);

        void SetFps(uint8_t fps);
        void SetBitrate(std::vector<uint32_t> bitrate);
        void SetMaxBitrate(uint32_t bitrate);
        void SetMinBitrate(uint32_t bitrate);

        void SetNodeId(uint32_t node_id);
        void SetNumNode(uint8_t num_node);

        void SetPolicy(POLICY policy);

        void StopEncodeFrame();

        void SetLogFile(std::string log_file);

        void SetUlDlParams(uint32_t, double_t);

        void SetUlThresh(uint32_t, uint32_t);

        static const uint32_t payloadSize = 1436; // internet TCP MTU = 576B, - 20B(IP header) - 20B(TCP header) - 12B(VCA header)

    protected:
        void DoDispose(void);

    private:
        void StartApplication(void);
        void StopApplication(void);

        void ConnectionSucceededUl(Ptr<Socket> socket);
        void ConnectionFailedUl(Ptr<Socket> socket);
        void DataSendUl(Ptr<Socket> socket);

        void HandleRead(Ptr<Socket> socket);
        void HandleRead2(Ptr<Packet> packet);
        void HandleAccept(Ptr<Socket> socket, const Address &from);
        void HandlePeerClose(Ptr<Socket> socket);
        void HandlePeerError(Ptr<Socket> socket);

        void SendData(Ptr<Socket> socket);
        void SendData();
        void ReceiveData(Ptr<Packet> packet);

        void EncodeFrame();
        void UpdateDutyRatio();
        void UpdateEncodeBitrate();

        void AdjustBw();
        bool IsBottleneck();
        bool IsLowRate();
        bool IsHighRate();
        uint64_t GetUlBottleneckBw();
        bool ShouldRecoverDl();
        double_t DecideDlParam();
        void EnforceDlParam(double_t param);
        bool ElasticTest();

        void OutputStatistics();

        double_t GetDutyRatio(uint8_t);

        uint32_t m_node_id;
        uint8_t m_num_node;

        Ptr<Socket> m_socket_dl;

        std::list<Ptr<Socket>> m_socket_list_ul;
        std::list<Ptr<Socket>> m_socket_list_dl;
        std::unordered_map<Ptr<Socket>, uint8_t> m_socket_id_map_ul;
        uint8_t m_socket_id_ul;
        std::unordered_map<Ptr<Socket>, uint8_t> m_socket_id_map_dl;
        uint8_t m_socket_id_dl;
        std::vector<Ipv4Address> m_peer_list;

        TypeId m_tid;

        Ipv4Address m_local_ul;
        Ipv4Address m_local_dl;

        uint16_t m_local_ul_port;
        uint16_t m_local_dl_port;
        uint16_t m_peer_ul_port;

        uint8_t m_fps;
        uint32_t m_max_bitrate; // in kbps
        uint32_t m_min_bitrate; // in kbps
        uint16_t m_frame_id;

        EventId m_enc_event;
        std::vector<uint32_t> m_bitrateBps;
        std::vector<uint32_t> m_txBufSize;
        std::vector<uint32_t> m_lastPendingBuf;
        std::vector<bool> m_firstUpdate;

        uint64_t m_total_packet_bit;
        std::vector<std::unordered_map<uint32_t, uint32_t>> m_transientRateBps; // vector index: time in second, map key: source ip, map value: bitrate in bps

        std::vector<std::deque<Ptr<Packet>>> m_send_buffer_pkt;
        std::vector<std::deque<Ptr<VcaAppProtHeaderInfo>>> m_send_buffer_hdr;

        bool m_is_my_wifi_access_bottleneck;

        POLICY m_policy;

        YONGYULE_REALIZATION m_yongyule_realization;

        uint32_t m_target_dl_bitrate_redc_factor;

        EventId m_eventUpdateDuty;

        uint32_t kTxRateUpdateWindowMs;
        uint32_t kMinEncodeBps;
        uint32_t kMaxEncodeBps;
        double_t kTargetDutyRatio;
        double_t kDampingCoef;

        std::vector<std::vector<bool>> m_dutyState;
        std::vector<std::deque<int64_t>> m_time_history;
        std::vector<std::deque<uint32_t>> m_write_history;

        uint64_t m_total_bitrate;
        uint32_t m_encode_times;

        bool m_increase_ul;

        std::string m_log_file;

        PROBE_STATE m_probe_state;
        bool is_half_duplex_bottleneck;
        uint32_t m_prev_ul_bitrate;
        uint64_t m_prev_ul_bottleneck_bw;

        uint32_t kUlImprove;
        double_t kDlYield;
        uint32_t kLowUlThresh;
        uint32_t kHighUlThresh;

        bool m_log;

        uint16_t m_probe_cooloff_count;
        uint16_t m_probe_cooloff_count_max;
        uint16_t m_probe_patience_count;
        uint16_t m_probe_patience_count_max;

        Ptr<PktInfo> pkt_info;
    }; // class VcaClient

    // enum DL_RATE_CONTROL_STATE
    // {
    //     DL_RATE_CONTROL_STATE_NATRUAL,
    //     DL_RATE_CONTROL_STATE_LIMIT
    // };
    // class ClientInfo : public Object
    // {
    // public:
    //     static TypeId GetTypeId(void);
    //     ClientInfo();
    //     ~ClientInfo();

    //     Ptr<Socket> socket_ul;
    //     Ptr<Socket> socket_dl;
    //     Ipv4Address ul_addr;
    //     std::deque<Ptr<Packet>> send_buffer;
    //     uint32_t cc_target_frame_size;
    //     uint32_t capacity_frame_size;
    //     std::unordered_map<uint8_t, uint32_t> frame_size_forwarded; // map key: dst_socket_id, value: frame_size_forwarded
    //     std::unordered_map<uint8_t, uint16_t> prev_frame_id;        // map key: dst_socket_id, value: prev_frame_id

    //     double_t dl_bitrate_reduce_factor;
    //     DL_RATE_CONTROL_STATE dl_rate_control_state;

    //     // Decode self-defined header in TCP payload
    //     uint8_t set_header;
    //     uint8_t read_status;
    //     /*
    //         m_status = 0   start to read header
    //         m_status = 1   continue to read header
    //         m_status = 2   start to read payload
    //         m_status = 3   continue to read payload
    //         m_status = 4   ready to send
    //     */
    //     uint32_t payload_size;
    //     Ptr<Packet> half_header;
    //     Ptr<Packet> half_payload;
    //     VcaAppProtHeader app_header;

    //     double lambda;

    // }; // class ClientInfo

}; // namespace ns3

#endif