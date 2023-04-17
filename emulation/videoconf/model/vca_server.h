#ifndef VCA_SERVER_H
#define VCA_SERVER_H

#include "ns3/application.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/simulator.h"
#include "ns3/assert.h"
#include "ns3/socket.h"
#include <deque>
#include <vector>
#include <unordered_map>

#include "prot-header.h"

namespace ns3
{

    enum DL_RATE_CONTROL_STATE
    {
        DL_RATE_CONTROL_STATE_NATRUAL,
        DL_RATE_CONTROL_STATE_LIMIT
    };

    class VcaServer : public Application
    {
    public:
        static TypeId GetTypeId(void);
        VcaServer();
        ~VcaServer();

        void SetLocalAddress(Ipv4Address local);
        void SetLocalUlPort(uint16_t port);
        void SetLocalDlPort(uint16_t port);
        void SetPeerDlPort(uint16_t port);

        void SetNodeId(uint32_t node_id);

    protected:
        void DoDispose(void);

    private:
        void StartApplication(void);
        void StopApplication(void);

        /**
         * \brief Connection Succeeded (called by Socket through a callback)
         * \param socket the connected socket
         */
        void ConnectionSucceededDl(Ptr<Socket> socket);
        /**
         * \brief Connection Failed (called by Socket through a callback)
         * \param socket the connected socket
         */
        void ConnectionFailedDl(Ptr<Socket> socket);
        /**
         * \brief Send more data as soon as some has been transmitted.
         *
         * Used in socket's SetSendCallback - params are forced by it.
         *
         * \param socket socket to use
         */
        void DataSendDl(Ptr<Socket> socket);

        /**
         * \brief Handle a packet received by the application
         * \param socket the receiving socket
         */
        void HandleRead(Ptr<Socket> socket);
        /**
         * \brief Handle an incoming connection
         * \param socket the incoming connection socket
         * \param from the address the connection is from
         */
        void HandleAccept(Ptr<Socket> socket, const Address &from);
        /**
         * \brief Handle an connection close
         * \param socket the connected socket
         */
        void HandlePeerClose(Ptr<Socket> socket);
        /**
         * \brief Handle an connection error
         * \param socket the connected socket
         */
        void HandlePeerError(Ptr<Socket> socket);

        Ptr<Packet> TranscodeFrame(uint8_t src_socket_id, uint8_t dst_socket_id, Ptr<Packet> packet, uint16_t frame_id);
        void SendData(Ptr<Socket> socket);
        void ReceiveData(Ptr<Packet>, uint8_t);

        void UpdateRate();

        uint32_t GetTargetFrameSize(uint8_t socket_id);

        uint32_t m_node_id;

        Ptr<Socket> m_socket_ul;
        std::list<Ptr<Socket>> m_socket_list_ul;
        std::list<Ptr<Socket>> m_socket_list_dl;
        std::unordered_map<uint32_t, uint8_t> m_socket_id_map;
        uint8_t m_socket_id;
        std::vector<Ipv4Address> m_peer_list;
        std::vector<std::deque<Ptr<Packet>>> m_send_buffer_list;
        std::vector<uint32_t> m_cc_target_frame_size;
        std::vector<std::unordered_map<uint8_t, uint32_t>> m_frame_size_forwarded; // vector index: src_socket_id, map key: dst_socket_id, value: frame_size_forwarded
        std::vector<std::unordered_map<uint8_t, uint16_t>> m_prev_frame_id;        // vector index: src_socket_id, map key: dst_socket_id, value: prev_frame_id

        TypeId m_tid;

        Ipv4Address m_local;
        uint16_t m_local_ul_port;
        uint16_t m_local_dl_port;
        uint16_t m_peer_dl_port;

        uint8_t m_fps;
        EventId m_update_rate_event;

        std::vector<double_t> m_dl_bitrate_reduce_factor;
        std::vector<DL_RATE_CONTROL_STATE> m_dl_rate_control_state;
        std::vector<uint32_t> m_capacity_frame_size;

        uint16_t m_num_degraded_users;
        
        //Decode DIY header in TCP payload
        std::vector<uint8_t> m_set_header;
        std::vector<uint8_t> m_status;
        /*
            m_status = 0   start to read header
            m_status = 1   continue to read header
            m_status = 2   start to read payload
            m_status = 3   continue to read payload
            m_status = 4   ready to send
        */
        std::vector<Ptr<Packet>> m_half_header;
        std::vector<Ptr<Packet>> m_half_payload;
        std::vector<VcaAppProtHeader> app_header;
        std::vector<uint32_t> m_payload_size;

        uint64_t m_tot_packetsize;
    }; // class VcaServer

}; // namespace ns3

#endif