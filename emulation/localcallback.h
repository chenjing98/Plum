#include<set>
#include<map>
extern std::set<uint32_t> m_paused[48];//node id to paused
extern std::map<uint8_t,ns3::Ipv4Address> socket_to_ip[48];
extern std::map<ns3::Ipv4Address,uint32_t> ip_to_node[48];

