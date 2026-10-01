using System;
using Xunit;
namespace Dbce.Wheel.Ffb.Tests
{
    public class DigitalInputTests
    {
        [Fact] public void SharedPressIsDeliveredOnceToEachActionAndReleased()
        {
            var id=Guid.NewGuid();var edges=new DigitalInputEdges();var current=new byte[160];var previous=new byte[160];
            current[5]=1;edges.Observe(id,current,previous);
            Assert.True(edges.Consume(id,5,"Confirm"));Assert.True(edges.Consume(id,5,"Camera"));
            Assert.False(edges.Consume(id,5,"Confirm"));Assert.False(edges.Consume(id,5,"Camera"));
            edges.Observe(id,current,current);Assert.False(edges.Consume(id,5,"Camera"));
            edges.Observe(id,previous,current);Assert.False(edges.Consume(id,5,"Confirm"));
            edges.Observe(id,current,previous);Assert.True(edges.Consume(id,5,"Camera"));
            edges.Disconnect(id);Assert.False(edges.Consume(id,5,"Confirm"));
        }
        [Fact] public void HatIndicesDoNotAliasPhysicalButtonsAndClearSuppressesEdges()
        {
            var id=Guid.NewGuid();var e=new DigitalInputEdges();var b=new byte[160];b[127]=b[128]=b[159]=1;
            e.Observe(id,b,new byte[160]);Assert.True(e.Consume(id,127,"physical"));Assert.True(e.Consume(id,128,"up"));Assert.True(e.Consume(id,159,"diagonal"));
            e.Clear();Assert.False(e.Consume(id,128,"up"));
            Assert.Equal("HAT 1 Up",DigitalInput.Label(128));Assert.Equal("HAT 4 Up-left",DigitalInput.Label(159));
            Assert.False(DigitalInput.Valid(-1));Assert.False(DigitalInput.Valid(160));
        }
    }
}
