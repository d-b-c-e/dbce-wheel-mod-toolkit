using System;
using Xunit;
namespace Dbce.Wheel.Ffb.Tests
{
    public class DeviceSelectionResultTests
    {
        [Fact] public void MissingSavedWheelIsDifferentFromEmptySetup()
        {
            Assert.Equal(DeviceSelectionStatus.NoDevices,new DevicePreference().Resolve(null).Status);
            var missing=DevicePreference.Parse(Guid.NewGuid().ToString()).Resolve(null);
            Assert.Equal(DeviceSelectionStatus.Missing,missing.Status);
            Assert.False(missing.Success); Assert.Null(missing.PersistentId); Assert.Null(missing.Device);
        }
        [Fact] public void PickerGetsStableIdentityAndSeparateNativeIndex()
        {
            var guid=Guid.NewGuid();
            var devices=new[]{new WheelFfbNative.DeviceInfo {Index=7,Name="Wheel",InstanceGuid=guid,ForceFeedback=true}};
            var result=DevicePreference.Parse(guid.ToString()).Resolve(devices);
            Assert.True(result.Success); Assert.Equal(0,result.ArrayPosition); Assert.Equal(7,result.Device.Value.Index);
            Assert.Equal(guid.ToString(),result.PersistentId); Assert.Equal(DeviceSelectionStatus.Matched,result.Status);
        }
        [Fact] public void AmbiguousUnsupportedAndVirtualChoicesHaveDistinctActions()
        {
            var devices=new[]{new WheelFfbNative.DeviceInfo {Name="Wheel",ForceFeedback=true},new WheelFfbNative.DeviceInfo {Name="Wheel"}};
            Assert.Equal(DeviceSelectionStatus.Ambiguous,DevicePreference.Parse("Wheel").Resolve(devices).Status);
            Assert.Equal(DeviceSelectionStatus.NoForceFeedback,DevicePreference.Parse("#1").Resolve(devices).Status);
            devices[0].Name="vJoy Wheel";
            Assert.Equal(DeviceSelectionStatus.VirtualDevice,DevicePreference.Parse("#0").Resolve(devices).Status);
            Assert.Equal(-1,DevicePreference.Parse("#0").Choose(devices));
            Assert.False(new DevicePreference().Resolve(devices).Success);
        }
    }
}
