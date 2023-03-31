#include "vca_client.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("VcaClient");

    TypeId VcaClient::GetTypeId()
    {
        static TypeId tid = TypeId("ns3::VcaClient")
                                .SetParent<Application>()
                                .SetGroupName("videoconf")
                                .AddConstructor<VcaClient>();
        return tid;
    };

    VcaClient::VcaClient()
        : m_socket_dl(nullptr),
          m_socket_list_ul(),
          m_socket_list_dl(),
          m_socket_id_map_ul(),
          m_socket_id_ul(0),
          m_peer_list(),
          m_tid(TypeId::LookupByName("ns3::TcpSocketFactory")),
          m_fps(20),
          m_bitrate(0),
          m_max_bitrate(10000),
          m_frame_id(0),
          m_enc_event(),
          m_total_packet_bit(0),
          m_send_buffer_list(),
          m_is_my_wifi_access_bottleneck(false),
          m_policy(VANILLA),
          m_target_dl_bitrate_redc_factor(1e4){};

    VcaClient::~VcaClient(){};

    void VcaClient::DoDispose()
    {
        Application::DoDispose();
    };

    // Public helpers
    Ptr<Socket> VcaClient::GetSocketDl(void)
    {
        return m_socket_dl;
    };

    void VcaClient::SetFps(uint8_t fps)
    {
        m_fps = fps;
    };

    void VcaClient::SetBitrate(uint32_t bitrate)
    {
        m_bitrate = bitrate;
    };

    void VcaClient::SetMaxBitrate(uint32_t bitrate)
    {
        m_max_bitrate = bitrate;
    };

    void VcaClient::SetLocalAddress(Ipv4Address local)
    {
        m_local = local;
    };

    void VcaClient::SetPeerAddress(std::vector<Ipv4Address> peer_list)
    {
        m_peer_list = peer_list;
    };

    void VcaClient::SetLocalUlPort(uint16_t port)
    {
        m_local_ul_port = port;
    };

    void VcaClient::SetLocalDlPort(uint16_t port)
    {
        m_local_dl_port = port;
    };

    void VcaClient::SetPeerPort(uint16_t port)
    {
        m_peer_ul_port = port;
    };

    void VcaClient::SetNodeId(uint32_t node_id)
    {
        m_node_id = node_id;
    };

    void VcaClient::SetPolicy(POLICY policy)
    {
        m_policy = policy;
    };

    // Application Methods
    void VcaClient::StartApplication()
    {
        m_enc_event = Simulator::ScheduleNow(&VcaClient::EncodeFrame, this);

        // Create the socket if not already

        for (auto peer_ip : m_peer_list)
        {
            Address peer_addr = InetSocketAddress{peer_ip, m_peer_ul_port};

            Ptr<Socket> socket_ul = Socket::CreateSocket(GetNode(), m_tid);

            if (socket_ul->Bind(InetSocketAddress{m_local, m_local_ul_port}) == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }

            socket_ul->Connect(peer_addr);
            socket_ul->ShutdownRecv();
            socket_ul->SetConnectCallback(
                MakeCallback(&VcaClient::ConnectionSucceededUl, this),
                MakeCallback(&VcaClient::ConnectionFailedUl, this));
            socket_ul->SetSendCallback(
                MakeCallback(&VcaClient::DataSendUl, this));
            m_socket_list_ul.push_back(socket_ul);
            m_socket_id_map_ul[socket_ul] = m_socket_id_ul;

            NS_LOG_DEBUG("[VcaClient][" << m_node_id << "]"
                                        << " uplink socket " << socket_ul);

            m_socket_id_ul += 1;
            m_local_ul_port += 1;

            m_send_buffer_list.push_back(std::deque<Ptr<Packet>>{});
        }

        if (!m_socket_dl)
        {
            m_socket_dl = Socket::CreateSocket(GetNode(), m_tid);
            if (m_socket_dl->Bind(InetSocketAddress{m_local, m_local_dl_port}) == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket_dl->Listen();
            m_socket_dl->ShutdownSend();

            m_socket_dl->SetRecvCallback(MakeCallback(&VcaClient::HandleRead, this));
            m_socket_dl->SetRecvPktInfo(true);
            m_socket_dl->SetAcceptCallback(
                MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                MakeCallback(&VcaClient::HandleAccept, this));
            m_socket_dl->SetCloseCallbacks(
                MakeCallback(&VcaClient::HandlePeerClose, this),
                MakeCallback(&VcaClient::HandlePeerError, this));
        }
    };

    void VcaClient::StopApplication()
    {
        if (m_enc_event.IsRunning())
        {
            Simulator::Cancel(m_enc_event);
        }

        while (!m_socket_list_ul.empty())
        { // these are connected sockets, close them
            Ptr<Socket> connectedSocket = m_socket_list_ul.front();
            m_socket_list_ul.pop_front();
            connectedSocket->SetSendCallback(MakeNullCallback<void, Ptr<Socket>, uint32_t>());
            connectedSocket->Close();
        }

        while (!m_socket_list_dl.empty())
        { // these are accepted sockets, close them
            Ptr<Socket> acceptedSocket = m_socket_list_dl.front();
            m_socket_list_dl.pop_front();
            acceptedSocket->Close();
        }
        if (m_socket_dl)
        {
            m_socket_dl->Close();
            m_socket_dl->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        }

        OutputStatistics();
    };

    // Private helpers
    void VcaClient::ConnectionSucceededUl(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "] Uplink connection succeeded");

        SendData(socket);
    };

    void VcaClient::ConnectionFailedUl(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaClient] Connection Failed");
    };

    void VcaClient::DataSendUl(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "] Uplink datasend");

        SendData(socket);
    };

    void VcaClient::HandleRead(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "] HandleRead");
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from)))
        {
            if (packet->GetSize() == 0)
            { // EOF
                NS_LOG_DEBUG("[VcaClient][Node" << m_node_id << "][ReceivePkt] PktSize(B)= 0");
                break;
            }

            m_total_packet_bit += packet->GetSize() * 8;
            // int now_second = Simulator::Now().GetSeconds();
            // Statistics: min_packet_bit per sec
            uint32_t now_second = floor(Simulator::Now().GetSeconds()); // 0~1s -> XX[0];  1~2s -> XX[1] ...
            while (m_min_packet_bit.size() < now_second + 1)
                m_min_packet_bit.push_back(0);
            if (packet->GetSize() * 8 < m_min_packet_bit[now_second] || m_min_packet_bit[now_second] == 0)
                m_min_packet_bit[now_second] = packet->GetSize() * 8;

            if (InetSocketAddress::IsMatchingType(from))
            {
                NS_LOG_DEBUG("[VcaClient][Node" << m_node_id << "][ReceivedPkt] Time= " << Simulator::Now().GetMilliSeconds() << " PktSize(B)= " << packet->GetSize() << " SrcIp= " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " SrcPort= " << InetSocketAddress::ConvertFrom(from).GetPort());
                ReceiveData(packet);
            }
            else if (Inet6SocketAddress::IsMatchingType(from))
            {
                NS_LOG_DEBUG("[VcaClient][Node" << m_node_id << "][ReceivedPkt] Time= " << Simulator::Now().GetMilliSeconds() << " PktSize(B)= " << packet->GetSize() << " SrcIp= " << Inet6SocketAddress::ConvertFrom(from).GetIpv6() << " SrcPort= " << Inet6SocketAddress::ConvertFrom(from).GetPort());
                ReceiveData(packet);
            }
        }
    };

    void
    VcaClient::HandleAccept(Ptr<Socket> socket, const Address &from)
    {
        NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "] HandleAccept");
        socket->SetRecvCallback(MakeCallback(&VcaClient::HandleRead, this));
        m_socket_list_dl.push_back(socket);
        NS_LOG_DEBUG("[VcaClient][Node" << m_node_id << "] HandleAccept: " << socket);
        m_socket_id_map_dl[socket] = m_socket_id_dl;
        m_socket_id_dl += 1;
    };

    void
    VcaClient::HandlePeerClose(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "] HandlePeerClose");
    };

    void
    VcaClient::HandlePeerError(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "] HandlePeerError");
    };

    // TX RX Logics
    void
    VcaClient::SendData(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "] SendData");

        uint8_t socket_id_up = m_socket_id_map_ul[socket];

        NS_LOG_LOGIC("[VcaClient][SendData] SendBufSize " << m_send_buffer_list[socket_id_up].size());
        if (!m_send_buffer_list[socket_id_up].empty())
        {
            Ptr<Packet> packet = m_send_buffer_list[socket_id_up].front();
            int actual = socket->Send(packet);
            if (actual > 0)
            {
                NS_LOG_DEBUG("[VcaClient][Node" << m_node_id << "][Send][Sock" << (uint16_t)socket_id_up << "] Time= " << Simulator::Now().GetMilliSeconds() << " PktSize(B)= " << packet->GetSize() << " SendBufSize= " << m_send_buffer_list[socket_id_up].size() - 1 << " DstIp= " << m_peer_list[socket_id_up]);

                m_send_buffer_list[socket_id_up].pop_front();
            }
            else
            {
                NS_LOG_DEBUG("[VcaClient][Send][Node" << m_node_id << "][Sock" << (uint16_t)socket_id_up << "] SendData failed");
            }
        }
    };

    void
    VcaClient::ReceiveData(Ptr<Packet> packet)
    {
        NS_LOG_LOGIC("[VcaClient][Node" << m_node_id << "] ReceiveData");
    };

    void
    VcaClient::EncodeFrame()
    {
        // Update bitrate based on current CCA
        UpdateEncodeBitrate();

        // Produce packets
        uint16_t pkt_id_in_frame;

        for (uint8_t i = 0; i < m_send_buffer_list.size(); i++)
        {
            // Calculate packets in the frame
            // uint16_t num_pkt_in_frame = frame_size / payloadSize + (frame_size % payloadSize != 0);

            // Calculate frame size in bytes
            uint32_t frame_size = m_cc_rate[i] / 8 / m_fps;
            NS_LOG_DEBUG("[VcaClient][Node" << m_node_id << "][EncodeFrame] Time= " << Simulator::Now().GetMilliSeconds() << " m_bitrate= " << m_bitrate << " realtimeBitrate[" << (uint16_t)i << "]= " << m_cc_rate[i] / 1000);

            if (frame_size == 0)
                frame_size = m_bitrate * 1000 / 8 / m_fps;

            pkt_id_in_frame = 0;

            for (uint32_t data_ptr = 0; data_ptr < frame_size; data_ptr += payloadSize)
            {
                VcaAppProtHeader app_header = VcaAppProtHeader(m_frame_id, pkt_id_in_frame);
                app_header.SetDlRedcFactor(m_target_dl_bitrate_redc_factor);
                app_header.SetPayloadSize(payloadSize);

                uint32_t packet_size = std::min(payloadSize, frame_size - data_ptr);

                Ptr<Packet> packet = Create<Packet>(packet_size);
                packet->AddHeader(app_header);

                m_send_buffer_list[i].push_back(packet);

                NS_LOG_LOGIC("ICARE_C [VcaClient][Node" << m_node_id << "][ProducePkt] Time= " << Simulator::Now().GetMilliSeconds() << " SendBufSize= " << m_send_buffer_list[i].size() << " PktSize= " << packet->GetSize() <<" packet_size= " << packet_size << " FrameId= " << m_frame_id << " PktId= " << pkt_id_in_frame);

                pkt_id_in_frame++;
            }
        }

        m_frame_id++;
        DecideBottleneckPosition();

        // Schedule next frame's encoding
        Time next_enc_frame = MicroSeconds(1e6 / m_fps);
        m_enc_event = Simulator::Schedule(next_enc_frame, &VcaClient::EncodeFrame, this);
    };

    void
    VcaClient::UpdateEncodeBitrate()
    {
        m_cc_rate.clear();
        for (auto it = m_socket_list_ul.begin(); it != m_socket_list_ul.end(); it++)
        {
            Ptr<TcpSocketBase> ul_socket = DynamicCast<TcpSocketBase, Socket>(*it);

            if (ul_socket->GetTcb()->m_pacing)
            {
                uint64_t bitrate = ul_socket->GetTcb()->m_pacingRate.Get().GetBitRate();
                m_cc_rate.push_back(bitrate);
                NS_LOG_DEBUG("[VcaClient][Node" << m_node_id << "] UpdateEncodeBitrate FlowBitrate= " << bitrate / 1000);
            }
            else
            {
                // Get cc rate for non-pacing CCAs
                uint32_t cwnd = ul_socket->GetTcb()->m_cWnd.Get();
                uint32_t rwnd = ul_socket->GetRwnd();
                uint64_t bitrate = std::min(cwnd, rwnd) * 8 / 40; // 40ms is the minRTT, if link dalay is changed, this should be changed accordingly
                bitrate = (uint64_t)(1.1 * (float)bitrate);
                m_cc_rate.push_back(bitrate);
                NS_LOG_DEBUG("[VcaClient][Node" << m_node_id << "] UpdateEncodeBitrate FlowBitrate= " << bitrate / 1000 << " cwnd= " << cwnd << " rwnd= " << rwnd);
            }
        }

        // if (!m_cc_rate.empty())
        // {
        //     m_bitrate = (*std::max_element(m_cc_rate.begin(), m_cc_rate.end())) / 1000;
        //     m_bitrate = std::min(m_bitrate, m_max_bitrate);
        // }
    };

    void
    VcaClient::OutputStatistics()
    {
        // Calculate average_throughput
        double average_throughput;
        average_throughput = 1.0 * m_total_packet_bit / Simulator::Now().GetSeconds();
        NS_LOG_ERROR("[VcaClient][Statistic][Node" << m_node_id << "] TotalBit= " << m_total_packet_bit << " Throughput= " << average_throughput);

        // Calculate min packet size (per second)
        int m_length = m_min_packet_bit.size();
        if (m_length == 0)
        {
            NS_LOG_ERROR("[VcaClient][Node" << m_node_id << "] Stat Sec = 0 so there is no data.");
            return;
        }
        std::sort(m_min_packet_bit.begin(), m_min_packet_bit.end());
        uint64_t m_sum_minpac = 0;
        for (auto pac : m_min_packet_bit)
            m_sum_minpac += pac;
        // // Median
        // NS_LOG_ERROR("[VcaClient][Node" << m_node_id << "] Stat  minPacketsize [Median] = " << m_min_packet_bit[m_length / 2]);
        // // Mean
        // NS_LOG_ERROR("[VcaClient][Node" << m_node_id << "] Stat  minPacketsize [Mean] = " << m_sum_minpac / m_length);
        // // 95per
        // NS_LOG_ERROR("[VcaClient][Node" << m_node_id << "] Stat  minPacketsize [95per] = " << m_min_packet_bit[(int)(m_length * 0.95)]);

        NS_LOG_ERROR("[VcaClient][Statistic][Node" << m_node_id << "] TransientRate Median= " << m_min_packet_bit[m_length / 2] << " Avg= " << m_sum_minpac / m_length << " 95ile= " << m_min_packet_bit[(int)(m_length * 0.95)]);
    };

    float
    VcaClient::DecideDlParam()
    {
        if (m_is_my_wifi_access_bottleneck == true)
        {
            NS_LOG_DEBUG("[VcaClient][Node" << m_node_id << "] Time= " << Simulator::Now().GetMilliSeconds() << " DecideDlParam= 0.01");
            return 0.01;
        }
        else
        {
            NS_LOG_DEBUG("[VcaClient][Node" << m_node_id << "] Time= " << Simulator::Now().GetMilliSeconds() << " DecideDlParam= 1");
            return 1;
        }
    };

    void
    VcaClient::DecideBottleneckPosition()
    {
        if (m_policy == VANILLA)
        {
            return;
        }
        else if (m_policy == YONGYULE)
        {
            uint64_t m_design_rate = m_max_bitrate;
            uint64_t m_real_rate = 0;
            bool m_changed = 0;
            for (auto m_temp : m_cc_rate)
                m_real_rate += m_temp;

            // decide to decrease rate
            if (m_real_rate >= m_design_rate)
            {
                if (m_is_my_wifi_access_bottleneck == false)
                {
                    m_is_my_wifi_access_bottleneck = true;
                    m_changed = true;
                }
            }
            // decide recover rate
            if (m_real_rate < m_design_rate * 0.8)
            {
                if (m_is_my_wifi_access_bottleneck == true)
                {
                    m_is_my_wifi_access_bottleneck = false;
                    m_changed = true;
                }
            }

            // Notice: modify lambda only when the status changes
            //   i.e., here we can't decrease lambda continuously
            if (m_changed)
            {
                float dl_lambda = DecideDlParam();

                for (auto it = m_socket_list_dl.begin(); it != m_socket_list_dl.end(); it++)
                {
                    Ptr<TcpSocketBase> dl_socket = DynamicCast<TcpSocketBase, Socket>(*it);
                    dl_socket->SetRwndLambda(dl_lambda);

                    NS_LOG_DEBUG("[VcaClient][Node" << m_node_id << "] Time= " << Simulator::Now().GetMilliSeconds() << " SetDlParam= " << dl_lambda);
                }

                m_target_dl_bitrate_redc_factor = 1000; // divided by 10000. to be the reduced factor
            }
        }
    }

}; // namespace ns3