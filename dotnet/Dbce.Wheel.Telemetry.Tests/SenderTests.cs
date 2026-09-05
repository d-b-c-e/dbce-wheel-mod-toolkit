using System;
using System.Net;
using System.Net.Sockets;
using Dbce.Wheel.Telemetry;
using Xunit;

namespace Dbce.Wheel.Telemetry.Tests
{
    public class SenderTests
    {
        [Theory]
        [InlineData("127.0.0.1")]
        [InlineData("::1")]
        public void SendsToTheRequestedAddressFamily(string host)
        {
            var address = IPAddress.Parse(host);
            using var receiver = new UdpClient(new IPEndPoint(address, 0));
            receiver.Client.ReceiveTimeout = 2000;
            using var sender = new TelemetrySender(host, ((IPEndPoint)receiver.Client.LocalEndPoint).Port);
            Assert.True(sender.Send(new TelemetryFrame { IsRaceOn = true, Speed = 12.5f }), sender.LastError);
            IPEndPoint peer = null;
            Assert.Equal(ForzaPacket.Size, receiver.Receive(ref peer).Length);
            Assert.Equal(1, sender.PacketsSent);
            sender.Dispose();
            Assert.False(sender.Send(new TelemetryFrame()));
        }
    }
}
