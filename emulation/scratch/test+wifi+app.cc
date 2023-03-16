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

int main(int argc, char *argv[])
{

  std::string mode = "p2p";
  uint8_t logLevel = 0;
  double_t simulationDuration = 20.0; // in s
  uint32_t maxBitrate = 10000;        // in kbps

  CommandLine cmd(__FILE__);
  cmd.AddValue("mode", "p2p or sfu mode", mode);
  cmd.AddValue("logLevel", "Log level: 0 for error, 1 for debug, 2 for logic", logLevel);
  cmd.AddValue("simTime", "Total simulation time in s", simulationDuration);
  cmd.AddValue("maxBitrate", "Max bitrate in kbps", maxBitrate);

  cmd.Parse(argc, argv);
  Time::SetResolution(Time::NS);

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
    int n = 2;                 // AP的数量
    int nWifi[n];              // 每个AP管多少Nodes
    int num = (n - 1) * n / 2; // AP之间的信道数量
    NodeContainer p2pNodes, wifiStaNodes[n], wifiApNode[n];
    for (int i = 0; i < n; i++)
      nWifi[i] = 1;
    p2pNodes.Create(n);
    for (int i = 0; i < n; i++)
    {
      wifiStaNodes[i].Create(nWifi[i]);
      wifiApNode[i] = p2pNodes.Get(i);
    }

    // 创建AP之间的信道
    PointToPointHelper pointToPoint[num];
    for (int i = 0; i < num; i++)
    {
      pointToPoint[i].SetDeviceAttribute("DataRate", StringValue("5Mbps"));
      pointToPoint[i].SetChannelAttribute("Delay", StringValue("2ms"));
    }

    // 在AP的p2p信道上安装NetDevice
    NetDeviceContainer p2pDevices[num];
    int p = 0;
    //    int dev[100][100];
    for (int i = 0; i < n; i++)
      for (int j = i + 1; j < n; j++)
      {
        p2pDevices[p] = pointToPoint[p].Install(p2pNodes.Get(i), p2pNodes.Get(j));
        //        dev[i][j] = p;
        //        dev[j][i] = p;
        p++;
      }
    // for(int i=0;i<n;i++)
    //   for(int j=0;j<n;j++)
    //     NS_LOG_DEBUG("dev[%d][%d]=%d\n",i,j,dev[i][j]);

    // 抄一些WifiAP和WifiNodes之间建立联系的代码
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    WifiMacHelper mac;
    Ssid ssid;
    WifiHelper wifi;
    NetDeviceContainer staDevices[n];
    NetDeviceContainer apDevices[n];

    // 每次的SSID、PHY要不同
    for (int i = 0; i < n; i++)
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
    for (int i = 0; i < n; i++)
    {
      mobility.Install(wifiStaNodes[i]);
    }

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    for (int i = 0; i < n; i++)
    {
      mobility.Install(wifiApNode[i]);
    }

    InternetStackHelper stack;
    for (int i = 0; i < n; i++)
      stack.Install(wifiApNode[i]);
    for (int i = 0; i < n; i++)
      stack.Install(wifiStaNodes[i]);

    // 给NetDevices分配IPv4地址
    Ipv4AddressHelper P2Paddress[num];
    Ipv4InterfaceContainer interfaces[num];
    for (int i = 0; i < num; i++)
    {
      std::string ip = "10.1." + std::to_string(i + 1) + ".0";
      P2Paddress[i].SetBase(ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
      interfaces[i] = P2Paddress[i].Assign(p2pDevices[i]);
    }
    Ipv4AddressHelper Wifiaddress[n];
    Ipv4InterfaceContainer APinterfaces[n];
    Ipv4InterfaceContainer Stainterfaces[n];
    for (int i = 0; i < n; i++)
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
    for (int id = 0; id < n; id++)
    {
      for (int i = 0; i < nWifi[id]; i++)
      {
        Ipv4Address staAddr = Stainterfaces[id].GetAddress(i);
        staAddr_all.push_back(staAddr);
      }
    }

    uint16_t port_ul = 81;
    uint16_t port_dl = 80;
    for (int id = 0; id < n; id++)
    {
      for (int i = 0; i < nWifi[id]; i++)
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
        vcaClientApp->SetFps(30);
        vcaClientApp->SetBitrate(1000);
        vcaClientApp->SetLocalAddress(staAddr);
        vcaClientApp->SetPeerAddress(peerAddr);
        vcaClientApp->SetLocalUlPort(port_ul);
        vcaClientApp->SetLocalDlPort(port_dl);
        vcaClientApp->SetPeerPort(port_dl); // to other's port_dl
        vcaClientApp->SetNodeId(wifiStaNodes[id].Get(i)->GetId());
        vcaClientApp->SetMaxBitrate(maxBitrate);
        wifiStaNodes[id].Get(i)->AddApplication(vcaClientApp);
        NS_LOG_DEBUG("[id=" << id << ",i=" << i << "] info:" << wifiStaNodes[id].Get(i)->GetId());

        vcaClientApp->SetStartTime(Seconds(0.0));
        vcaClientApp->SetStopTime(Seconds(simulationDuration));
      }
    }

    NS_LOG_DEBUG("Successfully Done!!!! [P2P MODE] \n");
  }

  else if (mode == "sfu")
  {
    NS_LOG_ERROR("[Scratch] P2P mode emulation started.");
    // 创建节点
    int n = 2;    // AP的数量
    int nWifi[n]; // 每个AP管多少Nodes
    NodeContainer p2pNodes, sfuCenter, wifiStaNodes[n], wifiApNode[n];
    for (int i = 0; i < n; i++)
      nWifi[i] = 1;
    p2pNodes.Create(n + 1);
    for (int i = 0; i < n; i++)
    {
      wifiStaNodes[i].Create(nWifi[i]);
      wifiApNode[i] = p2pNodes.Get(i);
    }
    sfuCenter = p2pNodes.Get(n);

    // 创建AP到Center之间的信道
    PointToPointHelper pointToPoint[n];
    for (int i = 0; i < n; i++)
    {
      pointToPoint[i].SetDeviceAttribute("DataRate", StringValue("10Mbps"));
      pointToPoint[i].SetChannelAttribute("Delay", StringValue("20ms"));
    }

    // 在AP的p2p信道上安装NetDevice
    NetDeviceContainer sfuDevices[n];
    for (int i = 0; i < n; i++)
      sfuDevices[i] = pointToPoint[i].Install(wifiApNode[i].Get(0), sfuCenter.Get(0));
    // for(int i=0;i<n;i++)
    //   for(int j=0;j<n;j++)
    //     NS_LOG_DEBUG("dev[%d][%d]=%d\n",i,j,dev[i][j]);

    // 抄一些WifiAP和WifiNodes之间建立联系的代码
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    WifiMacHelper mac;
    Ssid ssid;
    WifiHelper wifi;
    NetDeviceContainer staDevices[n];
    NetDeviceContainer apDevices[n];

    // 每次的SSID、PHY要不同
    for (int i = 0; i < n; i++)
    {
      std::string id = "ssid" + std::to_string(i + 1);
      //      std::cout<<id<<std::endl;
      ssid = Ssid(id);
      NS_LOG_DEBUG(ssid);
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
    for (int i = 0; i < n; i++)
    {
      mobility.Install(wifiStaNodes[i]);
    }

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    for (int i = 0; i < n; i++)
    {
      mobility.Install(wifiApNode[i]);
    }

    InternetStackHelper stack;
    for (int i = 0; i < n; i++)
      stack.Install(wifiApNode[i]);
    for (int i = 0; i < n; i++)
      stack.Install(wifiStaNodes[i]);
    stack.Install(sfuCenter);

    // 给NetDevices分配IPv4地址
    Ipv4AddressHelper P2Paddress[n];
    Ipv4InterfaceContainer interfaces[n];
    for (int i = 0; i < n; i++)
    {
      std::string ip = "10.1." + std::to_string(i + 1) + ".0";
      P2Paddress[i].SetBase(ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
      interfaces[i] = P2Paddress[i].Assign(sfuDevices[i]);
    }
    Ipv4AddressHelper Wifiaddress[n];
    Ipv4InterfaceContainer APinterfaces[n];
    Ipv4InterfaceContainer Stainterfaces[n];
    for (int i = 0; i < n; i++)
    {
      std::string ip = "10.1." + std::to_string(i + 1 + n) + ".0";
      //      std::cout<<"ip = "<<ip<<std::endl;
      Wifiaddress[i].SetBase(ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
      Stainterfaces[i] = Wifiaddress[i].Assign(staDevices[i]);
      APinterfaces[i] = Wifiaddress[i].Assign(apDevices[i]);
      NS_LOG_DEBUG("APinterfaces[" << i << "].getAddress(0) = " << APinterfaces[i].GetAddress(0));
    }

    // Debug：把每个node的每个接口的ip地址打印出来
    for (int i = 0; i < n; i++)
    {
      Ptr<Ipv4> ippp = wifiApNode[i].Get(0)->GetObject<Ipv4>();
      int interfacenumber = ippp->GetNInterfaces();
      for (int k = 0; k < interfacenumber; k++)
      {
        Ipv4Address ipaddress = ippp->GetAddress(k, 0).GetLocal();
        NS_LOG_DEBUG("Node(" << i << "),interface(" << k << ")   it's IPAddress =" << ipaddress);
      }
    }

    // 给每个user(WifiSta)装上Client，给Center装上Server
    uint16_t client_ul = 81;
    uint16_t client_dl = 80;
    uint16_t client_peer = 81;

    for (int id = 0; id < n; id++)
    {
      Ipv4Address serverAddr = interfaces[id].GetAddress(1); // sfuCenter
      Ptr<VcaServer> vcaServerApp = CreateObject<VcaServer>();
      vcaServerApp->SetLocalAddress(serverAddr);
      vcaServerApp->SetLocalUlPort(client_peer);
      vcaServerApp->SetPeerDlPort(client_dl);
      vcaServerApp->SetLocalDlPort(client_dl);
      sfuCenter.Get(0)->AddApplication(vcaServerApp);
      vcaServerApp->SetStartTime(Seconds(0.0));
      vcaServerApp->SetStopTime(Seconds(simulationDuration));

      for (int i = 0; i < nWifi[id]; i++)
      {
        Ipv4Address staAddr = Stainterfaces[id].GetAddress(i);
        NS_LOG_UNCOND("SFU VCA NodeId " << wifiStaNodes[id].Get(i)->GetId() << " " << sfuCenter.Get(0)->GetId());
        Ptr<VcaClient> vcaClientApp = CreateObject<VcaClient>();
        vcaClientApp->SetFps(30);
        vcaClientApp->SetBitrate(1000);
        vcaClientApp->SetLocalAddress(staAddr);
        vcaClientApp->SetPeerAddress(std::vector<Ipv4Address>{serverAddr});
        vcaClientApp->SetLocalUlPort(client_ul);
        vcaClientApp->SetLocalDlPort(client_dl);
        vcaClientApp->SetPeerPort(client_peer);
        vcaClientApp->SetNodeId(wifiStaNodes[id].Get(i)->GetId());
        vcaClientApp->SetMaxBitrate(maxBitrate);
        wifiStaNodes[id].Get(i)->AddApplication(vcaClientApp);

        vcaClientApp->SetStartTime(Seconds(0.0));
        vcaClientApp->SetStopTime(Seconds(simulationDuration));
      }
    }

    NS_LOG_DEBUG("Successfully Done [SFU MODE] !!!!\n");
  }

  FlowMonitorHelper flowmonHelper;
  flowmonHelper.InstallAll();

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  // int tracing = 1;
  // if (tracing) {
  //   phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
  //   pointToPoint[0].EnablePcapAll("third");
  //   phy.EnablePcap("third", apDevices[0].Get(0));
  //   phy.EnablePcap("third", apDevices[1].Get(0));
  //   phy.EnablePcap("third-sta", staDevices[0].Get(0));
  // }

  Simulator::Stop(Seconds(simulationDuration + 1));
  Simulator::Run();
  flowmonHelper.SerializeToXmlFile("test-emulation.flowmon", true, true);
  Simulator::Destroy();
  return 0;
}