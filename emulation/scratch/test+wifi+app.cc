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
  double_t minBitrateKbps = 4.0;
  
  std::string Version = "80211n_5GHZ";

  CommandLine cmd(__FILE__);
  cmd.AddValue("mode", "p2p or sfu mode", mode);
  cmd.AddValue("logLevel", "Log level: 0 for error, 1 for debug, 2 for logic", logLevel);
  cmd.AddValue("simTime", "Total simulation time in s", simulationDuration);
  cmd.AddValue("maxBitrateKbps", "Max bitrate in kbps", maxBitrateKbps);
  cmd.AddValue("policy", "0 for vanilla, 1 for Yongyule", policy);
  cmd.AddValue("nClient", "Number of clients", nClient);
  cmd.AddValue("printPosition", "Print position of nodes", printPosition);
  cmd.AddValue("minBitrate", "Minimum tolerable bitrate in kbps", minBitrateKbps);

  cmd.Parse(argc, argv);
  Time::SetResolution(Time::NS);

//  Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpBbr"));

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

  if (mode == "p2p")
  {
    NS_LOG_ERROR("[Scratch] P2P mode emulation started.");
    // 创建节点
    uint8_t nWifi[nClient];                     // 每个AP管多少Nodes
    uint32_t num = (nClient - 1) * nClient / 2; // AP之间的信道数量
    NodeContainer p2pNodes, wifiStaNodes[nClient], wifiApNode[nClient];
    for (uint32_t i = 0; i < nClient; i++)
      nWifi[i] = 1;
    p2pNodes.Create(nClient);
    for (uint32_t i = 0; i < nClient; i++)
    {
      wifiStaNodes[i].Create(nWifi[i]);
      wifiApNode[i] = p2pNodes.Get(i);
    }

    // 创建AP之间的信道
    PointToPointHelper pointToPoint[num];
    for (uint32_t i = 0; i < num; i++)
    {
      pointToPoint[i].SetDeviceAttribute("DataRate", StringValue("100Mbps"));
      pointToPoint[i].SetChannelAttribute("Delay", StringValue("20ms"));
    }

    // 在AP的p2p信道上安装NetDevice
    NetDeviceContainer p2pDevices[num];
    uint32_t p = 0;
    //    int dev[100][100];
    for (uint32_t i = 0; i < nClient; i++)
      for (uint32_t j = i + 1; j < nClient; j++)
      {
        p2pDevices[p] = pointToPoint[p].Install(p2pNodes.Get(i), p2pNodes.Get(j));
        //        dev[i][j] = p;
        //        dev[j][i] = p;
        p++;
      }
    // for(int i=0;i<nClient;i++)
    //   for(int j=0;j<nClient;j++)
    //     NS_LOG_DEBUG("dev[%d][%d]=%d\nClient",i,j,dev[i][j]);

    // 抄一些WifiAP和WifiNodes之间建立联系的代码
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    WifiMacHelper mac;
    Ssid ssid;
    WifiHelper wifi;
    NetDeviceContainer staDevices[nClient];
    NetDeviceContainer apDevices[nClient];

    // 每次的SSID、PHY要不同
    for (uint32_t i = 0; i < nClient; i++)
    {
      std::string id = "ssid" + std::to_string(i + 1);
      //      std::cout<<id<<std::endl;
      ssid = Ssid(id);
      //      std::cout<<ssid<<std::endl;
      phy.SetChannel(channel.Create());

      mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
      staDevices[i] = wifi.Install(phy, mac, wifiStaNodes[i]);
      mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
      apDevices[i] = wifi.Install(phy, mac, wifiApNode[i]);
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(100.0),
                                  "DeltaY",
                                  DoubleValue(2.0),
                                  "GridWidth",
                                  UintegerValue(3),
                                  "LayoutType",
                                  StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds",
                              RectangleValue(Rectangle(-50, 50, 0, 50)));
    for (uint32_t i = 0; i < nClient; i++)
    {
      mobility.Install(wifiStaNodes[i]);
    }

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    for (uint32_t i = 0; i < nClient; i++)
    {
      mobility.Install(wifiApNode[i]);
    }

    InternetStackHelper stack;
    for (uint32_t i = 0; i < nClient; i++)
      stack.Install(wifiApNode[i]);
    for (uint32_t i = 0; i < nClient; i++)
      stack.Install(wifiStaNodes[i]);

    // 给NetDevices分配IPv4地址
    Ipv4AddressHelper P2Paddress[num];
    Ipv4InterfaceContainer interfaces[num];
    for (uint32_t i = 0; i < num; i++)
    {
      std::string ip = "10.1." + std::to_string(i + 1) + ".0";
      P2Paddress[i].SetBase(ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
      interfaces[i] = P2Paddress[i].Assign(p2pDevices[i]);
    }
    Ipv4AddressHelper Wifiaddress[nClient];
    Ipv4InterfaceContainer APinterfaces[nClient];
    Ipv4InterfaceContainer Stainterfaces[nClient];
    for (uint32_t i = 0; i < nClient; i++)
    {
      std::string ip = "10.1." + std::to_string(i + 1 + num) + ".0";
      //      std::cout<<"ip = "<<ip<<std::endl;
      Wifiaddress[i].SetBase(ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
      Stainterfaces[i] = Wifiaddress[i].Assign(staDevices[i]);
      APinterfaces[i] = Wifiaddress[i].Assign(apDevices[i]);
      NS_LOG_DEBUG("APinterfaces[" << i << "].getAddress(0) = " << APinterfaces[i].GetAddress(0));
    }

    // 给AP的每个User装上Client
    std::vector<Ipv4Address> staAddr_all;
    for (uint32_t id = 0; id < nClient; id++)
    {
      for (uint8_t i = 0; i < nWifi[id]; i++)
      {
        Ipv4Address staAddr = Stainterfaces[id].GetAddress(i);
        staAddr_all.push_back(staAddr);
      }
    }

    uint16_t port_ul = 81;
    uint16_t port_dl = 80;
    for (uint32_t id = 0; id < nClient; id++)
    {
      for (uint8_t i = 0; i < nWifi[id]; i++)
      {
        Ipv4Address staAddr = Stainterfaces[id].GetAddress(i);
        //        Ipv4Address APAddr = APinterfaces[id].GetAddress(0);
        std::vector<Ipv4Address> peerAddr;
        for (auto item : staAddr_all)
          if (staAddr != item)
            peerAddr.push_back(item);
        for (auto item : peerAddr)
          NS_LOG_DEBUG("item:" << item);

        Ptr<VcaClient> vcaClientApp = CreateObject<VcaClient>();
        vcaClientApp->SetFps(20);
        vcaClientApp->SetLocalAddress(staAddr);
        vcaClientApp->SetPeerAddress(peerAddr);
        vcaClientApp->SetLocalUlPort(port_ul);
        vcaClientApp->SetLocalDlPort(port_dl);
        vcaClientApp->SetPeerPort(port_dl); // to other's port_dl
        vcaClientApp->SetNodeId(wifiStaNodes[id].Get(i)->GetId());
        vcaClientApp->SetMaxBitrate(maxBitrateKbps);
        wifiStaNodes[id].Get(i)->AddApplication(vcaClientApp);
        NS_LOG_DEBUG("[id=" << id << ",i=" << i << "] info:" << wifiStaNodes[id].Get(i)->GetId());

        vcaClientApp->SetStartTime(Seconds(0.0));
        vcaClientApp->SetStopTime(Seconds(simulationDuration));
      }
    }
  }

  else if (mode == "sfu")
  {
    NS_LOG_DEBUG("[Scratch] SFU mode emulation started.");

    double_t showPositionDeltaTime = 1; // in s
    // 创建节点
    // nClient of VcaClient, each in a different BSS
    NodeContainer p2pNodes, sfuCenter, wifiStaNodes[nClient], wifiApNode[nClient];
    uint8_t nWifi[nClient]; // number of stations in each BSS
    for (uint32_t i = 0; i < nClient; i++)
      nWifi[i] = 1;
    p2pNodes.Create(nClient + 1);
    for (uint8_t i = 0; i < nClient; i++)
    {
      wifiStaNodes[i].Create(nWifi[i]);
      wifiApNode[i] = p2pNodes.Get(i);
      if (printPosition)
      {
        Simulator::Schedule(Seconds(0.0), &showPosition, wifiStaNodes[i].Get(0), showPositionDeltaTime);
        Simulator::Schedule(Seconds(0.0), &showPosition, wifiApNode[i].Get(0), showPositionDeltaTime);
      }
    }
    sfuCenter = p2pNodes.Get(nClient);

    // 创建AP到Center之间的信道
    PointToPointHelper pointToPoint[nClient];
    for (uint32_t i = 0; i < nClient; i++)
    {
      pointToPoint[i].SetDeviceAttribute("DataRate", StringValue("100Mbps"));
      pointToPoint[i].SetChannelAttribute("Delay", StringValue("10ms"));
    }

    // 在AP的p2p信道上安装NetDevice
    NetDeviceContainer sfuDevices[nClient];
    for (uint32_t i = 0; i < nClient; i++)
      sfuDevices[i] = pointToPoint[i].Install(wifiApNode[i].Get(0), sfuCenter.Get(0));
    // for(int i=0;i<nClient;i++)
    //   for(int j=0;j<nClient;j++)
    //     NS_LOG_DEBUG("dev[%d][%d]=%d\nClient",i,j,dev[i][j]);

    // Wifi AP和stations之间建立channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    WifiMacHelper mac;
    Ssid ssid;
    WifiHelper wifi;
    NetDeviceContainer staDevices[nClient];
    NetDeviceContainer apDevices[nClient];

    const auto &[Standard, Band] = ConvertStringToStandardAndBand(Version);
    wifi.SetStandard(Standard);

    // Set different SSID (+ PHY channel) for each BSS
    for (uint32_t i = 0; i < nClient; i++)
    {
      std::string id = "ssid" + std::to_string(i + 1);
      ssid = Ssid(id);
      NS_LOG_DEBUG("[Scratch] Ssid= " << ssid);
      phy.SetChannel(channel.Create());

      mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
      staDevices[i] = wifi.Install(phy, mac, wifiStaNodes[i]);
      mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
      apDevices[i] = wifi.Install(phy, mac, wifiApNode[i]);
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(100.0),
                                  "DeltaY",
                                  DoubleValue(1.0),
                                  "GridWidth",
                                  UintegerValue(nClient),
                                  "LayoutType",
                                  StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds",
                              RectangleValue(Rectangle(-30, 30, -30, 30)));
    // for (uint32_t i = 0; i < nClient; i++)
    // {
    //   mobility.Install(wifiStaNodes[i]);
    // }

    mobility.Install(wifiStaNodes[0]);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    for (uint32_t i = 1; i < nClient; i++)
    {
      mobility.Install(wifiStaNodes[i]);
    }

    for (uint32_t i = 0; i < nClient; i++)
    {
      mobility.Install(wifiApNode[i]);
    }

    InternetStackHelper stack;
    for (uint32_t i = 0; i < nClient; i++)
    {
      stack.Install(wifiApNode[i]);
      stack.Install(wifiStaNodes[i]);
    }
    stack.Install(sfuCenter);

    // 给NetDevices分配IPv4地址
    Ipv4AddressHelper P2Paddress[nClient];
    Ipv4InterfaceContainer interfaces[nClient];
    for (uint32_t i = 0; i < nClient; i++)
    {
      std::string ip = "10.1." + std::to_string(i + 1) + ".0";
      P2Paddress[i].SetBase(ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
      interfaces[i] = P2Paddress[i].Assign(sfuDevices[i]);
    }
    Ipv4AddressHelper Wifiaddress[nClient];
    Ipv4InterfaceContainer APinterfaces[nClient];
    Ipv4InterfaceContainer Stainterfaces[nClient];
    for (uint32_t i = 0; i < nClient; i++)
    {
      std::string ip = "10.1." + std::to_string(i + 1 + nClient) + ".0";
      Wifiaddress[i].SetBase(ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
      Stainterfaces[i] = Wifiaddress[i].Assign(staDevices[i]);
      APinterfaces[i] = Wifiaddress[i].Assign(apDevices[i]);
      NS_LOG_DEBUG("APinterfaces[" << i << "].getAddress(0) = " << APinterfaces[i].GetAddress(0));
    }

    // 把每个node的每个接口的ip地址打印出来
    for (uint32_t i = 0; i < nClient; i++)
    {
      Ptr<Ipv4> ippp = wifiApNode[i].Get(0)->GetObject<Ipv4>();
      int interfacenumber = ippp->GetNInterfaces();
      for (int k = 0; k < interfacenumber; k++)
      {
        Ipv4Address ipaddress = ippp->GetAddress(k, 0).GetLocal();
        NS_LOG_DEBUG("Node(" << i << ") Interface(" << k << ") IPAddress= " << ipaddress);
      }
    }

    // 给每个user(WifiSta)装上VcaClient，给Center装上VcaServer
    uint16_t client_ul = 80;
    uint16_t client_dl = 8080; // dl_port may increase in VcaServer, make sure it doesn't overlap with ul_port
    uint16_t client_peer = 80;

    Ipv4Address serverAddr = interfaces[0].GetAddress(1); // sfuCenter
    Ptr<VcaServer> vcaServerApp = CreateObject<VcaServer>();
    vcaServerApp->SetLocalAddress(serverAddr);
    vcaServerApp->SetLocalUlPort(client_peer);
    vcaServerApp->SetPeerDlPort(client_dl);
    vcaServerApp->SetLocalDlPort(client_dl);
    vcaServerApp->SetNodeId(sfuCenter.Get(0)->GetId());
    sfuCenter.Get(0)->AddApplication(vcaServerApp);
    vcaServerApp->SetStartTime(Seconds(0.0));
    vcaServerApp->SetStopTime(Seconds(simulationDuration + 2));

    for (uint32_t id = 0; id < nClient; id++)
    {

      for (uint8_t i = 0; i < nWifi[id]; i++)
      {
        Ipv4Address staAddr = Stainterfaces[id].GetAddress(i);
        NS_LOG_DEBUG("SFU VCA NodeId " << wifiStaNodes[id].Get(i)->GetId() << " " << sfuCenter.Get(0)->GetId());
        Ptr<VcaClient> vcaClientApp = CreateObject<VcaClient>();
        vcaClientApp->SetFps(20);
        vcaClientApp->SetLocalAddress(staAddr);
        vcaClientApp->SetPeerAddress(std::vector<Ipv4Address>{serverAddr});
        vcaClientApp->SetLocalUlPort(client_ul);
        vcaClientApp->SetLocalDlPort(client_dl);
        vcaClientApp->SetPeerPort(client_peer);
        vcaClientApp->SetNodeId(wifiStaNodes[id].Get(i)->GetId());
        vcaClientApp->SetPolicy(static_cast<POLICY>(policy));
        vcaClientApp->SetMaxBitrate(maxBitrateKbps);
        vcaClientApp->SetMinBitrate(minBitrateKbps);
        wifiStaNodes[id].Get(i)->AddApplication(vcaClientApp);

        Simulator::Schedule(Seconds(simulationDuration), &VcaClient::StopEncodeFrame, vcaClientApp);
        vcaClientApp->SetStartTime(Seconds(0.0));
        vcaClientApp->SetStopTime(Seconds(simulationDuration + 4));
      }
    }

    int tracing = 1;
    if (tracing)
    {
      phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
      pointToPoint[0].EnablePcapAll("sfu");
      phy.EnablePcap("sfu-ap", apDevices[0].Get(0));
      phy.EnablePcap("sfu-sta", staDevices[0].Get(0));
      phy.EnablePcap("sfu-ap", apDevices[2].Get(0));
      phy.EnablePcap("sfu-sta", staDevices[2].Get(0));
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