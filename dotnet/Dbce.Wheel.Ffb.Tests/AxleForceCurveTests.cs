using System;
using System.Globalization;
using System.IO;
using Dbce.Wheel.Ffb;
using Xunit;
namespace Dbce.Wheel.Ffb.Tests {
 public class AxleForceCurveTests {
  [Fact] public void ReleasedVectorRemainsExactAtDevicePrecision() {
   int rows=0;
   foreach(var line in File.ReadAllLines(Path.Combine(AppContext.BaseDirectory,"data","art-of-rally-0.2.2-force-curve.csv"))) {
    if(string.IsNullOrWhiteSpace(line)) break;
    if(line.StartsWith("#") || line.StartsWith("fy_total_n")) continue;
    var cells=line.Split(','); Assert.Equal(7,cells.Length);
    var p=Array.ConvertAll(cells,s=>float.Parse(s,CultureInfo.InvariantCulture));
    var force=AxleForceCurve.Normalised(p[0],p[1],p[2],p[3],11500,1,false);
    Assert.InRange(Math.Abs(force-p[5]),0,.000001f);
    Assert.Equal((int)p[6],(int)(force*10000)); rows++;
   }
   Assert.Equal(640,rows);
  }
  [Fact] public void ClippingPrecedesFilteringAndFullHistoryWeightIsSupported() {
   float first=AxleForceCurve.Smooth(0,AxleForceCurve.Normalised(23000,0,8.5f,40,11500,1,false),.2f);
   Assert.Equal(.8f,first,6);Assert.Equal(.16f,AxleForceCurve.Smooth(first,0,.2f),6);
   Assert.Equal(first,AxleForceCurve.Smooth(first,-1,1),6);
   Assert.Equal(1,AxleForceCurve.CompatibilityVersion);
  }
 }
}
