/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ssid.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/flow-monitor-helper.h"

#include "ns3/videoconf-module.h"

// Default Network Topology
//
//   Wifi 10.1.3.0
//                 AP
//  *    *    *    *
//  |    |    |    |    10.1.1.0
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
//                   point-to-point  |    |    |    |
//                                   ================
//                                     LAN 10.1.2.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiChannelEmulation");

/**
 * Convert a string (e.g., "80211a") to a pair {WifiStandard, WifiPhyBand}
 *
 * \param version The WiFi standard version.
 * \return a pair of WifiStandard, WifiPhyBand
 */
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

enum APP_TYPE
{
    APP_TYPE_BULK,
    APP_TYPE_P2P,
    APP_TYPE_SFU
};

int main(int argc, char *argv[])
{
    LogComponentEnable("VcaServer", LOG_LEVEL_ERROR);
    LogComponentEnable("VcaClient", LOG_LEVEL_DEBUG);
    bool verbose = true;
    bool tracing = false;
    uint32_t nCsma = 1;
    uint32_t nWifi = 1;
    std::string apVersion = "80211a";
    std::string staVersion = "80211n_5GHZ";
    double_t simulationDuration = 5.0; // in s
    APP_TYPE appType = APP_TYPE_P2P;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
    cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);
    cmd.AddValue("apVersion",
                 "The standard version used by the AP: 80211a, 80211b, 80211g, 80211p, "
                 "80211n_2_4GHZ, 80211n_5GHZ, 80211ac, 80211ax_2_4GHZ or 80211ax_5GHZ",
                 apVersion);
    cmd.AddValue("staVersion",
                 "The standard version used by the station: 80211a, 80211b, 80211g, 80211_10MHZ, "
                 "80211_5MHZ, 80211n_2_4GHZ, 80211n_5GHZ, 80211ac, 80211ax_2_4GHZ or 80211ax_5GHZ",
                 staVersion);
    cmd.AddValue("simTime", "Total simulation time in s", simulationDuration);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);

    // The underlying restriction of 18 is due to the grid position
    // allocator's configuration; the grid layout will exceed the
    // bounding box if more than 18 nodes are provided.
    if (nWifi > 18)
    {
        std::cout << "nWifi should be 18 or less; otherwise grid layout exceeds the bounding box"
                  << std::endl;
        return 1;
    }

    if (verbose)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer p2pNodes;
    p2pNodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer p2pDevices;
    p2pDevices = pointToPoint.Install(p2pNodes);

    NodeContainer csmaNodes;
    csmaNodes.Add(p2pNodes.Get(1));
    csmaNodes.Create(nCsma);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("1Gbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi);
    NodeContainer wifiApNode = p2pNodes.Get(0);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    WifiHelper wifi;
    const auto &[staStandard, staBand] = ConvertStringToStandardAndBand(staVersion);
    wifi.SetStandard(staStandard);

    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    const auto &[apStandard, apBand] = ConvertStringToStandardAndBand(apVersion);
    wifi.SetStandard(apStandard);
    NetDeviceContainer apDevices;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    apDevices = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(5.0),
                                  "DeltaY",
                                  DoubleValue(10.0),
                                  "GridWidth",
                                  UintegerValue(3),
                                  "LayoutType",
                                  StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds",
                              RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wifiStaNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(csmaNodes);
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces;
    p2pInterfaces = address.Assign(p2pDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces;
    csmaInterfaces = address.Assign(csmaDevices);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiStaInterfaces, wifiApInterfaces;
    wifiStaInterfaces = address.Assign(staDevices);
    wifiApInterfaces = address.Assign(apDevices);

    uint16_t port_ul = 8080;
    uint16_t port_dl = 9090;
    Ipv4Address staAddr = wifiStaInterfaces.GetAddress(0);
    Ipv4Address csmaAddr = csmaInterfaces.GetAddress(1);

    // Print dev address
    NS_LOG_UNCOND("[NodeDev] STA NodeId= " << wifiStaNodes.Get(0)->GetId() << " Addr= " << wifiStaInterfaces.GetAddress(0));
    NS_LOG_UNCOND("[NodeDev] AP NodeId= " << wifiApNode.Get(0)->GetId() << " Addr= " << wifiApInterfaces.GetAddress(0) << " Addr= " << p2pInterfaces.GetAddress(0));
    NS_LOG_UNCOND("[NodeDev] CSMA1 NodeId= " << p2pNodes.Get(1)->GetId() << " Addr= " << p2pInterfaces.GetAddress(1) << " Addr= " << csmaInterfaces.GetAddress(0));
    NS_LOG_UNCOND("[NodeDev] CSMA2 NodeId= " << csmaNodes.Get(0)->GetId() << " Addr= " << csmaInterfaces.GetAddress(1));

    // Application

    if (appType == APP_TYPE_BULK)
    {
        BulkSendHelper ulBulkSender("ns3::TcpSocketFactory", InetSocketAddress{csmaAddr, port_ul});
        ulBulkSender.SetAttribute("Local", AddressValue(InetSocketAddress{staAddr, port_ul}));
        ulBulkSender.SetAttribute("SendSize", UintegerValue(1448));
        ApplicationContainer ulApps = ulBulkSender.Install(wifiStaNodes.Get(nWifi - 1));
        ulApps.Start(Seconds(0.0));
        ulApps.Stop(Seconds(simulationDuration - 2));
        PacketSinkHelper ulSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port_ul));
        ApplicationContainer ulSinkApps = ulSink.Install(csmaNodes.Get(nCsma));
        ulSinkApps.Start(Seconds(0.0));
        ulSinkApps.Stop(Seconds(simulationDuration));

        BulkSendHelper dlBulkSender("ns3::TcpSocketFactory", InetSocketAddress{staAddr, port_dl});
        dlBulkSender.SetAttribute("Local", AddressValue(InetSocketAddress{csmaAddr, port_dl}));
        dlBulkSender.SetAttribute("SendSize", UintegerValue(1448));
        ApplicationContainer dlApps = dlBulkSender.Install(csmaNodes.Get(nCsma));
        dlApps.Start(Seconds(0.0));
        dlApps.Stop(Seconds(simulationDuration - 2));
        PacketSinkHelper dlSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port_dl));
        ApplicationContainer dlSinkApps = dlSink.Install(wifiStaNodes.Get(nWifi - 1));
        dlSinkApps.Start(Seconds(0.0));
        dlSinkApps.Stop(Seconds(simulationDuration));
    }
    else if (appType == APP_TYPE_P2P)
    {
        NS_LOG_UNCOND("P2P VCA NodeId " << wifiStaNodes.Get(nWifi - 1)->GetId() << " " << csmaNodes.Get(nCsma)->GetId());
        Ptr<VcaClient> vcaClientAppLeft = CreateObject<VcaClient>();
        vcaClientAppLeft->SetFps(30);
        vcaClientAppLeft->SetBitrate(1000);
        vcaClientAppLeft->SetLocalAddress(staAddr);
        vcaClientAppLeft->SetPeerAddress(InetSocketAddress{csmaAddr, port_dl});
        vcaClientAppLeft->SetLocalUlPort(port_ul);
        vcaClientAppLeft->SetLocalDlPort(port_dl);
        vcaClientAppLeft->SetNodeId(wifiStaNodes.Get(nWifi - 1)->GetId());
        wifiStaNodes.Get(nWifi - 1)->AddApplication(vcaClientAppLeft);

        Ptr<VcaClient> vcaClientAppRight = CreateObject<VcaClient>();
        vcaClientAppRight->SetFps(30);
        vcaClientAppRight->SetBitrate(1000);
        vcaClientAppRight->SetLocalAddress(csmaAddr);
        vcaClientAppRight->SetPeerAddress(InetSocketAddress{staAddr, port_dl});
        vcaClientAppRight->SetLocalUlPort(port_ul);
        vcaClientAppRight->SetLocalDlPort(port_dl);
        vcaClientAppRight->SetNodeId(csmaNodes.Get(nCsma)->GetId());
        csmaNodes.Get(nCsma)->AddApplication(vcaClientAppRight);
    }
    else if (appType == APP_TYPE_SFU)
    {
    }
    else
    {
        NS_LOG_DEBUG("[Simulation] Unsupported application type");
    }

    // Flow Monitor
    FlowMonitorHelper flowmonHelper;
    flowmonHelper.InstallAll();

    // UdpEchoServer application
    // UdpEchoServerHelper echoServer(9);

    // ApplicationContainer serverApps = echoServer.Install(csmaNodes.Get(nCsma));
    // serverApps.Start(Seconds(1.0));
    // serverApps.Stop(Seconds(10.0));

    // UdpEchoClientHelper echoClient(csmaInterfaces.GetAddress(nCsma), 9);
    // echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    // echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    // echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    // ApplicationContainer clientApps = echoClient.Install(wifiStaNodes.Get(nWifi - 1));
    // clientApps.Start(Seconds(2.0));
    // clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simulationDuration));

    if (tracing)
    {
        phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        pointToPoint.EnablePcapAll("wifichannelemu");
        phy.EnablePcap("wifichannelemu", apDevices.Get(0));
        csma.EnablePcap("wifichannelemu", csmaDevices.Get(0), true);
    }

    Simulator::Run();

    flowmonHelper.SerializeToXmlFile("wifi-channel-emulation.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}