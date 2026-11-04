#include "storage.h"
#include "LittleFS.h"
#include <cassert>
#include <iostream>
#include <limits>
#ifdef _WIN32
#include <direct.h>
#endif
std::string fakeRoot;
size_t fakeCapacity=1245184, fakeWriteLimit=std::numeric_limits<size_t>::max();
bool fakeMount=true, fakeErased=true;
unsigned fakeFormats=0;
FakeFS LittleFS;
uint64_t testNow=0;
uint64_t ClockState::nowUs() const { return testNow; }
uint32_t ClockState::session() const { return 1; }
uint32_t ClockState::epoch() const { return 0; }
void fixture(const std::string &base,const char *name) {
    fakeRoot=base+"/"+name;
#ifdef _WIN32
    _mkdir(fakeRoot.c_str());
#else
    mkdir(fakeRoot.c_str(),0700);
#endif
    fakeMount=true; fakeErased=true; fakeCapacity=1245184;
    fakeWriteLimit=std::numeric_limits<size_t>::max();
}
logger::Record record(uint32_t elapsed) {
    logger::Record r{};
    r.session=1; r.elapsedS=elapsed; r.temperature=-1250; r.humidity=5600;
    r.pressure=100000; r.flags=logger::FLAG_SENSOR_OK;
    return r;
}
int main(int argc,char **argv) {
    assert(argc==2);
    std::string base=argv[1];
    fixture(base,"fresh");
    fakeMount=false;
    Storage fresh;
    assert(fresh.begin() && fakeFormats==1);
    assert(fresh.append(record(60)));
    logger::Record last;
    assert(fresh.latest(last) && logger::valid(last) && last.temperature==-1250);
    fakeWriteLimit=7; // power cut during second record
    assert(!fresh.append(record(120)) && !fresh.ready());
    fakeWriteLimit=std::numeric_limits<size_t>::max();
    Storage repaired;
    assert(repaired.begin());
    assert(repaired.used()==24);
    assert(repaired.append(record(180)));
    size_t count=0;
    repaired.visit([&](const logger::Record &r){++count;assert(r.elapsedS==60||r.elapsedS==180);return true;});
    assert(count==2);
    // A corrupt complete tail is discarded, then append resumes on a boundary.
    {FILE *f=fopen((fakeRoot+"/b00000001.bin").c_str(),"r+b");fseek(f,24,SEEK_SET);fputc(0xff,f);fclose(f);}
    Storage crcRepair; assert(crcRepair.begin() && crcRepair.used()==24);
    assert(crcRepair.append(record(240)));
    // A CRC-valid unknown schema is preserved and refuses further logging.
    logger::Record future=record(300); future.flags=0x2001;
    future.crc=logger::crc16(reinterpret_cast<uint8_t*>(&future),offsetof(logger::Record,crc));
    {FILE *f=fopen((fakeRoot+"/b00000001.bin").c_str(),"ab");fwrite(&future,1,sizeof(future),f);fclose(f);}
    Storage incompatible; assert(!incompatible.begin());
    assert(LittleFS.usedBytes()==72);
    fixture(base,"corrupt"); fakeMount=false; fakeErased=false;
    unsigned formats=fakeFormats;
    Storage corrupt; assert(!corrupt.begin() && fakeFormats==formats);
    fixture(base,"retention"); fakeCapacity=60000;
    Storage ring; assert(ring.begin());
    for(uint32_t i=0;i<2400;++i)assert(ring.append(record(i*60)));
    assert(ring.used()<fakeCapacity*9/10);
    count=0; uint32_t first=0;
    ring.visit([&](const logger::Record&r){if(!count)first=r.elapsedS;++count;return true;});
    assert(first==1440*60 && count==960);
    // A sensor error in an otherwise occupied graph bucket must produce a gap.
    testNow=2400ULL*60*1000000;
    auto bad=record(2399*60); bad.flags=0;
    assert(ring.append(bad));
    HistoryBin bins[1]; ClockState clock;
    ring.history(clock,8*3600,bins,1);
    assert(bins[0].valid && bins[0].gap);
    // Time-based retention removes whole expired blocks without filling flash.
    fixture(base,"expiry");
    Storage expiry;assert(expiry.begin());
    for(uint32_t i=0;i<1440;++i)assert(expiry.append(record(i*60)));
    assert(expiry.append(record(32UL*86400)));
    assert(expiry.used()==24);
    std::cout<<"Storage: erased provisioning, no format on corruption, torn writes, CRC recovery, schema protection, rotation, expiry, graph gaps OK\n";
}
