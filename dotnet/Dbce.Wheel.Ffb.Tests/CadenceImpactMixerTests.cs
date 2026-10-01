using System;
using Dbce.Wheel.Ffb;
using Xunit;
namespace Dbce.Wheel.Ffb.Tests
{
 public class CadenceImpactMixerTests
 {
  [Theory][InlineData(20)][InlineData(30)][InlineData(50)][InlineData(60)][InlineData(120)][InlineData(250)][InlineData(1000)]
  public void EveryPhaseRetainsPositiveLobeAndSignedArea(int hz)
  {
   double dt=1.0/hz;
   for(int phase=0;phase<16;phase++){
    var m=new CadenceImpactMixer();Assert.True(m.Trigger(1,1,1,true));double start=1-dt+phase*dt/16,area=0;float peak=0,previous=0;
    for(int i=0;i<Math.Ceiling(.4/dt)+2;i++){
     float force=m.Mix(0,start,start+dt,true,true);
     Assert.False(m.LastInputRejected);Assert.InRange(Math.Abs(force),0,.25f);
     Assert.InRange(Math.Abs(force-previous),0,1.5f*(float)dt+1e-6f);
     peak=Math.Max(peak,m.LastIntervalPulse);area+=m.LastIntervalPulse*dt;previous=force;start+=dt;
    }
    Assert.True(peak>0);Assert.InRange(Math.Abs(area),0,1e-8);Assert.Equal(0,previous);
   }
  }
  [Fact] public void PointSampleBaselineStillMissesTwentyMillisecondPositiveLobeButNewPathDoesNot()
  {
   var old=new ImpactMixer();old.Trigger(1,1,0);float oldPeak=0;
   for(int i=0;i<8;i++)oldPeak=Math.Max(oldPeak,old.Mix(0,i*.02));Assert.Equal(0,oldPeak);
   var n=new CadenceImpactMixer();n.Trigger(1,1,0,true);
   Assert.InRange(n.Mix(0,0,.02,true,true),.029999f,.030001f);
   Assert.InRange(n.LastIntervalPulse,.499999f,.500001f);
   Assert.InRange(n.LastTarget,.031249f,.031251f);
   Assert.Equal(.01,ImpactMixer.EnvelopeIntegral(.02),10);
   Assert.Equal(0,ImpactMixer.EnvelopeIntegral(.1));
  }
  [Fact] public void FinalBudgetAndSlewApplyAfterOverlapAndStructuralMixing()
  {
   var n=new CadenceImpactMixer();float previous=0;
   for(int i=0;i<1000;i++){
    double start=i*.001;if(i==100)for(int j=0;j<8;j++)Assert.True(n.Trigger(1,j%2==0?1:-1,start,true));
    float force=n.Mix(i%50<25?1:-1,start,start+.001,true,true);
    Assert.InRange(Math.Abs(force),0,.25f);Assert.InRange(Math.Abs(force-previous),0,.001501f);previous=force;
   }
   Assert.False(n.Trigger(1,1,1)); // unverified default
   n.Mix(0,1,1.001,true,false); Assert.False(n.LastHadImpact); // queue is cleared
  }
  [Fact] public void AbsentUnverifiedOrInvalidEventsNeverBecomeCollisionOutput()
  {
   var n=new CadenceImpactMixer();Assert.False(n.Trigger(1,1,0));Assert.Equal(0,n.Mix(0,0,.02,true));
   foreach(float bad in new[]{float.NaN,float.PositiveInfinity,-1f,0f,1.1f})Assert.False(n.Trigger(bad,1,.02,true));
   Assert.False(n.Trigger(1,0,.02,true));Assert.False(n.Trigger(1,1,-1,true));
   Assert.True(n.Trigger(1,1,.02,true));Assert.Equal(0,n.Mix(0,.02,.04,true,false));Assert.False(n.LastHadImpact);
   Assert.Equal(0,n.Mix(0,.04,.06,true,true));
  }
  [Fact] public void PauseFocusStaleRespawnAndRewindClearQueueAndZeroImmediately()
  {
   foreach(string gate in new[]{"pause","focus","respawn"}){
    var n=new CadenceImpactMixer();n.Trigger(1,1,0,true);Assert.True(n.Mix(0,0,.02,true,true)>0);
    Assert.Equal(0,n.Mix(.25f,.02,.04,false,true));Assert.Equal("inactive-zero",n.Status);
    Assert.Equal(0,n.Mix(0,.04,.06,true,true));
   }
   foreach(var interval in new[]{new[]{.02,.08},new[]{.01,.03},new[]{.03,.05},new[]{.02,.02},new[]{double.NaN,.04}}){
    var n=new CadenceImpactMixer();n.Trigger(1,1,0,true);n.Mix(0,0,.02,true,true);
    Assert.Equal(0,n.Mix(.25f,interval[0],interval[1],true,true));Assert.True(n.LastInputRejected);
    Assert.Equal(0,n.Mix(0,.08,.10,true,true));
   }
   var f=new CadenceImpactMixer();f.Trigger(1,1,0,true);Assert.Equal(0,f.Mix(0,.01,.03,true,true));Assert.True(f.LastInputRejected);
  }
  [Fact] public void ValidNoEventStructuralRampHasNoReserveAttenuation()
  {
   var m=new CadenceImpactMixer();for(int i=0;i<100;i++){
    float requested=Math.Min(.25f,(i+1)*.01f);float result=m.Mix(requested,i*.02,(i+1)*.02,true);
    Assert.InRange(Math.Abs(result-requested),0,1e-6);Assert.False(m.LastHadImpact);
   }
  }
  [Fact] public void ImmutableInvalidBudgetCannotBeActivated()
  {
   Assert.Throws<ArgumentOutOfRangeException>(()=>new CadenceImpactMixer(float.NaN));
   Assert.Throws<ArgumentOutOfRangeException>(()=>new CadenceImpactMixer(.25f,0));
   Assert.Throws<ArgumentOutOfRangeException>(()=>new CadenceImpactMixer(.25f,1.5f,.51f));
   Assert.Equal(150,CadenceImpactMixer.RequiredHoldTimeoutMs);
  }
 }
}
