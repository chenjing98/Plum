#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <string.h>
#include <cstdlib>
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-address.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/videoconf-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MulticastEmulation");

enum LOG_LEVEL
{
    ERROR,
    DEBUG,
    LOGIC
};

void showPosition(Ptr<Node> node, double deltaTime)
{
    uint32_t nodeId = node->GetId();
    Ptr<MobilityModel> mobModel = node->GetObject<MobilityModel>();
    Vector3D pos = mobModel->GetPosition();
    Vector3D speed = mobModel->GetVelocity();
    std::cout << "At " << Simulator::Now().GetSeconds() << " node " << nodeId << ": Position("
              << pos.x << ", " << pos.y << ", " << pos.z << ");   Speed(" << speed.x << ", "
              << speed.y << ", " << speed.z << ")" << std::endl;

    Simulator::Schedule(Seconds(deltaTime), &showPosition, node, deltaTime);
};

std::pair<WifiStandard, WifiPhyBand>
ConvertStringToStandardAndBand(std::string version)
{
    WifiStandard standard = WIFI_STANDARD_80211a;
    WifiPhyBand band = WIFI_PHY_BAND_5GHZ;
    if (version == "80211a")
    {
        standard = WIFI_STANDARD_80211a;
        band = WIFI_PHY_BAND_5GHZ;
    }
    else if (version == "80211b")
    {
        standard = WIFI_STANDARD_80211b;
        band = WIFI_PHY_BAND_2_4GHZ;
    }
    else if (version == "80211g")
    {
        standard = WIFI_STANDARD_80211g;
        band = WIFI_PHY_BAND_2_4GHZ;
    }
    else if (version == "80211p")
    {
        standard = WIFI_STANDARD_80211p;
        band = WIFI_PHY_BAND_5GHZ;
    }
    else if (version == "80211n_2_4GHZ")
    {
        standard = WIFI_STANDARD_80211n;
        band = WIFI_PHY_BAND_2_4GHZ;
    }
    else if (version == "80211n_5GHZ")
    {
        standard = WIFI_STANDARD_80211n;
        band = WIFI_PHY_BAND_5GHZ;
    }
    else if (version == "80211ac")
    {
        standard = WIFI_STANDARD_80211ac;
        band = WIFI_PHY_BAND_5GHZ;
    }
    else if (version == "80211ax_2_4GHZ")
    {
        standard = WIFI_STANDARD_80211ax;
        band = WIFI_PHY_BAND_2_4GHZ;
    }
    else if (version == "80211ax_5GHZ")
    {
        standard = WIFI_STANDARD_80211ax;
        band = WIFI_PHY_BAND_5GHZ;
    }
    return {standard, band};
}

