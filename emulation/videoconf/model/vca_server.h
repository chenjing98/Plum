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

        Ptr<Packet> TranscodeFrame(uint8_t socket_id);
        void SendData(Ptr<Socket> socket);
        void ReceiveData(Ptr<Packet>, uint8_t);

        uint32_t m_node_id;

        Ptr<Socket> m_socket_ul;
        std::list<Ptr<Socket>> m_socket_list_ul;
        std::list<Ptr<Socket>> m_socket_list_dl;
        std::unordered_map<Ptr<Socket>, uint8_t> m_socket_id_map;
        uint8_t m_socket_id;
        std::vector<Ipv4Address> m_peer_list;
        std::vector<std::deque<Ptr<Packet>>> m_send_buffer_list;

        TypeId m_tid;

        Ipv4Address m_local;
        uint16_t m_local_ul_port;
        uint16_t m_local_dl_port;
        uint16_t m_peer_dl_port;

    }; // class VcaServer

}; // namespace ns3

#endif