#include "../components/esp-ml307/src/esp/esp_wifi_tx_parser.h"
#include <vector>
#include <cassert>
#include <cstdio>
using namespace wifi_tx_trace;
using Bytes=std::vector<uint8_t>;
Bytes IpPacket(){Bytes b(40);b[0]=0x45;b[2]=2;b[3]=40;b[4]=0x12;b[5]=0x34;b[8]=64;b[9]=6;b[12]=192;b[13]=168;b[14]=5;b[15]=228;b[16]=192;b[17]=168;b[18]=5;b[19]=77;b[20]=0xd0;b[21]=0x12;b[22]=0x1f;b[23]=0x90;b[27]=123;b[32]=0x50;b[33]=0x18;uint32_t s=0;for(int i=0;i<20;i+=2)s+=U16(b.data()+i);while(s>>16)s=(s&65535)+(s>>16);b[10]=(~s)>>8;b[11]=~s;return b;}
void Append(Bytes& a,const Bytes& b){a.insert(a.end(),b.begin(),b.end());}
void Expect(const Bytes& b,Format f){Packet p;assert(Parse(b.data(),b.size(),p)==Result::Target);assert(p.format==f);assert(p.seq==123&&p.bytes==512&&p.ip_id==0x1234&&p.target_port==8080);}
int main(){
 auto ip=IpPacket();Expect(ip,Format::RawIp);
 Bytes eth(14);eth[12]=8;Append(eth,ip);Expect(eth,Format::Ethernet);
 Bytes vlan(18);vlan[12]=0x81;vlan[16]=8;Append(vlan,ip);Expect(vlan,Format::Vlan);
 Bytes snap={0xaa,0xaa,3,0,0,0,8,0};Append(snap,ip);Expect(snap,Format::Snap);
 for(bool qos:{false,true})for(bool ccmp:{false,true}){
  Bytes wifi(qos?26:24);wifi[0]=qos?0x88:8;wifi[1]=ccmp?0x41:1;
  if(ccmp){wifi.resize(wifi.size()+8);}Append(wifi,snap);Expect(wifi,Format::Wifi);
 }
 Bytes four(32);four[0]=0x88;four[1]=3;Append(four,snap);Expect(four,Format::Wifi);
 Bytes metadata(9,0xcc);Append(metadata,ip);Expect(metadata,Format::Scan);
 Packet p;auto short_ip=ip;short_ip.resize(25);assert(Parse(short_ip.data(),short_ip.size(),p)==Result::Malformed);
 auto bad_tcp=ip;bad_tcp[32]=0x40;assert(Parse(bad_tcp.data(),bad_tcp.size(),p)==Result::Malformed);
 auto other=ip;other[19]=78;assert(Parse(other.data(),other.size(),p)==Result::Target);assert(p.dst==0xc0a8054e);
 auto fragment=ip;fragment[6]=0x20;assert(Parse(fragment.data(),fragment.size(),p)==Result::Unsupported);
 Bytes cipher(40,0x11);cipher[0]=8;cipher[1]=0x41;assert(Parse(cipher.data(),cipher.size(),p)==Result::Protected);
 Bytes nonsense(40,0xcc);assert(Parse(nonsense.data(),nonsense.size(),p)==Result::Unsupported);
 assert(Parse(nullptr,0,p)==Result::Malformed);
 // A fragment or malformed header must never be accepted by offset scanning.
 Bytes invalid_metadata(9,0xcc);auto invalid=ip;invalid[10]^=1;Append(invalid_metadata,invalid);assert(Parse(invalid_metadata.data(),invalid_metadata.size(),p)!=Result::Target);
 puts("wifi_tx_parser: 18 format/bounds/filter checks passed");
}
