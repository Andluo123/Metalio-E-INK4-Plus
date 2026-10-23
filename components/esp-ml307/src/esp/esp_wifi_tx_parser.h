#pragma once
#include <cstddef>
#include <cstdint>
namespace wifi_tx_trace {
enum class Result : uint8_t { Target, Other, Malformed, Unsupported, Protected };
enum class Format : uint8_t { Ethernet, Vlan, RawIp, Snap, Wifi, Scan, Unknown };
struct Packet {
 uint32_t seq=0,ack=0,src=0,dst=0;
 uint16_t source_port=0,target_port=0,bytes=0,ip_total=0,ip_id=0,window=0,fc=0,wifi_seq=0;
 uint8_t flags=0,offset=0,tid=255;Format format=Format::Unknown;
};
inline uint16_t U16(const uint8_t* p){return (unsigned(p[0])<<8)|p[1];}
inline uint32_t U32(const uint8_t* p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}
inline bool Snap(const uint8_t* p,size_t n){return n>=8&&p[0]==0xaa&&p[1]==0xaa&&p[2]==3&&p[3]==0&&p[4]==0&&(p[5]==0||p[5]==0xf8)&&U16(p+6)==0x0800;}
inline bool Checksum(const uint8_t* p,size_t n){uint32_t sum=0;for(size_t i=0;i<n;i+=2)sum+=U16(p+i);while(sum>>16)sum=(sum&65535)+(sum>>16);return sum==65535;}
inline Result Ip(const uint8_t* data,size_t size,size_t off,Format format,Packet& p){
 if(off>size||size-off<20)return Result::Malformed;
 const auto* ip=data+off;const unsigned ihl=(ip[0]&15)*4;
 if(ip[0]>>4!=4||ihl<20||size-off<ihl)return Result::Malformed;
 if(ip[9]!=6)return Result::Other;
 if((U16(ip+6)&0x3fff)!=0)return Result::Unsupported; // Never parse fragments as TCP.
 const unsigned total=U16(ip+2);
 if(total<ihl+20||size-off<ihl+20)return Result::Malformed;
 const auto* tcp=ip+ihl;const unsigned thl=(tcp[12]>>4)*4;
 if(thl<20||thl>60||total<ihl+thl||size-off<ihl+thl)return Result::Malformed;
 p.src=U32(ip+12);p.dst=U32(ip+16);p.source_port=U16(tcp);p.target_port=U16(tcp+2);
 p.seq=U32(tcp+4);p.ack=U32(tcp+8);p.flags=tcp[13];p.window=U16(tcp+14);
 p.bytes=total-ihl-thl;p.ip_total=total;p.ip_id=U16(ip+4);p.offset=off;p.format=format;return Result::Target;
}
inline Result Parse(const uint8_t* data,size_t size,Packet& p){
 if(!data||size==0)return Result::Malformed;
 if(size>=14){size_t off=14;uint16_t type=U16(data+12);bool vlan=false;
  for(unsigned i=0;i<2&&(type==0x8100||type==0x88a8);++i){if(size<off+4)return Result::Malformed;type=U16(data+off+2);off+=4;vlan=true;}
  if(type==0x0800)return Ip(data,size,off,vlan?Format::Vlan:Format::Ethernet,p);
  if(type==0x0806||type==0x86dd)return Result::Other;
 }
 if(data[0]>>4==4)return Ip(data,size,0,Format::RawIp,p);
 if(Snap(data,size))return Ip(data,size,8,Format::Snap,p);
 bool protected_frame=false;
 if(size>=24&&(data[0]&0x0c)==0x08&&(data[0]&3)==0){
  p.fc=uint16_t(data[0])|(uint16_t(data[1])<<8);p.wifi_seq=(uint16_t(data[22])|(uint16_t(data[23])<<8))>>4;
  size_t off=24;if((data[1]&3)==3)off+=6;
  if(data[0]&0x80){if(size<off+2)return Result::Malformed;p.tid=data[off]&15;off+=2;if(data[1]&0x80)off+=4;}
  protected_frame=(data[1]&0x40)!=0;
  if(off<=size&&Snap(data+off,size-off))return Ip(data,size,off+8,Format::Wifi,p);
  if(off+8<=size&&Snap(data+off+8,size-off-8))return Ip(data,size,off+16,Format::Wifi,p);
 }
 // Some driver versions prepend private metadata. Only accept bounded IPv4
 // candidates with a valid header checksum. Connection filtering is external.
 for(size_t off=1;off+40<=size&&off<=96;++off){
  const auto* ip=data+off;const unsigned ihl=(ip[0]&15)*4;
  if(ip[0]>>4!=4||ihl<20||ihl>60||size-off<ihl+20||ip[9]!=6||!Checksum(ip,ihl))continue;
  Packet candidate=p;if(Ip(data,size,off,Format::Scan,candidate)==Result::Target){p=candidate;return Result::Target;}
 }
 return protected_frame?Result::Protected:Result::Unsupported;
}
inline const char* Name(Format f){switch(f){case Format::Ethernet:return "ethernet";case Format::Vlan:return "vlan";case Format::RawIp:return "raw_ip";case Format::Snap:return "llc_snap";case Format::Wifi:return "80211";case Format::Scan:return "validated_offset";default:return "unknown";}}
inline const char* Name(Result r){switch(r){case Result::Target:return "target";case Result::Other:return "other_flow";case Result::Malformed:return "malformed_or_short";case Result::Protected:return "protected_unparsed";default:return "unsupported";}}
}
