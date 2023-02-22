#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <string.h>
#include <cstdlib>
// #include <boost/lexical_cast.hpp>
#include "ns3/ipv4-address.h"

//改编自first.cc
//
// Default Network Topology
//
//       10.1.1.0
// n0 -------------- n1
//    point-to-point
//
 
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TestScriptExample");

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  std::string mode = "p2p";
  int n = 3;
  cmd.AddValue("mode","help_info",mode);
  cmd.AddValue("n","help_info",n);
  cmd.Parse (argc, argv);
  
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  


  
  if(mode == "p2p"){
    printf("Now it's p2p!\n");
    //创建节点
    int num = (n-1)*n/2;
    NodeContainer nodes;
    nodes.Create (n);

    //创建节点之间的信道
    PointToPointHelper pointToPoint[num];
    for(int i = 0; i < num; i ++){
      pointToPoint[i].SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
      pointToPoint[i].SetChannelAttribute ("Delay", StringValue ("2ms"));
    }

    //在信道上安装NetDevice
    NetDeviceContainer devices[num];
    int p = 0;
    int dev[100][100];
    for(int i = 0; i < n; i++)
      for(int j = i + 1; j < n; j++){
        devices[p] = pointToPoint[p].Install (nodes.Get(i),nodes.Get(j));
        dev[i][j] = p;
        dev[j][i] = p;
        p++;
      }
    for(int i=0;i<n;i++)
      for(int j=0;j<n;j++)
        printf("dev[%d][%d]=%d\n",i,j,dev[i][j]);

    InternetStackHelper stack;
    stack.Install (nodes);

    //给NetDevices分配IPv4地址
    Ipv4AddressHelper address[num];
    Ipv4InterfaceContainer interfaces[num];
    for(int i = 0; i < num; i++){
      std::string ip = "10.1." + std::to_string(i+1) + ".0";
      address[i].SetBase (ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
  //    address.SetBase("10.1.1.0","255.255.255.0");
      interfaces[i] = address[i].Assign (devices[i]);
    }

    //给所有点两两之间安装CS
    int port_c = 9;
    for(int i = 0; i < n; i++){
      for(int j = 0; j < n; j++){
        if(i == j) continue;
        // Client -> i  Server -> j
        printf("Client %d ;; Server %d   dev[%d][%d]=%d\n",i,j,i,j,dev[i][j]);
        UdpEchoServerHelper echoServer(port_c);
        ApplicationContainer serverApps = echoServer.Install (nodes.Get(j));
        serverApps.Start (Seconds (1.0));
        serverApps.Stop (Seconds (10.0));

        int op = (i<j)?0:1;
        printf("op=%d\n",op);
        UdpEchoClientHelper echoClient (interfaces[dev[i][j]].GetAddress(op), port_c);
        echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
        echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
        echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

        ApplicationContainer clientApps = echoClient.Install (nodes.Get(i));
        clientApps.Start (Seconds (2.0));
        clientApps.Stop (Seconds (10.0));
        port_c++;
      }
    }

    printf("Successfully Done!!!! [P2P MODE] \n");

    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
  }







  else if(mode == "sfu"){
    printf("Now it's sfu!\n");
    //创建节点
    NodeContainer nodes;
    nodes.Create (n+1);
    //0,1,2,3,4,5...n-1都连到中间的选择转发单元n上

    //创建节点之间的信道
    PointToPointHelper pointToPoint[n];
    for(int i = 0; i < n; i ++){
      pointToPoint[i].SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
      pointToPoint[i].SetChannelAttribute ("Delay", StringValue ("2ms"));
    }

    //在信道上安装NetDevice
    NetDeviceContainer devices[n];
    for(int i = 0; i < n; i++)
        devices[i] = pointToPoint[i].Install (nodes.Get(i),nodes.Get(0));
    InternetStackHelper stack;
    stack.Install (nodes);

    //给NetDevices分配IPv4地址
    Ipv4AddressHelper address[n];
    Ipv4InterfaceContainer interfaces[n];
    for(int i = 0; i < n; i++){
      std::string ip = "10.1." + std::to_string(i+1) + ".0";
      address[i].SetBase (ns3::Ipv4Address(ip.c_str()), "255.255.255.0");
  //    address.SetBase("10.1.1.0","255.255.255.0");
      interfaces[i] = address[i].Assign (devices[i]);
    }

    //给所有点与选择转发单元安装CS
    int port_c = 9;
    for(int i = 0; i < n; i++){
      for(int op = 0; op <= 1; op++){
        // op = 0 :::: Client -> i  Server -> 0
        // op = 1 :::: Client -> 0  Server -> i
        int server = (op==0)?0:i;
        int client = (op==0)?i:0;
        UdpEchoServerHelper echoServer(port_c);
        ApplicationContainer serverApps = echoServer.Install (nodes.Get(server));
        serverApps.Start (Seconds (1.0));
        serverApps.Stop (Seconds (10.0));

        UdpEchoClientHelper echoClient (interfaces[i].GetAddress(op), port_c);//(i,0)->(0,1)
        echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
        echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
        echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

        ApplicationContainer clientApps = echoClient.Install (nodes.Get(client));
        clientApps.Start (Seconds (2.0));
        clientApps.Stop (Seconds (10.0));
        port_c++;
      }
    }

    printf("Successfully Done [SFU MODE] !!!!\n");

    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
  }
}
