#ifndef VCA_SERVER_H
#define VCA_SERVER_H

#include "ns3/application.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/simulator.h"
#include "ns3/assert.h"
#include "ns3/socket.h"

namespace ns3
{

    class VcaServer : public Application
    {
    public:
        static TypeId GetTypeId(void);
        VcaServer();
        ~VcaServer();

        Ptr<Socket> GetSocketUl(void);
        Ptr<Socket> GetSocketDl(void);

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

        void SendData(Ptr<Socket> socket, uint32_t size);
        void ReceiveData(Ptr<Packet>);

        Ptr<Socket> m_socket_ul;
        Ptr<Socket> m_socket_dl;
        std::list<Ptr<Socket>> m_socketList_ul;

        bool m_connected_dl;

        TypeId m_tid;

        Address m_local;
        Address m_peer;

        uint16_t m_localPort;

    }; // class VcaServer

}; // namespace ns3

#endif