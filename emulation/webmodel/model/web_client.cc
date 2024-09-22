#include "web_client.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("WebClient");

    bool map_compare(const std::pair<uint8_t, uint32_t> &a, const std::pair<uint8_t, uint32_t> &b)
    {
        return a.second < b.second;
    }

    TypeId WebClient::GetTypeId()
    {
        static TypeId tid = TypeId("ns3::WebClient")
                                .SetParent<Application>()
                                .SetGroupName("videoconf")
                                .AddConstructor<WebClient>();
        return tid;
    };

    WebClient::WebClient()
        : m_socket_dl(nullptr),
          m_socket_list_ul(),
          m_socket_list_dl(),
          m_socket_id_map_ul(),
          m_socket_id_ul(0),
          m_peer_list(),
          m_tid(TypeId::LookupByName("ns3::TcpSocketFactory")),
          m_fps(20),
          m_max_bitrate(10000),
          m_frame_id(0),
          m_enc_event(),
          m_bitrateBps(),
          m_txBufSize(),
          m_lastPendingBuf(),
          m_firstUpdate(),
          m_total_packet_bit(0),
          m_send_buffer_pkt(),
          m_send_buffer_hdr(),
          m_is_my_wifi_access_bottleneck(false),
          m_policy(VANILLA),
          m_yongyule_realization(PLUM_OLD_REALIZATION::PLUM_OLD_APPRATE),
          m_target_dl_bitrate_redc_factor(1e4),
          kTxRateUpdateWindowMs(20),
          kMinEncodeBps((uint32_t)1E6),
          kMaxEncodeBps((uint32_t)10E6),
          kTargetDutyRatio(0.9),
          kDampingCoef(0.5f),
          m_total_bitrate(0),
          m_encode_times(0),
          m_increase_ul(false),
          m_probe_state(NATURAL),
          m_prev_ul_bitrate(0),
          m_prev_ul_bottleneck_bw(0),
          kUlImprove(3),
          kDlYield(0.5),
          kLowUlThresh(2e6),
          kHighUlThresh(kMaxEncodeBps),
          m_log(false),
          m_probe_cooloff_count(0),
          m_probe_cooloff_count_max(8),
          m_probe_patience_count(0),
          m_probe_patience_count_max(8),
          client_info(CreateObject<ClientInfo>()){};

    WebClient::~WebClient(){};

    void WebClient::DoDispose()
    {
        Application::DoDispose();
    };

    // Public helpers
    Ptr<Socket> WebClient::GetSocketDl(void)
    {
        return m_socket_dl;
    };

    void WebClient::SetFps(uint8_t fps)
    {
        m_fps = fps;
    };

    void WebClient::SetBitrate(std::vector<uint32_t> bitrate)
    {
        m_bitrateBps = bitrate;
    };

    void WebClient::SetMaxBitrate(uint32_t bitrate)
    {
        m_max_bitrate = bitrate;
    };

    void WebClient::SetMinBitrate(uint32_t bitrate)
    {
        m_min_bitrate = bitrate;
    };

    void WebClient::SetLocalAddress(Ipv4Address local)
    {
        m_local_ul = local;
        m_local_dl = local;
    };

    void WebClient::SetLocalAddress(Ipv4Address local_ul, Ipv4Address local_dl)
    {
        m_local_ul = local_ul;
        m_local_dl = local_dl;
    };

    void WebClient::SetPeerAddress(std::vector<Ipv4Address> peer_list)
    {
        m_peer_list = peer_list;
    };

    void WebClient::SetLocalUlPort(uint16_t port)
    {
        m_local_ul_port = port;
    };

    void WebClient::SetLocalDlPort(uint16_t port)
    {
        m_local_dl_port = port;
    };

    void WebClient::SetPeerPort(uint16_t port)
    {
        m_peer_ul_port = port;
    };

    void WebClient::SetNodeId(uint32_t node_id)
    {
        m_node_id = node_id;
    };

    void WebClient::SetNumNode(uint8_t num_nodes)
    {
        m_num_node = num_nodes;
    };

    void WebClient::SetPolicy(POLICY policy)
    {
        m_policy = policy;
    };

    void WebClient::SetLogFile(std::string log_file)
    {
        m_log = true;
        m_log_file = log_file;
    };

    void WebClient::SetUlDlParams(uint32_t ul_improve, double_t dl_yield)
    {
        kUlImprove = ul_improve;
        kDlYield = dl_yield;
    };

    void WebClient::SetUlThresh(uint32_t low_ul_thresh, uint32_t high_ul_thresh)
    {
        kLowUlThresh = low_ul_thresh;
        kHighUlThresh = high_ul_thresh;
    };

    // Application Methods
    void WebClient::StartApplication()
    {
        m_enc_event = Simulator::ScheduleNow(&WebClient::EncodeFrame, this);

        // Create the socket if not already

        for (auto peer_ip : m_peer_list)
        {
            Address peer_addr = InetSocketAddress{peer_ip, m_peer_ul_port};

            NS_LOG_LOGIC("[WebClient][" << m_node_id << "]"
                                        << " peer addr " << peer_ip
                                        << " peer port " << m_peer_ul_port);

            Ptr<Socket> socket_ul = Socket::CreateSocket(GetNode(), m_tid);

            if (socket_ul->Bind(InetSocketAddress{m_local_ul, m_local_ul_port}) == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }

            socket_ul->Connect(peer_addr);
            socket_ul->ShutdownRecv();
            socket_ul->SetConnectCallback(
                MakeCallback(&WebClient::ConnectionSucceededUl, this),
                MakeCallback(&WebClient::ConnectionFailedUl, this));
            m_socket_list_ul.push_back(socket_ul);
            m_socket_id_map_ul[socket_ul] = m_socket_id_ul;

            m_socket_id_ul += 1;
            m_local_ul_port += 1;

            m_send_buffer_pkt.push_back(std::deque<Ptr<Packet>>{});
            m_send_buffer_hdr.push_back(std::deque<Ptr<WebAppProtHeaderInfo>>{});

            UintegerValue val;
            socket_ul->GetAttribute("SndBufSize", val);
            m_txBufSize.push_back((uint32_t)val.Get());

            m_firstUpdate.push_back(true);
            m_lastPendingBuf.push_back(0);

            m_bitrateBps.push_back(m_min_bitrate * 2);
            m_dutyState.push_back(std::vector<bool>(kTxRateUpdateWindowMs, false));

            m_time_history.push_back(std::deque<int64_t>{});
            m_write_history.push_back(std::deque<uint32_t>{});
        }

        if (!m_socket_dl)
        {
            m_socket_dl = Socket::CreateSocket(GetNode(), m_tid);
            if (m_socket_dl->Bind(InetSocketAddress{m_local_dl, m_local_dl_port}) == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket_dl->Listen();
            m_socket_dl->ShutdownSend();

            NS_LOG_DEBUG("[WebClient][Node" << m_node_id << "] listening on " << m_local_dl << ":" << m_local_dl_port);

            m_socket_dl->SetRecvCallback(MakeCallback(&WebClient::HandleRead, this));
            m_socket_dl->SetRecvPktInfo(true);
            m_socket_dl->SetAcceptCallback(
                MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
                MakeCallback(&WebClient::HandleAccept, this));
            m_socket_dl->SetCloseCallbacks(
                MakeCallback(&WebClient::HandlePeerClose, this),
                MakeCallback(&WebClient::HandlePeerError, this));
        }
    };

    void WebClient::StopApplication()
    {
        StopEncodeFrame();
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
    void WebClient::ConnectionSucceededUl(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[WebClient][Node" << m_node_id << "] Uplink connection succeeded");
    };

    void WebClient::ConnectionFailedUl(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[WebClient] Connection Failed");
    };

    void WebClient::DataSendUl(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[WebClient][Node" << m_node_id << "] Uplink datasend");

        SendData(socket);
    };

    void WebClient::HandleRead2(Ptr<Packet> packet){
        if (packet->GetSize() == 0)
        { // EOF
            NS_LOG_DEBUG("[WebClient][Node" << m_node_id << "][ReceivePkt] PktSize(B)= 0");
            return;
        }

        m_total_packet_bit += packet->GetSize() * 8;

        Address from;//Todo This from : RecvFrom has the value of 'from', but Recv don't

        if (InetSocketAddress::IsMatchingType(from))
        {
            uint32_t src_ip = InetSocketAddress::ConvertFrom(from).GetIpv4().Get();

            // Statistics: transient rate

            uint32_t now_second = Simulator::Now().GetSeconds();

            while (m_transientRateBps.size() < now_second + 1)
            {
                m_transientRateBps.push_back(std::unordered_map<uint32_t, uint32_t>());
                if (m_transientRateBps.size() < now_second + 1)
                {
                    NS_LOG_DEBUG("[WebClient][Node" << m_node_id << "] ZeroRate in " << m_transientRateBps.size() - 1);
                }
            }
            // if (packet->GetSize() * 8 < m_transientRateBps[now_second] || m_transientRateBps[now_second] == 0)
            if (m_transientRateBps[now_second].find(src_ip) == m_transientRateBps[now_second].end())
            {
                m_transientRateBps[now_second][src_ip] = packet->GetSize() * 8;
            }
            else
            {
                auto latest_rate = m_transientRateBps.back();
                // if(latest_rate.size() > 0) {
                //     NS_LOG_UNCOND("[WebClient][Node" << m_node_id << "] LatestRate Time= " << m_transientRateBps.size() << " Rate= " << latest_rate[src_ip]);
                // }
                m_transientRateBps[now_second][src_ip] += packet->GetSize() * 8;
            }
            NS_LOG_LOGIC("[WebClient][Node" << m_node_id << "][ReceivedPkt] Time= " << Simulator::Now().GetMilliSeconds() << " PktSize(B)= " << packet->GetSize() << " SrcIp= " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " SrcPort= " << InetSocketAddress::ConvertFrom(from).GetPort());
            ReceiveData(packet);
        }
    }

    void WebClient::HandleRead(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[WebClient][Node" << m_node_id << "] HandleRead");
        Ptr<Packet> packet;
        // Address from;
        // // uint8_t socket_id = m_socket_id_map_dl[socket];
        // while ((packet = socket->RecvFrom(from)))
        // {
            
        // }
        
        while (true)
        {
            // start to read header
            if (client_info->read_status == 0)
            {
                packet = socket->Recv(12, false);
                if (packet == NULL)
                    return;
                if (packet->GetSize() == 0)
                    return;
                client_info->half_header = packet;
                if (client_info->half_header->GetSize() < 12)
                    client_info->read_status = 1; // continue to read header;
                if (client_info->half_header->GetSize() == 12)
                    client_info->read_status = 2; // start to read payload;
            }
            // continue to read header
            if (client_info->read_status == 1)
            {
                packet = socket->Recv(12 - client_info->half_header->GetSize(), false);
                if (packet == NULL)
                    return;
                if (packet->GetSize() == 0)
                    return;
                client_info->half_header->AddAtEnd(packet);
                if (client_info->half_header->GetSize() == 12)
                    client_info->read_status = 2; // start to read payload;
            }
            // start to read payload
            if (client_info->read_status == 2)
            {

                if (client_info->set_header == 0)
                {
                    client_info->app_header.Reset();
                    client_info->half_header->RemoveHeader(client_info->app_header);
                    client_info->payload_size = client_info->app_header.GetPayloadSize();

                    client_info->set_header = 1;
                    if (client_info->payload_size == 0)
                    {
                        // read again
                        client_info->read_status = 0;
                        client_info->set_header = 0;
                        return;
                    }
                }
                packet = socket->Recv(client_info->payload_size, false);
                if (packet == NULL)
                    return;
                if (packet->GetSize() == 0)
                    return;
                client_info->half_payload = packet;
                if (client_info->half_payload->GetSize() < client_info->payload_size)
                    client_info->read_status = 3; // continue to read payload;
                if (client_info->half_payload->GetSize() == client_info->payload_size)
                    client_info->read_status = 4; // READY TO SEND;
            }
            // continue to read payload
            if (client_info->read_status == 3)
            {
                packet = socket->Recv(client_info->payload_size - client_info->half_payload->GetSize(), false);
                if (packet == NULL)
                    return;
                if (packet->GetSize() == 0)
                    return;
                client_info->half_payload->AddAtEnd(packet);
                if (client_info->half_payload->GetSize() == client_info->payload_size)
                    client_info->read_status = 4; // READY TO SEND;
            }
            // Send packets only when header+payload is ready
            // status = 0  (1\ all empty then return    2\ all ready)
            if (client_info->read_status == 4)
            {

                uint8_t *buffer = new uint8_t[client_info->half_payload->GetSize()];               // create a buffer for store packet
                client_info->half_payload->CopyData(buffer, client_info->half_payload->GetSize()); // copy the packet to the buffer

                HandleRead2(client_info->half_payload);
                client_info->read_status = 0;
                client_info->set_header = 0;
                break;
            }
        }

        double dl_lambda = client_info->app_header.GetFrameId()+
                            client_info->app_header.GetPacketId()/10000.0;
//        NS_LOG_UNCOND("Hello!!!Lambda="<<dl_lambda);
        if (m_policy == SERVINIT) {
            EnforceDlParam(dl_lambda);
            NS_LOG_DEBUG("[WebClient][Node" << m_node_id << "] GetDlParam = " << dl_lambda);
        }
    };

    void
    WebClient::HandleAccept(Ptr<Socket> socket, const Address &from)
    {
        NS_LOG_LOGIC("[WebClient][Node" << m_node_id << "] HandleAccept");
        socket->SetRecvCallback(MakeCallback(&WebClient::HandleRead, this));
        m_socket_list_dl.push_back(socket);
        NS_LOG_DEBUG("[WebClient][Node" << m_node_id << "] HandleAccept: " << socket);
        // m_socket_id_map_dl[socket] = m_socket_id_dl;
        // m_socket_id_dl += 1;
    };

    void
    WebClient::HandlePeerClose(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[WebClient][Node" << m_node_id << "] HandlePeerClose");
    };

    void
    WebClient::HandlePeerError(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[WebClient][Node" << m_node_id << "] HandlePeerError");
    };

    // TX RX Logics
    void
    WebClient::SendData(Ptr<Socket> socket)
    {
        NS_LOG_LOGIC("[WebClient][Node" << m_node_id << "] SendData");

        uint8_t socket_id_up = m_socket_id_map_ul[socket];

        NS_LOG_LOGIC("[WebClient][SendData] SendBufSize " << m_send_buffer_pkt[socket_id_up].size());
        while (!m_send_buffer_pkt[socket_id_up].empty())
        {
            Ptr<Packet> packet = m_send_buffer_pkt[socket_id_up].front();
            Ptr<WebAppProtHeaderInfo> hdr_info = m_send_buffer_hdr[socket_id_up].front();

            // Add header
            WebAppProtHeader app_header = WebAppProtHeader(hdr_info->GetFrameId(), hdr_info->GetPacketId());
            //            NS_LOG_UNCOND("APPHEADER(hdr) Node"<<m_node_id<<" ("<<hdr_info->GetFrameId()<<","<<hdr_info->GetPacketId()<<")");
            app_header.SetPayloadSize(hdr_info->GetPayloadSize());
            app_header.SetDlRedcFactor(m_target_dl_bitrate_redc_factor);
            packet->AddHeader(app_header);

            int actual = socket->Send(packet);
            if (actual > 0)
            {
                NS_LOG_LOGIC("[WebClient][Node" << m_node_id << "][Send][Sock" << (uint16_t)socket_id_up << "] Time= " << Simulator::Now().GetMilliSeconds() << " PktSize(B)= " << packet->GetSize() << " SendBufSize= " << m_send_buffer_pkt[socket_id_up].size() - 1 << " DstIp= " << m_peer_list[socket_id_up]);

                m_send_buffer_pkt[socket_id_up].pop_front();
                m_send_buffer_hdr[socket_id_up].pop_front();

                m_time_history[socket_id_up].push_back(Simulator::Now().GetMilliSeconds());
                m_write_history[socket_id_up].push_back(actual);
            }
            else
            {
                packet->RemoveHeader(app_header);
                if (m_node_id == 0 && Simulator::Now().GetSeconds() > 148)
                    NS_LOG_DEBUG("[WebClient][Send][Node" << m_node_id << "][Sock" << (uint16_t)socket_id_up << "] SendData failed");
                break;
            }
        }
    };

    void
    WebClient::SendData()
    {
        for (auto socket : m_socket_list_ul)
        {
            SendData(socket);
        }
    };

    void
    WebClient::ReceiveData(Ptr<Packet> packet)
    {
        NS_LOG_LOGIC("[WebClient][Node" << m_node_id << "] ReceiveData");
    };

    void
    WebClient::EncodeFrame()
    {
        // Update bitrate based on current CCA
        UpdateEncodeBitrate();

        // Produce packets
        uint16_t pkt_id_in_frame;

        for (uint8_t i = 0; i < m_send_buffer_pkt.size(); i++)
        {
            // Statistics: average bitrate
            m_total_bitrate += m_bitrateBps[i];
            m_encode_times++;

            // Calculate packets in the frame
            // uint16_t num_pkt_in_frame = frame_size / payloadSize + (frame_size % payloadSize != 0);

            // Calculate frame size in bytes
            uint32_t frame_size = m_bitrateBps[i] / 8 / m_fps;
            // if (m_node_id == (uint32_t)m_num_node + 1)
            NS_LOG_DEBUG("[WebClient][Node" << m_node_id << "][EncodeFrame] Time= " << Simulator::Now().GetMilliSeconds() << " FrameId= " << m_frame_id << " BitrateMbps[" << (uint16_t)i << "]= " << m_bitrateBps[i] / 1e6 << " RedcFactor= " << m_target_dl_bitrate_redc_factor << " SendBufSize= " << m_send_buffer_pkt[i].size() << " total_goodput " << m_total_packet_bit / 1000000.);

            // if (frame_size == 0)
            //     frame_size = m_bitrateBps * 1000 / 8 / m_fps;

            pkt_id_in_frame = 0;

            for (uint32_t data_ptr = 0; data_ptr < frame_size; data_ptr += payloadSize)
            {
                Ptr<WebAppProtHeaderInfo> app_header_info = Create<WebAppProtHeaderInfo>(m_frame_id, pkt_id_in_frame);
                //                NS_LOG_UNCOND("APPHEADER Encode Node"<<m_node_id<<" ("<<m_frame_id<<","<<pkt_id_in_frame<<")");

                app_header_info->SetPayloadSize(payloadSize);

                // uint32_t packet_size = std::min(payloadSize, frame_size - data_ptr);

                uint32_t packet_size = payloadSize;

                Ptr<Packet> packet = Create<Packet>(packet_size);

                m_send_buffer_pkt[i].push_back(packet);
                m_send_buffer_hdr[i].push_back(app_header_info);

                NS_LOG_LOGIC("[WebClient][Node" << m_node_id << "][ProducePkt] Time= " << Simulator::Now().GetMilliSeconds() << " SendBufSize= " << m_send_buffer_pkt[i].size() << " PktSize= " << packet->GetSize() << " FrameId= " << m_frame_id << " PktId= " << pkt_id_in_frame);

                pkt_id_in_frame++;
            }
        }

        m_frame_id++;
        AdjustBw();

        SendData();

        // Schedule next frame's encoding
        Time next_enc_frame = MicroSeconds(1e6 / m_fps);
        m_enc_event = Simulator::Schedule(next_enc_frame, &WebClient::EncodeFrame, this);
    };

    void WebClient::UpdateDutyRatio()
    {
        uint8_t ul_id = 0;
        for (auto it = m_socket_list_ul.begin(); it != m_socket_list_ul.end(); it++)
        {
            Ptr<TcpSocketBase> tcpsocketbase = DynamicCast<TcpSocketBase, Socket>(*it);
            uint32_t sentsize = tcpsocketbase->GetTxBuffer()->GetSentSize();
            uint32_t txbuffer = (*it)->GetTxAvailable();
            uint16_t nowIndex = Simulator::Now().GetMilliSeconds() % kTxRateUpdateWindowMs;
            m_dutyState[ul_id][nowIndex] = (sentsize + txbuffer < m_txBufSize[ul_id]);

            ul_id++;
        }
        m_eventUpdateDuty = Simulator::Schedule(MilliSeconds(1), &WebClient::UpdateDutyRatio, this);
    }

    double_t
    WebClient::GetDutyRatio(uint8_t ul_id)
    {
        uint32_t count = 0;
        for (uint16_t i = 0; i < kTxRateUpdateWindowMs; i++)
        {
            if (m_dutyState[ul_id][i])
                count++;
        }
        return (double_t)count / (double_t)kTxRateUpdateWindowMs;
    }

    void
    WebClient::UpdateEncodeBitrate()
    {
        uint8_t ul_id = 0;

        for (auto it = m_socket_list_ul.begin(); it != m_socket_list_ul.end(); it++)
        {
            Ptr<TcpSocketBase> ul_socket = DynamicCast<TcpSocketBase, Socket>(*it);

            uint32_t sentsize = ul_socket->GetTxBuffer()->GetSentSize(); //
            uint32_t txbufferavailable = ul_socket->GetTxAvailable();
            uint32_t curPendingBuf = m_txBufSize[ul_id] - sentsize - txbufferavailable;
            int32_t deltaPendingBuf = (int32_t)curPendingBuf - (int32_t)m_lastPendingBuf[ul_id];

            int64_t time_now = Simulator::Now().GetMilliSeconds();
            uint32_t totalWriteBytes = 0;
            if (!m_time_history.empty())
            {
                while ((!m_time_history[ul_id].empty()) && ((time_now - m_time_history[ul_id].front()) > kTxRateUpdateWindowMs))
                {
                    m_time_history[ul_id].pop_front();
                    m_write_history[ul_id].pop_front();
                }
            }
            if (!m_write_history.empty())
            {
                totalWriteBytes = std::accumulate(m_write_history[ul_id].begin(), m_write_history[ul_id].end(), 0);
            }
            if (m_firstUpdate[ul_id])
            {
                m_bitrateBps[ul_id] = kMinEncodeBps;
                m_firstUpdate[ul_id] = false;
                m_lastPendingBuf[ul_id] = curPendingBuf;
            }
            else
            {
                double dutyRatio = GetDutyRatio(ul_id);
                uint32_t totalSendBytes = totalWriteBytes - deltaPendingBuf;
                uint32_t lastSendingRateBps = (totalWriteBytes - deltaPendingBuf) * 1000 * 8 / kTxRateUpdateWindowMs;
                if (curPendingBuf > 0)
                {
                    m_bitrateBps[ul_id] = uint32_t(kTargetDutyRatio * std::min((double)m_bitrateBps[ul_id],
                                                                               std::max(0.1, 1 - (double)curPendingBuf / totalSendBytes) * lastSendingRateBps));
                }
                else
                {
                    m_bitrateBps[ul_id] -= kDampingCoef * (dutyRatio - kTargetDutyRatio) * m_bitrateBps[ul_id];
                }

                if (m_increase_ul)
                {
                    m_bitrateBps[ul_id] = m_bitrateBps[ul_id] * kUlImprove;
                }

                m_bitrateBps[ul_id] = std::min(m_bitrateBps[ul_id], kMaxEncodeBps);
                m_bitrateBps[ul_id] = std::max(m_bitrateBps[ul_id], kMinEncodeBps);

                NS_LOG_DEBUG("[WebClient][Node" << m_node_id << "][UpdateBitrate] Time= " << Simulator::Now().GetMilliSeconds() << " Bitrate(bps) " << lastSendingRateBps << " Rtt(ms) " << (uint32_t)ul_socket->GetRtt()->GetEstimate().GetMilliSeconds() << " Cwnd(bytes) " << ul_socket->GetTcb()->m_cWnd.Get() << " pacingRate " << ((double_t)ul_socket->GetTcb()->m_pacingRate.Get().GetBitRate() / 1000000.) << " nowBuf " << curPendingBuf << " TcpCongState " << ul_socket->GetTcb()->m_congState);

                m_lastPendingBuf[ul_id] = curPendingBuf;
            }

            ul_id++;
        }
    };

    void
    WebClient::StopEncodeFrame()
    {
        if (m_enc_event.IsRunning())
        {
            Simulator::Cancel(m_enc_event);
        }
    };

    void
    WebClient::OutputStatistics()
    {
        // NS_LOG_ERROR(" ============= Output Statistics =============");

        // Calculate average_throughput
        double average_throughput;
        average_throughput = 1.0 * m_total_packet_bit / Simulator::Now().GetSeconds();
        // NS_LOG_ERROR("[WebClient][Result] Throughput= " << average_throughput << " NodeId= " << m_node_id);

        uint8_t InitPhaseFilterSec = 5;

        uint64_t pkt_history_length = m_transientRateBps.size();
        if (pkt_history_length <= InitPhaseFilterSec)
        {
            NS_LOG_ERROR("[WebClient][Node" << m_node_id << "] the stream is too short (<= " << (uint16_t)InitPhaseFilterSec << " seconds).");
            return;
        }

        while (m_transientRateBps.size() < Simulator::Now().GetSeconds() - 4)
        {
            m_transientRateBps.push_back(std::unordered_map<uint32_t, uint32_t>());
        }

        uint64_t sum_transient_rate_kbps = 0;
        uint64_t less_then_thresh_count = 0;

        std::map<uint32_t, uint64_t> transient_rate_distribution;
        // std::cout << "[Node" << m_node_id << "] Full transient rate: ";

        uint8_t init_seconds = 0;
        uint32_t transient_rate_kbps;
        uint32_t time_slot_count = 0;
        for (auto transient_rate_all_flows : m_transientRateBps)
        {
            if (init_seconds < InitPhaseFilterSec)
            {
                init_seconds++;
                continue;
            }
            if (time_slot_count > Simulator::Now().GetSeconds() - InitPhaseFilterSec)
            {
                break;
            }
            time_slot_count++;

            if (transient_rate_all_flows.empty())
                transient_rate_kbps = 0;
            else
            {
                transient_rate_kbps = std::max_element(transient_rate_all_flows.begin(), transient_rate_all_flows.end(), map_compare)->second / 1000;
            }

            sum_transient_rate_kbps += transient_rate_kbps;
            if (transient_rate_kbps < (uint32_t)150 * (m_num_node - 1))
                less_then_thresh_count++;

            auto it = transient_rate_distribution.find(transient_rate_kbps);
            if (it != transient_rate_distribution.end())
                it->second += 1;
            else
                transient_rate_distribution[transient_rate_kbps] = 1;

            if (m_log)
            {
                std::ofstream logfile(m_log_file, std::ios_base::app);
                logfile << transient_rate_kbps << "\n";
                logfile.close();
            }

            // std::cout << transient_rate_kbps << " ";
        }
        // std::cout << std::endl;

        NS_LOG_DEBUG("[WebClient][Result][Node" << m_node_id << "] TransientRateDistribution");
        for (auto transient_rate_kbps : transient_rate_distribution)
        {
            NS_LOG_DEBUG("TransientRate= " << transient_rate_kbps.first << " Count= " << transient_rate_kbps.second);
        }

        NS_LOG_ERROR("[WebClient][Result] TailThroughput= " << (double_t)less_then_thresh_count / (double_t)(m_transientRateBps.size() - InitPhaseFilterSec) << " AvgThroughput= " << /*(double_t)sum_transient_rate_kbps / (double_t)(pkt_history_length - InitPhaseFilterSec)*/ average_throughput << " NodeId= " << m_node_id);
    };

    void
    WebClient::AdjustBw()
    {
        if (m_policy == VANILLA)
        {
            return;
        }
        else if (m_policy == YONGYULE)
        {
            bool changed = 0;

            if (m_node_id != (uint32_t)m_num_node + 1)
            {
                // ignore clients who are not the host
                return;
            }

            bool is_low_ul_bitrate = IsLowRate();
            bool is_high_ul_bitrate = IsHighRate();

            // if (m_bitrateBps.size() > 0)
            // {
            //     is_low_ul_bitrate = m_bitrateBps[0] < kLowUlThresh;
            //     is_high_ul_bitrate = m_bitrateBps[0] >= kHighUlThresh;
            // }
            // else
            // {
            //     is_low_ul_bitrate = false;
            //     is_high_ul_bitrate = false;
            // }

            if (is_low_ul_bitrate)
            {
                if (m_probe_state == PROBING)
                {
                    if (ElasticTest())
                    {
                        m_probe_state = YIELD;
                        m_is_my_wifi_access_bottleneck = true;
                        double_t dl_lambda = DecideDlParam();
                        EnforceDlParam(dl_lambda);
                        NS_LOG_DEBUG("[WebClient][Node" << m_node_id << "] FSM PROBING -> YIELD");
                    }
                    else
                    {
                        if (m_probe_patience_count > 0)
                        {
                            m_probe_patience_count--;
                            return;
                        }
                        // not half-duplex bottleneck
                        m_probe_state = NATURAL;
                        m_is_my_wifi_access_bottleneck = false;
                        EnforceDlParam(1.0);
                        NS_LOG_DEBUG("[WebClient][Node" << m_node_id << "] FSM PROBING -> NATURAL");
                    }
                }
                else if (m_probe_state == NATURAL)
                {
                    // do not enter probe if has just probed recently
                    if (m_probe_cooloff_count > 0)
                    {
                        m_probe_cooloff_count--;
                        return;
                    }
                    m_probe_state = PROBING;
                    if (m_bitrateBps.size() > 0)
                    {
                        m_prev_ul_bitrate = m_bitrateBps[0];
                    }
                    m_prev_ul_bottleneck_bw = GetUlBottleneckBw();
                    m_is_my_wifi_access_bottleneck = true;
                    double_t dl_lambda = DecideDlParam();
                    EnforceDlParam(dl_lambda);
                    m_probe_cooloff_count = m_probe_cooloff_count_max;
                    m_probe_patience_count = m_probe_patience_count_max;
                    NS_LOG_DEBUG("[WebClient][Node" << m_node_id << "] FSM NATURAL -> PROBING");
                }
            }
            else
            {
                if (is_high_ul_bitrate && m_probe_state == YIELD)
                {
                    m_probe_state = NATURAL;
                    m_is_my_wifi_access_bottleneck = false;
                    EnforceDlParam(1.0);
                    NS_LOG_DEBUG("[WebClient][Node" << m_node_id << "] FSM YIELD -> NATURAL");
                }
            }

            return;

            // deprecated

            // decide to decrease rate
            if (IsBottleneck())
            {
                if (m_is_my_wifi_access_bottleneck == false)
                {
                    m_is_my_wifi_access_bottleneck = true;
                    changed = true;
                    NS_LOG_DEBUG("[WebClient][Node" << m_node_id << "] Time= " << Simulator::Now().GetMilliSeconds() << " Detected BE half-duplex bottleneck");
                }
            }
            // decide recover rate
            if (ShouldRecoverDl())
            {
                if (m_is_my_wifi_access_bottleneck == true)
                {
                    m_is_my_wifi_access_bottleneck = false;
                    changed = true;
                    NS_LOG_DEBUG("[WebClient][Node" << m_node_id << "] Time= " << Simulator::Now().GetMilliSeconds() << " Detected NOT half-duplex bottleneck");
                }
            }

            // Notice: modify lambda only when the status changes
            //   i.e., here we can't decrease lambda continuously
            if (changed)
            {
                double_t dl_lambda = DecideDlParam();

                EnforceDlParam(dl_lambda);
            }
        }
        else if (m_policy == SERVINIT)
        {
//            EnforceDlParam(dl_lambda);
//            NS_LOG_DEBUG("[WebClient][Node" << m_node_id << "] GetDlParam = " << dl_lambda);
        }
    };

    bool
    WebClient::IsBottleneck()
    {
        // return 0: capacity in enough, no congestion
        // 1: sending rate exceeds the capacity, congested

        bool is_bottleneck = false;
        for (auto it = m_socket_list_ul.begin(); it != m_socket_list_ul.end(); it++)
        {
            Ptr<TcpSocketBase> ul_socket = DynamicCast<TcpSocketBase, Socket>(*it);

            // if (ul_socket->GetTcb()->m_congState == ul_socket->GetTcb()->CA_CWR || ul_socket->GetTcb()->m_congState == ul_socket->GetTcb()->CA_LOSS || ul_socket->GetTcb()->m_congState == ul_socket->GetTcb()->CA_RECOVERY)
            // {
            //     is_bottleneck = true;
            //     break;
            // }

            Ptr<TcpBbr> bbr = DynamicCast<TcpBbr, TcpCongestionOps>(ul_socket->GetCongCtrl());

            if (bbr->GetBbrState() == 1)
            {
                is_bottleneck = true;
                break;
            }
        }

        return is_bottleneck;
    };

    bool
    WebClient::IsLowRate()
    {
        bool is_low = false;

        if (m_socket_list_ul.size() == 0)
            return false;

        Ptr<TcpSocketBase> ul_socket = DynamicCast<TcpSocketBase, Socket>(m_socket_list_ul.front());

        if(!ul_socket->GetTcb()->m_pacing) return false;

        Ptr<TcpBbr> bbr = DynamicCast<TcpBbr, TcpCongestionOps>(ul_socket->GetCongCtrl());

        if (bbr)
        {

            NS_LOG_DEBUG("[WebClient][Node" << m_node_id << "] islowrate: bbr state = "<<bbr->GetBbrState()<<" pacing= " << GetUlBottleneckBw() << " klowthresh " << kLowUlThresh);
            if (bbr->GetBbrState() != 2)
            {
                return false;
            }
        }

        if (GetUlBottleneckBw() < kLowUlThresh)
        {
            is_low = true;
        }

        return is_low;
    };

    bool
    WebClient::IsHighRate()
    {
        bool is_high = true;
        if (GetUlBottleneckBw() >= kHighUlThresh)
        {
            is_high = true;
        }

        return is_high;
    };

    bool
    WebClient::ShouldRecoverDl()
    {
        bool should_recover_dl = false;

        for (auto it = m_socket_list_ul.begin(); it != m_socket_list_ul.end(); it++)
        {
            Ptr<TcpSocketBase> ul_socket = DynamicCast<TcpSocketBase, Socket>(*it);

            if (ul_socket->GetTcb()->m_congState == ul_socket->GetTcb()->CA_OPEN)
            {
                should_recover_dl = true;
                break;
            }
        }

        return should_recover_dl;
    };

    double_t
    WebClient::DecideDlParam()
    {
        if (m_is_my_wifi_access_bottleneck == true)
        {
            NS_LOG_LOGIC("[WebClient][Node" << m_node_id << "] Time= " << Simulator::Now().GetMilliSeconds() << " Detected BE half-duplex bottleneck");

            // for (uint8_t i = 0; i < m_bitrateBps.size(); i++)
            // {
            //     m_bitrateBps[i] = (uint32_t)((double_t)m_bitrateBps[i] * 2);
            // }

            m_increase_ul = true;

            return kDlYield;
        }
        else
        {
            NS_LOG_LOGIC("[WebClient][Node" << m_node_id << "] Time= " << Simulator::Now().GetMilliSeconds() << " Detected NOT half-duplex bottleneck");
            m_increase_ul = false;
            return 1.0;
        }
    };

    void
    WebClient::EnforceDlParam(double_t dl_lambda)
    {
        if (m_yongyule_realization == PLUM_OLD_RWND)
        {
            for (auto it = m_socket_list_dl.begin(); it != m_socket_list_dl.end(); it++)
            {
                Ptr<TcpSocketBase> dl_socket = DynamicCast<TcpSocketBase, Socket>(*it);
                dl_socket->SetRwndLambda(dl_lambda);

                NS_LOG_LOGIC("[WebClient][Node" << m_node_id << "] Time= " << Simulator::Now().GetMilliSeconds() << " SetDlParam= " << dl_lambda);
            }
        }

        else if (m_yongyule_realization == PLUM_OLD_APPRATE)
        {
            m_target_dl_bitrate_redc_factor = (uint32_t)(dl_lambda * 10000.0); // divided by 10000. to be the reduced factor
        }
    };

    bool
    WebClient::ElasticTest()
    {
        // if (m_bitrateBps.size() > 0)
        // {
        //     if (m_bitrateBps[0] > m_prev_ul_bitrate * 1.2)
        //     {
        //         NS_LOG_DEBUG("[WebClient][Node" << m_node_id << "] ElasticTest currRate " << (double_t)m_bitrateBps[0] / 1000000. << " prevRate " << (double_t)m_prev_ul_bitrate / 1000000.);
        //         return true;
        //     }
        // }

        uint64_t curr_bw = GetUlBottleneckBw();
        if (curr_bw > m_prev_ul_bottleneck_bw * 1.2)
        {
            NS_LOG_DEBUG("[WebClient][Node" << m_node_id << "] ElasticTest currbw " << curr_bw << " prevbw " << m_prev_ul_bottleneck_bw);
            return true;
        }

        return false;
    };

    uint64_t
    WebClient::GetUlBottleneckBw()
    {
        uint64_t bitrate = m_prev_ul_bottleneck_bw;
        for (auto it = m_socket_list_ul.begin(); it != m_socket_list_ul.end(); it++)
        {
            Ptr<TcpSocketBase> ul_socket = DynamicCast<TcpSocketBase, Socket>(*it);

            if (ul_socket->GetTcb()->m_pacing)
            {
                bitrate = ul_socket->GetTcb()->m_pacingRate.Get().GetBitRate();

                Ptr<TcpBbr> bbr = DynamicCast<TcpBbr, TcpCongestionOps>(ul_socket->GetCongCtrl());

                if (bbr)
                {
                    double gain = 1.0;
                    if (bbr->GetBbrState() == 0)
                    { // startup
                        gain = 2.89;
                    }
                    if (bbr->GetBbrState() == 2)
                    { // probebw
                        gain = std::max(0.75, bbr->GetPacingGain());
                    }
                    // bitrate = 1.1 * bbr->m_maxBwFilter.GetBest().GetBitRate();
                    bitrate /= gain;
                }
            }
            else
            {
                double_t rtt_estimate = ul_socket->GetRtt()->GetEstimate().GetSeconds();
                if (rtt_estimate > 0)
                    bitrate = ul_socket->GetTcb()->m_cWnd * 8 / rtt_estimate;
            }
        }
        return bitrate;
    };

    double_t GetDlParamFromServ()
    {
        return 1.0;
    }

}; // namespace ns3