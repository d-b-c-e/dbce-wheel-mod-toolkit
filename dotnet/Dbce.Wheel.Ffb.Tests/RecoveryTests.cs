using System;
using System.Reflection;
using Xunit;
namespace Dbce.Wheel.Ffb.Tests
{
    public class RecoveryTests
    {
        private static ForceShaper Plain() => new ForceShaper { Deadzone=0, OutputDeadband=0,
            AttackSmoothing=0, DecaySmoothing=0, RampSeconds=0, FadeFullKmh=0, FadeStartKmh=0 };
        [Fact] public void HysteresisPassesBothSignChangesAndHonoursLoweredLimit()
        {
            var s=Plain(); s.RelevelHysteresis=.1f;
            Assert.Equal(.02f,s.Shape(.02f,40,.02f));
            Assert.Equal(-.02f,s.Shape(-.02f,40,.02f));
            Assert.Equal(.02f,s.Shape(.02f,40,.02f));
            s.Shape(.8f,40,.02f); s.PeakLimit=.75f;
            Assert.Equal(.75f,s.Shape(.8f,40,.02f));
            s.PeakLimit=0; Assert.Equal(0,s.Shape(.8f,40,.02f));
        }
        [Fact] public void InvalidSamplesClearHistoryAndNextSampleMatchesFreshShaper()
        {
            foreach(float invalid in new[]{float.NaN,float.PositiveInfinity,float.NegativeInfinity})
            for(int input=0;input<3;input++)
            {
                var s=Plain(); s.SmoothingMs=50; s.Shape(.8f,40,.02f);
                Assert.Equal(0,s.Shape(input==0?invalid:.5f,input==1?invalid:40,input==2?invalid:.02f));
                Assert.True(s.LastInputRejected); Assert.Equal(0,s.Last);
                var fresh=Plain(); fresh.SmoothingMs=50;
                Assert.Equal(fresh.Shape(.5f,40,.02f),s.Shape(.5f,40,.02f));
                Assert.False(s.LastInputRejected);
            }
        }
        [Fact] public void EveryNonFiniteShaperSettingRejectsAndRecovers()
        {
            foreach(var field in typeof(ForceShaper).GetFields(BindingFlags.Instance|BindingFlags.Public))
            {
                if(field.FieldType!=typeof(float)) continue;
                foreach(float invalid in new[]{float.NaN,float.PositiveInfinity,float.NegativeInfinity})
                {
                    var s=Plain(); var original=field.GetValue(s); field.SetValue(s,invalid);
                    Assert.Equal(0,s.Shape(.5f,40,.02f)); Assert.True(s.LastInputRejected);
                    field.SetValue(s,original); Assert.Equal(.5f,s.Shape(.5f,40,.02f));
                    Assert.False(s.LastInputRejected);
                }
            }
        }
        [Fact] public void FiniteOverflowCannotPoisonShaper()
        {
            var s=Plain(); s.Strength=100;
            Assert.Equal(0,s.Shape(float.MaxValue,40,.02f)); Assert.True(s.LastInputRejected);
            Assert.Equal(.5f,s.Shape(.25f,40,.02f));
        }
        [Fact] public void ModelRejectsBadTimeAndSettingsWithoutRetainingEvents()
        {
            foreach(float invalid in new[]{float.NaN,float.PositiveInfinity,float.NegativeInfinity})
            {
                var settings=new ForceModelSettings { ImpactStrength=.5f };
                var m=new ForceModel(settings); var hit=new ForceInputs {Impact=1};
                m.Compute(hit,.02f); Assert.Equal(0,m.Compute(hit,invalid));
                Assert.True(m.LastInputRejected); Assert.False(m.LastWasEvent);
                Assert.Equal(0,m.Compute(new ForceInputs(),.02f));
                Assert.False(m.LastInputRejected);
                foreach(var field in typeof(ForceModelSettings).GetFields())
                {
                    if(field.FieldType!=typeof(float)) continue;
                    var original=field.GetValue(settings); field.SetValue(settings,invalid);
                    Assert.Equal(0,m.Compute(hit,.02f)); Assert.True(m.LastInputRejected);
                    field.SetValue(settings,original);
                    Assert.Equal(.5f,m.Compute(hit,.02f)); Assert.False(m.LastInputRejected);
                    m.Reset();
                }
            }
        }
        [Fact] public void AbsentSignalMayBeNonFiniteButPresentSignalCannot()
        {
            var m=new ForceModel(new ForceModelSettings());
            var i=new ForceInputs {Steer=float.NaN,FrontLateralForce=float.NaN};
            Assert.Equal(0,m.Compute(i,.02f)); Assert.False(m.LastInputRejected);
            i.HasSteer=true; Assert.Equal(0,m.Compute(i,.02f)); Assert.True(m.LastInputRejected);
            i=new ForceInputs {Impact=1,ImpactDirection=float.NaN};
            Assert.Equal(0,m.Compute(i,.02f)); Assert.True(m.LastInputRejected);
        }
    }
}
