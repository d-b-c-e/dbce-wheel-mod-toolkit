#include "cadence_impact_mixer.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
int main(int argc,char**argv){
 using namespace dbce::force;
 for(int hz:{20,30,50,60,120,250,1000})for(int phase=0;phase<16;phase++){
  double dt=1.0/hz,start=1-dt+phase*dt/16,area=0;CadenceImpactMixer m;assert(m.trigger(1,1,1,true));float peak=0,previous=0;
  for(int i=0;i<int(std::ceil(.4/dt))+2;i++){
   float force=m.mix(0,start,start+dt,true,true);assert(!m.last_input_rejected&&std::abs(force)<=.25f);
   assert(std::abs(force-previous)<=1.5f*float(dt)+1e-6f);peak=std::max(peak,m.last_interval_pulse);area+=m.last_interval_pulse*dt;
   previous=force;start+=dt;
  }
  assert(peak>0&&std::abs(area)<1e-8&&previous==0);
 }
 CadenceImpactMixer b;assert(!b.trigger(1,1,0));assert(b.mix(0,0,.02,true)==0);
 assert(b.trigger(1,1,.02,true));assert(b.mix(0,.02,.04,true,false)==0&&!b.last_had_impact);
 assert(b.mix(.25f,.04,.10,true,true)==0&&b.last_input_rejected);
 CadenceImpactMixer p;assert(p.trigger(1,1,0,true));assert(p.mix(0,0,.02,true,true)>0);assert(p.mix(.25f,.02,.04,false,true)==0);
 assert(p.mix(0,.04,.06,true,true)==0);
 unsigned rows=0;
 if(argc==2){
  std::ifstream file(argv[1]);assert(file);std::string line;std::getline(file,line);
  int previousSeries=-1;CadenceImpactMixer m;
  while(std::getline(file,line)){
   std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);
   int series;double start,end;float structural,amplitude,direction,expectedPulse,expectedTarget,expectedCommand;
   assert(bool(row>>series>>start>>end>>structural>>amplitude>>direction>>expectedPulse>>expectedTarget>>expectedCommand));
   if(series!=previousSeries){m.reset();assert(m.trigger(amplitude,direction,1,true));previousSeries=series;}
   float force=m.mix(structural,start,end,true,true);assert(!m.last_input_rejected);
   assert(std::abs(m.last_interval_pulse-expectedPulse)<1e-6&&std::abs(m.last_target-expectedTarget)<1e-6&&std::abs(force-expectedCommand)<1e-6);rows++;
  }
  assert(rows>1000);
 }
 std::printf("PASS cadence/headroom/slew/gate pure native checks; %u managed/native parity rows; no devices\n",rows);
}
