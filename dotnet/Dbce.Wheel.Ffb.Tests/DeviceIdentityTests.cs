using System;
using Xunit;
namespace Dbce.Wheel.Ffb.Tests
{
    public class DeviceIdentityTests
    {
        [Fact] public void ExplicitIdentityNeverFallsBackOrResolvesAmbiguously()
        {
            var a = Guid.NewGuid(); var b = Guid.NewGuid();
            var devices = new[] {
                new WheelFfbNative.DeviceInfo { Name="Wheel", ForceFeedback=true, InstanceGuid=a, VendorId=0x346e, ProductId=6 },
                new WheelFfbNative.DeviceInfo { Name="Wheel", ForceFeedback=true, InstanceGuid=b, VendorId=0x346e, ProductId=6 }
            };
            Assert.Equal(-1, DevicePreference.Parse("missing").Choose(devices));
            Assert.Equal(-1, DevicePreference.Parse("Wheel").Choose(devices));
            Assert.Equal(-1, DevicePreference.Parse("346e:0006").Choose(devices));
            Assert.Equal(-1, DevicePreference.Parse("#2").Choose(devices));
            Assert.Equal(1, DevicePreference.Parse(b.ToString()).Choose(devices));
            Array.Reverse(devices);
            Assert.Equal(0, DevicePreference.Parse(b.ToString()).Choose(devices));
            Assert.Equal(-1, DevicePreference.Parse("346e:0024").Choose(devices));
            devices[0].VendorId = null;
            Assert.Equal(1, DevicePreference.Parse("346e:0006").Choose(devices));
            devices[1].ForceFeedback = false;
            Assert.Equal(-1, DevicePreference.Parse("#1").Choose(devices));
        }
    }
}
