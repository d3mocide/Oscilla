#!/usr/bin/env python3
"""Exercise actual scan/drain methods: one row per loop, no premature rescan."""
from pathlib import Path
import subprocess,tempfile,resource
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
root=Path(__file__).resolve().parents[1]
source='\n'.join((root/f'firmware-cardputer/src/app/{name}').read_text() for name in ['deck_app.cpp','deck_wardrive.cpp'])
def function(name):
    start=source.index(('bool' if name=='wardriveRowsPending' else 'void')+' DeckApp::'+name+'(')
    brace=source.index('{',start);depth=1;end=brace+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
prefix=r'''
#include <vector>
#include <cstdint>
#include <cassert>
#include "ocp.h"
namespace ocp {enum class LinkState {Ready};}
namespace storage {
struct Stats {bool open=true;};Stats stats;int saved=0,closed=0;
const Stats &wardriveLogStats(){return stats;}
void wardriveLogAp(int,int,uint32_t){++saved;}
}
struct Client {
 bool accepted=true,busy=false;unsigned sends=0;
 ocp::LinkState state(){return ocp::LinkState::Ready;}
 bool send(const char*,uint32_t){if(accepted)++sends;return accepted;}
 bool pending(){return busy;}
};
struct Scan {std::vector<int> data;void begin(){data.clear();}const std::vector<int>& rows()const{return data;}};
class DeckApp {
public:
 Client client_;Scan scan_;uint32_t now_=0,scan_started_ms_=0,scan_wait_started_ms_=0,wardrive_page_fix_ms_=0;
 int wardrive_page_fix_=0;bool scan_pending_=false,wardrive_rescan_=false,wardrive_closing_=false,wardrive_stream_supported_=false;
 size_t next_page_=0,wardrive_logged_upto_=0;
 template<class T>void retrySoon(T,uint32_t){}
 void requestScan(uint32_t);void logScanRows();bool wardriveRowsPending()const;void tickWardrive(uint32_t);
 void finishWardrive(){++storage::closed;storage::stats.open=false;wardrive_closing_=false;wardrive_rescan_=false;}
};
'''
main=r'''
int main(){
 DeckApp d;
 for(int n:{5,3,4,4}){
   d.requestScan(1);d.scan_.data.resize(n);const int before=storage::saved;
   d.logScanRows();assert(storage::saved==before+1);
   while(d.wardriveRowsPending())d.logScanRows();d.logScanRows();
 }
 assert(storage::saved==16);
 d.requestScan(2);d.scan_.data.resize(2);d.logScanRows();
 const auto sends=d.client_.sends;d.requestScan(3);assert(d.scan_.data.size()==2&&d.client_.sends==sends);
 d.logScanRows();assert(storage::saved==18);
 d.client_.accepted=false;d.requestScan(4);d.scan_.data.resize(4);
 while(d.wardriveRowsPending())d.logScanRows();assert(storage::saved==20);
 d.client_.accepted=true;d.requestScan(5);d.scan_.data.resize(3);d.wardrive_rescan_=true;
 const auto before=d.client_.sends;d.tickWardrive(6);assert(d.client_.sends==before);
 d.tickWardrive(7);assert(d.client_.sends==before);
 d.tickWardrive(8);assert(d.client_.sends==before+1&&d.scan_.data.empty());
 d.scan_.data.resize(2);d.wardrive_closing_=true;d.wardrive_rescan_=true;
 d.tickWardrive(9);assert(storage::stats.open&&storage::closed==0);
 d.tickWardrive(10);assert(!storage::stats.open&&storage::closed==1);
 // start_wardrive owns AP rows: page logging must not re-log stream-added rows.
 DeckApp e;storage::stats.open=true;e.wardrive_stream_supported_=true;e.scan_.data.resize(5);
 const int saved=storage::saved;e.tickWardrive(20);e.tickWardrive(21);
 assert(storage::saved==saved&&!e.wardriveRowsPending());
}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);methods='\n'.join(function(name) for name in ['requestScan','logScanRows','wardriveRowsPending','tickWardrive'])
 def run(text):
  (p/'test.cpp').write_text(prefix+text+main)
  subprocess.run(['g++','-std=c++17','-include','cstddef','-I'+str(root/'protocol'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  return subprocess.run([str(p/'test')],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode
 assert run(methods)==0,'bounded drain/cursor/rescan/close regression'
 mutant=methods.replace('wardrive_logged_upto_ = 0;\n','',1)
 assert mutant!=methods and run(mutant)!=0,'cursor regression must fail'
 mutant=methods.replace('    if(wardriveRowsPending())return;','',1)
 assert mutant!=methods and run(mutant)!=0,'premature rescan must fail'
 mutant=methods.replace('    if(wardrive_stream_supported_)wardrive_logged_upto_=scan_.rows().size();\n','',1)
 assert mutant!=methods and run(mutant)!=0,'stream-mode double logging must fail'
print('wardrive drain: one row per tick, repeated tables, paging, busy sends, deferred rescan/close, stream owns AP rows; mutations rejected')
