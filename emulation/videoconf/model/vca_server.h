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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "prot-header.h"

namespace ns3
{
    // must stay aligned with the value in solver.py
    const uint8_t MAX_NUM_USERS = 20;
    const uint16_t SOLVER_SOCKET_PORT = 11996;

    enum QOE_TYPE
    {
        QOE_TYPE_LIN,
        QOE_TYPE_LOG,
        QOE_TYPE_SQR_CONVEX,
        QOE_TYPE_SQR_CONCAVE
    };

    class ClientInfo : public Object
    {
    public:
        static TypeId GetTypeId(void);
        ClientInfo();
        ~ClientInfo();

        Ptr<Socket> socket_ul;
        Ptr<Socket> socket_dl;
        Ipv4Address ul_addr;
        std::deque<Ptr<Packet>> send_buffer;
        uint32_t cc_target_frame_size;
        uint32_t capacity_frame_size;
        std::unordered_map<uint8_t, uint32_t> frame_size_forwarded; // map key: dst_socket_id, value: frame_size_forwarded
        std::unordered_map<uint8_t, uint16_t> prev_frame_id;        // map key: dst_socket_id, value: prev_frame_id

        double_t dl_target_rate; // in kbps
        RATE_CONTROL_STATE dl_rate_control_state;

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

        double_t ul_rate; // in kbps
        double_t dl_rate; // in kbps

        double_t ul_target_rate; // in kbps

    }; // class ClientInfo

    class VcaServer : public Application
    {
    public:
        static TypeId GetTypeId(void);
        VcaServer();
        ~VcaServer();

        void SetLocalAddress(Ipv4Address local);
        void SetLocalAddress(std::list<Ipv4Address> local);
        void SetLocalUlPort(uint16_t port);
        void SetLocalDlPort(uint16_t port);
        void SetPeerDlPort(uint16_t port);

        void SetNodeId(uint32_t node_id);
        void SetPolicy(POLICY policy);
        void SetDlpercentage(double_t percentage);
        void SetSeparateSocket();
        void SetNumNode(uint8_t num_node);

        void SetRho(double_t rho);
        void SetQoEType(QOE_TYPE qoe_type);
        void SetMaxThroughput(double_t max_throughput_kbps);

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

        uint32_t GetDlAddr(uint32_t ulAddr, int node);

        void OptimizeAllocation();

        void UpdateCapacities();

        uint32_t GetFrameSizeFairShare(uint32_t cc_target_framesize);

        bool CheckOptResultsValidity();

        uint32_t m_node_id;

        uint8_t m_num_node;

        Ptr<Socket> m_socket_ul;
        std::list<Ipv4Address> m_local_list;
        std::list<Ptr<Socket>> m_socket_ul_list;
        std::unordered_map<uint32_t, uint8_t> m_ul_socket_id_map;
        std::unordered_map<uint32_t, uint8_t> m_dl_socket_id_map;
        uint8_t m_socket_id;

        std::unordered_map<uint8_t, Ptr<ClientInfo>> m_client_info_map;

        TypeId m_tid;

        Ipv4Address m_local;
        uint16_t m_local_ul_port;
        uint16_t m_local_dl_port;
        uint16_t m_peer_dl_port;

        uint8_t m_fps;
        EventId m_update_rate_event;

        uint16_t m_num_degraded_users;

        uint64_t m_total_packet_size;

        uint32_t kMinFrameSizeBytes = 5000;

        bool m_separate_socket;

        uint32_t forwarded_frame_size = 0;
        uint32_t dropped_frame_size = 0;
        uint32_t total_frame_size = 0;
        uint32_t last_time = 0;

        // optimization related

        struct OptParams
        {
            // don't change the order of the following parameters
            uint16_t num_users;
            uint16_t num_view = 25;
            QOE_TYPE qoe_type = QOE_TYPE_LIN;
            uint32_t rst = 0;
            double_t rho = 0.5;
            double_t max_bitrate_kbps = 30000;
            double_t qoe_func_alpha = 0.0;
            double_t qoe_func_beta = 0.0;
            double_t capacities_kbps[MAX_NUM_USERS];
        } m_opt_params;

        POLICY m_policy;
        double_t m_dl_percentage;

        double_t m_opt_alloc[MAX_NUM_USERS];

        int m_py_socket;

    }; // class VcaServer

}; // namespace ns3

#endif