#include "vca_server.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("VcaServer");

    TypeId VcaServer::GetTypeId()
    {
        static TypeId tid = TypeId("ns3::VcaServer")
                                .SetParent<Application>()
                                .SetGroupName("videoconf")
                                .AddConstructor<VcaServer>();
        return tid;
    };

    VcaServer::VcaServer()
        : m_socket_ul(nullptr),
          m_socket_dl(nullptr),
          m_socketList_ul(),
          m_connected_dl(false),
          m_tid(TypeId::LookupByName("ns3::TcpSocketFactory")){};

    VcaServer::~VcaServer(){};

    Ptr<Socket>
    VcaServer::GetSocketUl(void)
    {
        return m_socket_ul;
    };

    Ptr<Socket>
    VcaServer::GetSocketDl(void)
    {
        return m_socket_dl;
    };

    void
    VcaServer::DoDispose()
    {
        m_socket_ul = nullptr;
        m_socket_dl = nullptr;
        m_socketList_ul.clear();
        Application::DoDispose();
    };

    // Application Methods
    void
    VcaServer::StartApplication()
    {

        // Create the socket if not already
        if (!m_socket_dl)
        {
            m_socket_dl = Socket::CreateSocket(GetNode(), m_tid);

            if (m_socket_dl->Bind(m_local) == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }

            m_socket_dl->Connect(m_peer);
            m_socket_dl->ShutdownRecv();
            m_socket_dl->SetConnectCallback(
                MakeCallback(&VcaServer::ConnectionSucceededDl, this),
                MakeCallback(&VcaServer::ConnectionFailedDl, this));
            m_socket_dl->SetSendCallback(
                MakeCallback(&VcaServer::DataSendDl, this));
        }

        if (!m_socket_ul)
        {
            m_socket_ul = Socket::CreateSocket(GetNode(), m_tid);
            if (m_socket_ul->Bind(m_local) == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket_ul->Listen();
            m_socket_ul->ShutdownSend();

            if (InetSocketAddress::IsMatchingType(m_local))
            {
                m_localPort = InetSocketAddress::ConvertFrom(m_local).GetPort();
            }
            else if (Inet6SocketAddress::IsMatchingType(m_local))
            {
                m_localPort = Inet6SocketAddress::ConvertFrom(m_local).GetPort();
            }
            else
            {
                m_localPort = 0;
            }
            m_socket_ul->SetRecvCallback(MakeCallback(&VcaServer::HandleRead, this));
            m_socket_ul->SetRecvPktInfo(true);
            m_socket_ul->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                                           MakeCallback(&VcaServer::HandleAccept, this));
            m_socket_ul->SetCloseCallbacks(MakeCallback(&VcaServer::HandlePeerClose, this),
                                           MakeCallback(&VcaServer::HandlePeerError, this));
        }

        if (m_connected_dl)
        {
            // send data
            SendData(m_socket_dl, 1500);
        }
    };

    void
    VcaServer::StopApplication()
    {
        while (!m_socketList_ul.empty())
        { // these are accepted sockets, close them
            Ptr<Socket> acceptedSocket = m_socketList_ul.front();
            m_socketList_ul.pop_front();
            acceptedSocket->Close();
        }
        if (m_socket_ul)
        {
            m_socket_ul->Close();
            m_socket_ul->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        }
        if (m_socket_dl)
        {
            m_socket_dl->Close();
            m_connected_dl = false;
        }
    };

    // private helpers

    void
    VcaServer::ConnectionSucceededDl(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaServer] Downlink connection succeeded");
        m_connected_dl = true;

        // TODO: send data
        SendData(socket, 1500);
    };

    void
    VcaServer::ConnectionFailedDl(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaServer] Connection Failed");
    };

    void
    VcaServer::DataSendDl(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaServer] Downlink dataSend");
        if (m_connected_dl)
        {
            // TODO: send data
            SendData(socket, 1500);
        }
    };

    void
    VcaServer::HandleRead(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaServer] HandleRead");
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from)))
        {
            if (packet->GetSize() == 0)
            { // EOF
                break;
            }

            if (InetSocketAddress::IsMatchingType(from))
            {
                NS_LOG_LOGIC("[VcaServer][ReceivedPkt] Time= " << Simulator::Now().GetSeconds() << " PktSize(B)= " << packet->GetSize() << " SrcIp= " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " SrcPort= " << InetSocketAddress::ConvertFrom(from).GetPort());
                // TODO: handle received data
            }
            else if (Inet6SocketAddress::IsMatchingType(from))
            {
                NS_LOG_LOGIC("[VcaServer][ReceivedPkt] Time= " << Simulator::Now().GetSeconds() << " PktSize(B)= " << packet->GetSize() << " SrcIp= " << Inet6SocketAddress::ConvertFrom(from).GetIpv6() << " SrcPort= " << Inet6SocketAddress::ConvertFrom(from).GetPort());
                // TODO: handle received data
            }
        }
    };

    void
    VcaServer::HandleAccept(Ptr<Socket> socket, const Address &from)
    {
        NS_LOG_LOGIC("[VcaServer] HandleAccept");
        socket->SetRecvCallback(MakeCallback(&VcaServer::HandleRead, this));
        m_socketList_ul.push_back(socket);
    };

    void
    VcaServer::HandlePeerClose(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaServer] HandlePeerClose");
    };

    void
    VcaServer::HandlePeerError(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaServer] HandlePeerError");
    };

    void
    VcaServer::SendData(Ptr<Socket> socket, uint32_t size)
    {
        NS_LOG_LOGIC("[VcaServer] SendData");
        Ptr<Packet> packet = Create<Packet>(size);
        socket->Send(packet);
    };

}; // namespace ns3