#include "callback.h"
std::set<uint32_t> m_paused[24];
std::map<uint8_t,ns3::Ipv4Address> socket_to_ip[24];
std::map<ns3::Ipv4Address,uint32_t> ip_to_node[24];

