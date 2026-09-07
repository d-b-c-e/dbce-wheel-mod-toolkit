using Xunit;
namespace Dbce.Wheel.Ffb.Tests
{
    public class DeliveryDiagnosticsTests
    {
        [Fact] public void ComputationNeverClaimsDeliveryAndRecoveryClearsLatestError()
        {
            var d=new FfbDiagnostics(); d.RecordOutput(100,40,.2f,1000,50,0);
            Assert.Contains("force samples computed : 1",d.Summary());
            Assert.Contains("not reported",d.Summary()); Assert.Null(d.LastDeliverySucceeded);
            d.RecordDelivery(false,"Wheel unavailable");
            Assert.Contains("0 accepted, 1 failed",d.Summary()); Assert.Contains("Wheel unavailable",d.Summary());
            d.RecordDelivery(true); Assert.Null(d.LastDeliveryError); Assert.True(d.LastDeliverySucceeded);
            Assert.Contains("1 accepted, 1 failed",d.Summary());
            d.Reset(); Assert.Equal(0,d.DeliveryAttempts); Assert.Equal(0,d.DeliveryFailures);
            Assert.Null(d.LastDeliverySucceeded); Assert.Contains("no force samples",d.Summary());
        }
    }
}