int main(int argc, char *argv[])
{

    std::string mode = "p2p";
    uint8_t logLevel = 0;
    double_t simulationDuration = 10.0; // in s
    uint32_t maxBitrateKbps = 10000;
    uint8_t policy = 0;
    uint32_t nClient = 1;
    bool printPosition = false;
    bool savePcap = false;
    bool saveTransRate = false;
    double_t minBitrateKbps = 4.0;
    uint32_t kUlImprove = 3;
    double_t kDlYield = 0.5;
    uint32_t kLowUlThresh = 2e6;
    uint32_t kHighUlThresh = 5e6;
    bool is_tack = false;
    uint32_t tack_max_count = 32;
    int qoeType = 0;
    double dl_percentage = 0.5;

    // std::string Version = "80211n_5GHZ";

    CommandLine cmd(__FILE__);
    cmd.AddValue("mode", "p2p or sfu mode", mode);
    cmd.AddValue("logLevel", "Log level: 0 for error, 1 for debug, 2 for logic", logLevel);
    cmd.AddValue("simTime", "Total simulation time in s", simulationDuration);
    cmd.AddValue("maxBitrateKbps", "Max bitrate in kbps", maxBitrateKbps);
    cmd.AddValue("policy", "0 for Vanilla, 1 for Plum old version, 2 for Plum, 3 for Fixed", policy);
    cmd.AddValue("nClient", "Number of clients", nClient);
    cmd.AddValue("printPosition", "Print position of nodes", printPosition);
    cmd.AddValue("minBitrate", "Minimum tolerable bitrate in kbps", minBitrateKbps);
    cmd.AddValue("savePcap", "Save pcap file", savePcap);
    cmd.AddValue("ulImpv", "UL improvement param", kUlImprove);
    cmd.AddValue("dlYield", "DL yield param", kDlYield);
    cmd.AddValue("lowUlThresh", "Low UL threshold", kLowUlThresh);
    cmd.AddValue("highUlThresh", "High UL threshold", kHighUlThresh);
    cmd.AddValue("saveTransRate", "Save transmission rate", saveTransRate);
    cmd.AddValue("isTack", "Is TACK enabled", is_tack);
    cmd.AddValue("tackMaxCount", "Max TACK count", tack_max_count);
    cmd.AddValue("qoeType", "0 for lin, 1 for log, 2 for sqr_concave, 3 for sqr_convex", qoeType);
    cmd.AddValue("dlpercentage", "for policy 3(FIXED), dl_percentage", dl_percentage);

    cmd.Parse(argc, argv);
    Time::SetResolution(Time::NS);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpBbr"));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));

    if (is_tack)
    {
        Config::SetDefault("ns3::TcpSocketBase::IsTack", BooleanValue(true));
        Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(tack_max_count));
    }

    // set log level
    if (static_cast<LOG_LEVEL>(logLevel) == LOG_LEVEL::ERROR)
    {
        LogComponentEnable("VcaServer", LOG_LEVEL_ERROR);
        LogComponentEnable("VcaClient", LOG_LEVEL_ERROR);
        LogComponentEnable("MulticastEmulation", LOG_LEVEL_ERROR);
    }
    else if (static_cast<LOG_LEVEL>(logLevel) == LOG_LEVEL::DEBUG)
    {
        LogComponentEnable("VcaServer", LOG_LEVEL_DEBUG);
        LogComponentEnable("VcaClient", LOG_LEVEL_DEBUG);
        LogComponentEnable("MulticastEmulation", LOG_LEVEL_DEBUG);
    }
    else if (static_cast<LOG_LEVEL>(logLevel) == LOG_LEVEL::LOGIC)
    {
        LogComponentEnable("VcaServer", LOG_LEVEL_LOGIC);
        LogComponentEnable("VcaClient", LOG_LEVEL_LOGIC);
        LogComponentEnable("MulticastEmulation", LOG_LEVEL_LOGIC);
    }

    if (mode == "sfu")
    {
        NS_LOG_DEBUG("[Scratch] SFU mode emulation started.");

        NodeContainer p2pNodes, sfuCenter, wifiStaNodes, wifiApNode;

        p2pNodes.Create(2);
        wifiStaNodes.Create(nClient);

        sfuCenter = p2pNodes.Get(1);
        wifiApNode = p2pNodes.Get(0);

        // create p2p channel from AP to Center
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("50Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

        // install NetDevice on the p2p channel
        NetDeviceContainer backhaulDevices;
        backhaulDevices = pointToPoint.Install(wifiApNode.Get(0), sfuCenter.Get(0));
        // for(int i=0;i<nClient;i++)
        //   for(int j=0;j<nClient;j++)
        //     NS_LOG_DEBUG("dev[%d][%d]=%d\nClient",i,j,dev[i][j]);

        // channel between Wifi AP and stations
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        WifiMacHelper mac;
        Ssid ssid = Ssid("ssid");
        WifiHelper wifi;
        NetDeviceContainer staDevices;
        NetDeviceContainer apDevices;

        // const auto &[Standard, Band] = ConvertStringToStandardAndBand(Version);
        wifi.SetStandard(WIFI_STANDARD_80211p);
        phy.Set("ChannelSettings", StringValue("{0, 10, BAND_5GHZ, 0}"));
        phy.SetChannel(channel.Create());
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        staDevices = wifi.Install(phy, mac, wifiStaNodes);
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        apDevices = wifi.Install(phy, mac, wifiApNode);


        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX",
                                      DoubleValue(0.0),
                                      "MinY",
                                      DoubleValue(0.0),
                                      "DeltaX",
                                      DoubleValue(1.0),
                                      "DeltaY",
                                      DoubleValue(1.0),
                                      "GridWidth",
                                      UintegerValue(nClient),
                                      "LayoutType",
                                      StringValue("RowFirst"));
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds",
                                  RectangleValue(Rectangle(-15, 15, -15, 15)));
        // for (uint32_t i = 0; i < nClient; i++)
        // {
        //   mobility.Install(wifiStaNodes[i]);
        // }

        mobility.Install(wifiStaNodes.Get(0));

        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

        for (uint32_t i = 1; i < nClient; i++)
        {
            mobility.Install(wifiStaNodes.Get(i));
        }

        mobility.Install(wifiApNode.Get(0));
        

        InternetStackHelper stack;
        stack.Install(wifiApNode);
        for (uint32_t i = 0; i < nClient; i++)
        {

            stack.Install(wifiStaNodes.Get(i));
        }
        stack.Install(sfuCenter);

        // allocate IPv4 address for NetDevices
        Ipv4AddressHelper P2Paddress;
        Ipv4InterfaceContainer BackhaulIf;

        std::string ip = "10.1.1.0";
        P2Paddress.SetBase(ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
        BackhaulIf = P2Paddress.Assign(backhaulDevices);

        Ipv4AddressHelper Wifiaddress;
        Ipv4InterfaceContainer APinterfaces;
        Ipv4InterfaceContainer Stainterfaces;

        ip = "10.1.2.0";
        Wifiaddress.SetBase(ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
        Stainterfaces = Wifiaddress.Assign(staDevices);
        APinterfaces = Wifiaddress.Assign(apDevices);

        // print IPv4 address of each node
        for (uint32_t i = 0; i < nClient; i++)
        {
            Ptr<Ipv4> ippp = wifiApNode.Get(0)->GetObject<Ipv4>();
            int interfacenumber = ippp->GetNInterfaces();
            for (int k = 0; k < interfacenumber; k++)
            {
                Ipv4Address ipaddress = ippp->GetAddress(k, 0).GetLocal();
                NS_LOG_DEBUG("Node(" << i << ") Interface(" << k << ") IPAddress= " << ipaddress);
            }
        }

        // install VCA server and clients app
        uint16_t client_ul = 80;
        uint16_t client_dl = 8080; // dl_port may increase in VcaServer, make sure it doesn't overlap with ul_port
        uint16_t client_peer = 80;

        Ipv4Address serverAddr = BackhaulIf.GetAddress(1); // sfuCenter
        Ptr<VcaServer> vcaServerApp = CreateObject<VcaServer>();
        vcaServerApp->SetLocalAddress(serverAddr);
        vcaServerApp->SetLocalUlPort(client_peer);
        vcaServerApp->SetPeerDlPort(client_dl);
        vcaServerApp->SetLocalDlPort(client_dl);
        vcaServerApp->SetNumNode(nClient);
        vcaServerApp->SetPolicy(static_cast<POLICY>(policy));
        vcaServerApp->SetDlpercentage(dl_percentage);
        vcaServerApp->SetQoEType(static_cast<QOE_TYPE>(qoeType));
        vcaServerApp->SetNodeId(sfuCenter.Get(0)->GetId());
        sfuCenter.Get(0)->AddApplication(vcaServerApp);
        vcaServerApp->SetStartTime(Seconds(0.0));
        vcaServerApp->SetStopTime(Seconds(simulationDuration + 2));

        for (uint32_t id = 0; id < nClient; id++)
        {
            Ipv4Address staAddr = Stainterfaces.GetAddress(id);
            // Ipv4Address apAddr = BackhaulIf.GetAddress(0);

            Ipv4Address local;
            Ptr<Node> local_node;

            local = staAddr;
            local_node = wifiStaNodes.Get(id);

            Ptr<Node> node = wifiStaNodes.Get(id);
            NS_LOG_UNCOND("SFU VCA NodeId " << local_node->GetId() << " " << sfuCenter.Get(0)->GetId());
            Ptr<VcaClient> vcaClientApp = CreateObject<VcaClient>();
            vcaClientApp->SetFps(20);
            vcaClientApp->SetLocalAddress(local);
            vcaClientApp->SetPeerAddress(std::vector<Ipv4Address>{serverAddr});
            vcaClientApp->SetLocalUlPort(client_ul);
            vcaClientApp->SetLocalDlPort(client_dl);
            vcaClientApp->SetPeerPort(client_peer);
            vcaClientApp->SetNodeId(local_node->GetId());
            vcaClientApp->SetNumNode(nClient);
            vcaClientApp->SetPolicy(static_cast<POLICY>(policy));
            vcaClientApp->SetUlDlParams(kUlImprove, kDlYield);
            vcaClientApp->SetUlThresh(kLowUlThresh, kHighUlThresh);
            vcaClientApp->SetMaxBitrate(maxBitrateKbps);
            vcaClientApp->SetMinBitrate(minBitrateKbps);
            // vcaClientApp->SetLogFile("../../../evaluation/results/transient_rate_debug_" + std::to_string(nClient) + "_node_" + std::to_string(local_node->GetId()) + ".txt");
            // wifiApNode[id].Get(0)->AddApplication(vcaClientApp);
            // wifiStaNodes[id].Get(i)->AddApplication(vcaClientApp);
            local_node->AddApplication(vcaClientApp);

            Simulator::Schedule(Seconds(simulationDuration), &VcaClient::StopEncodeFrame, vcaClientApp);
            vcaClientApp->SetStartTime(Seconds(0.0));
            vcaClientApp->SetStopTime(Seconds(simulationDuration + 4));
        }

        if (savePcap)
        {
            phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
            pointToPoint.EnablePcapAll("sfu");
            phy.EnablePcap("sfu-ap", apDevices.Get(0));
            phy.EnablePcap("sfu-sta", staDevices.Get(0));
            phy.EnablePcap("sfu-ap", apDevices.Get(0));
            phy.EnablePcap("sfu-sta", staDevices.Get(0));
        }
    }

    FlowMonitorHelper flowmonHelper;
    flowmonHelper.InstallAll();

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simulationDuration + 5));
    Simulator::Run();
    flowmonHelper.SerializeToXmlFile("test-emulation.flowmon", true, true);
    Simulator::Destroy();
    return 0;
}