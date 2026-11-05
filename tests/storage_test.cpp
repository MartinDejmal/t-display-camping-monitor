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
    r.pressure=100000; r.flags=logger::FLAG_SENSOR_OK | logger::FLAG_HUMIDITY | logger::TYPE_BME280;
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
    logger::Record future=record(300); future.flags=0x3001;
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
    // Read legacy history in place, then mix BME and BMP without inventing RH.
    fixture(base,"mixed");
    auto old=record(60); old.flags=logger::LEGACY_RECORD_VERSION | logger::FLAG_SENSOR_OK;
    old.humidity=0;
    old.crc=logger::crc16(reinterpret_cast<uint8_t*>(&old),offsetof(logger::Record,crc));
    {FILE *f=fopen((fakeRoot+"/b00000001.bin").c_str(),"wb");fwrite(&old,1,sizeof(old),f);fclose(f);}
    Storage mixed;assert(mixed.begin());
    auto bmp=record(120); bmp.flags=logger::FLAG_SENSOR_OK | logger::TYPE_BMP085180;
    bmp.pressure=98574;
    bmp.humidity=logger::HUMIDITY_UNAVAILABLE;
    assert(mixed.append(bmp));assert(mixed.append(record(180)));
    testNow=240ULL*1000000;
    HistoryBin separate[4]; mixed.history(clock,240,separate,4);
    assert(separate[0].valid && separate[0].humidityValid && !separate[0].humidityGap && separate[0].minH==0);
    assert(separate[1].valid && !separate[1].gap && !separate[1].humidityValid && separate[1].humidityGap);
    assert(separate[1].minP==98574 && separate[1].maxP==98574);
    assert(separate[2].valid && separate[2].humidityValid && !separate[2].humidityGap);
    mixed.history(clock,240,bins,1);
    assert(bins[0].valid && !bins[0].gap && bins[0].humidityGap);
    Storage reopened;assert(reopened.begin());
    assert(reopened.latest(last) && logger::hasHumidity(last));
    assert(reopened.summary().count==3 && reopened.summary().good==3 && reopened.summary().sessionCount==1);
    assert(reopened.summary().first.elapsedS==60 && reopened.summary().last.elapsedS==180);
    assert(reopened.append(record(240)));
    assert(reopened.summary().count==4 && reopened.summary().last.elapsedS==240);
    auto other=record(1);other.session=2;other.epoch=2100000000;other.flags|=logger::FLAG_UTC;
    other.temperature=8000;other.pressure=90000;
    assert(reopened.append(other));
    assert(reopened.summary().count==5 && reopened.summary().sessionCount==2);
    SessionSummary session;
    assert(reopened.sessionInfo(1,session) && session.count==4 && session.firstElapsed==60 && session.lastElapsed==240);
    assert(reopened.sessionInfo(2,session) && session.count==1 && session.lastElapsed==1);
    assert(session.extrema.valid && session.extrema.minT==8000 && session.extrema.maxT==8000);
    assert(reopened.sessionInfo(1,session) && session.extrema.minT==-1250 && session.extrema.maxT==-1250);
    assert(session.extrema.minP==98574 && session.extrema.maxP==100000);
    assert(session.extrema.humidityValid && session.extrema.minH==0 && session.extrema.maxH==5600);
    auto invalid=record(300);invalid.flags=0;invalid.temperature=-4000;invalid.pressure=110000;
    assert(reopened.append(invalid));
    assert(reopened.sessionInfo(1,session) && session.extrema.minT==-1250 && session.extrema.maxP==100000);
    auto extreme=record(360);extreme.temperature=2534;extreme.pressure=101234;extreme.humidity=7890;
    assert(reopened.append(extreme));
    assert(reopened.sessionInfo(1,session) && session.extrema.maxT==2534 && session.extrema.maxP==101234 && session.extrema.maxH==7890);
    Storage again;assert(again.begin());
    assert(again.sessionInfo(1,session) && session.extrema.maxT==2534 && session.extrema.minH==0);
    reopened.historyForSession(1,240,3600,bins,1);
    assert(bins[0].valid && bins[0].maxT==-1250 && bins[0].minP==98574); // no joining by wrong UTC
    reopened.historyForSession(2,1,3600,bins,1);
    assert(bins[0].valid && bins[0].maxT==8000 && bins[0].minP==90000);
    fixture(base,"bmp-extrema");Storage bmpOnly;assert(bmpOnly.begin());assert(bmpOnly.append(bmp));
    assert(bmpOnly.sessionInfo(1,session) && session.extrema.valid && !session.extrema.humidityValid);
    fixture(base,"wrong-date");
    Storage timeless;assert(timeless.begin());
    for(uint32_t i=0;i<1440;++i){auto r=record(i*60);r.epoch=1704067200+i*60;r.flags|=logger::FLAG_UTC;assert(timeless.append(r));}
    assert(timeless.append(other));
    assert(timeless.used()==1441*24); // a different session's wrong date must not purge history
    std::cout<<"Storage: erased provisioning, no format on corruption, torn writes, CRC recovery, schema protection, rotation, expiry, graph gaps OK\n";
}
