"""Exercise the real FT6336U upgrade protocol with a simulated I2C device."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DISPLAY = ROOT / "main/boards/metalio-e-ink4-plus/display"
TEST = r'''
#include "ft6336u_upgrade_protocol.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstdio>
using namespace Ft6336uUpgrade;
struct Mock {
    int version=2, packets=0, erases=0, resets=0;
    bool wrong_chip=false, wrong_boot=false, erase_timeout=false,
         packet_timeout=false, bad_ecc=false, bad_version=false, fail_write=false;
    uint16_t status=0;
    uint8_t xor_value=0;
    std::vector<uint8_t> written;
    bool Write(const uint8_t* p,size_t n) {
        if(fail_write && p[0]==0xbf)return false;
        if(p[0]==0x61){assert(erases==0);++erases;status=0xb002;}
        if(p[0]==0x07){++resets;if(packets && !bad_version)version=3;}
        if(p[0]==0xbf){
            const size_t addr=(size_t(p[1])<<16)|(size_t(p[2])<<8)|p[3];
            const size_t len=(size_t(p[4])<<8)|p[5];
            assert(erases==1 && addr==written.size() && len<=32 && n==len+6);
            for(size_t i=0;i<len;++i){written.push_back(p[6+i]);xor_value^=p[6+i];}
            status=uint16_t(0xb003+packets++);
        }
        return true;
    }
    bool Read(const uint8_t* p,size_t n,uint8_t* out,size_t len) {
        switch(p[0]) {
        case 0xa3:out[0]=wrong_chip?0:0x64;break;
        case 0x9f:out[0]=0x26;break;
        case 0xa6:out[0]=uint8_t(version);break;
        case 0x90:assert(n==4 && len==2);out[0]=wrong_boot?0:0x79;out[1]=0x1c;break;
        case 0x6a:{assert(n==4 && len==2);auto s=(erase_timeout || (packet_timeout && packets))?0:status;out[0]=s>>8;out[1]=s;break;}
        case 0xcc:out[0]=bad_ecc?0:xor_value;break;
        default:assert(false);
        }
        return true;
    }
    void Delay(int) {}
};
int main(int argc,char**argv) {
    assert(argc==2);
    std::ifstream input(argv[1],std::ios::binary);
    std::vector<uint8_t> fw((std::istreambuf_iterator<char>(input)),{});
    assert(Validate(fw.data(),fw.size()));
    auto run=[&](Mock&io){return Run(io,fw.data(),fw.size());};
    Mock ok;assert(run(ok)==Result::Updated);
    assert(ok.erases==1 && ok.resets==1 && ok.packets==1472);
    assert(ok.written.size()==kAppSize && std::equal(ok.written.begin(),ok.written.end(),fw.begin()));
    for(int version:{3,4}){Mock io;io.version=version;assert(run(io)==Result::Current && !io.erases && !io.resets);}
    Mock bad;bad.wrong_chip=true;assert(run(bad)==Result::WrongChip && !bad.erases);
    Mock boot;boot.wrong_boot=true;assert(run(boot)==Result::BootIdError && !boot.erases && boot.resets==1);
    Mock erase;erase.erase_timeout=true;assert(run(erase)==Result::EraseError && !erase.packets && erase.resets==1);
    Mock packet;packet.packet_timeout=true;assert(run(packet)==Result::PacketError && packet.packets==1 && packet.resets==1);
    Mock ecc;ecc.bad_ecc=true;assert(run(ecc)==Result::EccError && ecc.resets==1);
    Mock ver;ver.bad_version=true;assert(run(ver)==Result::VersionError);
    Mock write;write.fail_write=true;assert(run(write)==Result::IoError && write.resets==1);
    Mock invalid;assert(Run(invalid,fw.data(),fw.size()-1)==Result::InvalidImage && !invalid.erases);
    fw[400]^=1;fw[401]^=1; // Same XOR, CRC must still reject corruption.
    assert(run(invalid)==Result::InvalidImage && !invalid.erases);
    puts("PASS: image CRC, chip/version gates, packet bounds/content, status timeouts, ECC, reset and final version");
}
'''

def main():
    compiler = os.environ.get("CXX") or shutil.which("g++")
    if not compiler:
        raise SystemExit("Set CXX to a host g++ compiler")
    with tempfile.TemporaryDirectory(prefix="470-touch-upgrade-") as directory:
        folder = Path(directory)
        source = folder / "check.cpp"
        source.write_text(TEST, encoding="utf-8")
        exe = folder / "check.exe"
        subprocess.run([compiler, "-std=c++17", "-I", str(DISPLAY), str(source), "-o", str(exe)], check=True)
        subprocess.run([str(exe), str(DISPLAY / "firmware/ft6336u_v03_app.bin")], check=True)

if __name__ == "__main__":
    main()
