#!/usr/bin/env python3
"""Exercise actual deck scan/log methods across repeated and paginated tables."""
from pathlib import Path
import subprocess
import tempfile
import resource
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
root=Path(__file__).resolve().parents[1]
source=(root/'firmware-cardputer/src/app/deck_app.cpp').read_text()
def function(name):
    start=source.index('void DeckApp::'+name+'(')
    brace=source.index('{',start); depth=1; end=brace+1
    while depth:
        depth += (source[end]=='{')-(source[end]=='}'); end+=1
    return source[start:end]
prefix=r'''
#include <vector>
#include <cstdint>
#include <cassert>
#include "ocp.h"
namespace ocp { enum class LinkState { Ready }; }
namespace storage {
struct Stats {bool open=true;}; Stats stats; int saved=0;
const Stats &wardriveLogStats(){return stats;}
void wardriveLogAp(int,int,uint32_t){++saved;}
}
struct Client {
 bool accepted=true;
 ocp::LinkState state(){return ocp::LinkState::Ready;}
 bool send(const char*,uint32_t){return accepted;}
};
struct Scan {
 std::vector<int> data;
 void begin(){data.clear();}
 const std::vector<int>& rows(){return data;}
};
struct Gps {int fix(){return 0;} uint32_t fixAgeMs(uint32_t){return 0;}};
class DeckApp {
public:
 Client client_; Scan scan_; Gps gnss_; uint32_t now_=0,scan_started_ms_=0;
 bool scan_pending_=false; size_t next_page_=0,wardrive_logged_upto_=0;
 template<class T> void retrySoon(T,uint32_t){}
 void requestScan(uint32_t); void logScanRows();
};
'''
main=r'''
int main(){
 DeckApp d;
 for(int n:{5,3,4,4}) {d.requestScan(1);d.scan_.data.resize(n);d.logScanRows();d.logScanRows();}
 assert(storage::saved==16);
 d.requestScan(2);d.scan_.data.resize(2);d.logScanRows();
 d.client_.accepted=false;d.requestScan(3);d.logScanRows();assert(storage::saved==18);
 d.scan_.data.resize(4);d.logScanRows();assert(storage::saved==20);
}
'''
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory); methods=function('requestScan')+'\n'+function('logScanRows')
    def run(text):
        (p/'test.cpp').write_text(prefix+text+main)
        subprocess.run(['g++','-std=c++17','-include','cstddef','-I'+str(root/'protocol'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
        return subprocess.run([str(p/'test')],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode
    assert run(methods)==0, 'repeated scan tables lost observations'
    mutant=methods.replace('wardrive_logged_upto_ = 0;\n    scan_.begin();','scan_.begin();',1)
    assert mutant!=methods and run(mutant)!=0, 'cursor regression must fail'
print('wardrive scan cursor: repeated/equal/smaller tables, pages and busy send passed; mutation rejected')
