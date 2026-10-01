using Dbce.Wheel.Ffb;
using System.Globalization;
using System.Security.Cryptography;
using System.Text.Json;
#if WODEN_REFERENCE
using WodenRallyEdge.Core;
#endif
if(args.Length<1)throw new Exception("Provide a new evidence directory, optional immutable Woden case and Art CSV.");
string output=Path.GetFullPath(args[0]);if(Directory.Exists(output))throw new IOException("Evidence directory exists");Directory.CreateDirectory(output);
int checks=0;void Check(bool value,string reason){if(!value)throw new Exception(reason);checks++;}
string Hash(string p)=>Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(p))).ToLowerInvariant();
var seriesRows=new List<object>();int series=0,parityRows=0;
using(var csv=new StreamWriter(Path.Combine(output,"managed-native.csv"))){
 csv.WriteLine("series,start,end,structural,amplitude,direction,pulse,target,command,legacyUnbudgeted");
 foreach(int hz in new[]{20,30,50,60,120,250,1000})foreach(int phase in Enumerable.Range(0,16))foreach(float structural in new[]{0f,.2f,.25f})foreach(float direction in new[]{-1f,1f}){
  double dt=1.0/hz,start=1-dt+phase*dt/16,area=0,absoluteArea=0,maxSlew=0,oldPeak=0,newPeak=0,pulsePeak=0;
  var current=new CadenceImpactMixer();var legacy=new ImpactMixer();current.Trigger(1,direction,1,true);legacy.Trigger(1,direction,1);float previous=0;
  for(int i=0;i<(int)Math.Ceiling(.4/dt)+2;i++){
   double end=start+dt;float command=current.Mix(structural,start,end,true,true),old=legacy.Mix(structural,end);
   Check(!current.LastInputRejected&&Math.Abs(command)<=.25,"Final Woden budget violated");
   Check(Math.Abs(command-previous)<=1.5*dt+1e-6,"Final mixed slew violated");
   maxSlew=Math.Max(maxSlew,Math.Abs(command-previous)/dt);area+=current.LastIntervalPulse*dt;absoluteArea+=Math.Abs(current.LastIntervalPulse)*dt;
   oldPeak=Math.Max(oldPeak,Math.Abs(old));newPeak=Math.Max(newPeak,Math.Abs(command));pulsePeak=Math.Max(pulsePeak,direction*current.LastIntervalPulse);
   csv.WriteLine(string.Join(",",new[]{(double)series,start,end,structural,1,direction,current.LastIntervalPulse,current.LastTarget,command,old}.Select(x=>x.ToString("R",CultureInfo.InvariantCulture))));
   parityRows++;previous=command;start=end;
  }
  Check(pulsePeak>0&&Math.Abs(area)<1e-8,"Positive lobe/area regression");
  seriesRows.Add(new{hz,phaseFraction=phase/16.0,structural,direction,peakIntervalPulse=pulsePeak,legacyUnbudgetedPeak=oldPeak,newBoundedPeak=newPeak,maxSlew,signedIntervalArea=area,absoluteIntervalArea=absoluteArea});series++;
 }
}
object? recordedWoden=null,recordedArt=null;
#if WODEN_REFERENCE
if(args.Length>1){
 string root=args[1];using var manifest=JsonDocument.Parse(File.ReadAllText(Path.Combine(root,"case.json")));var source=manifest.RootElement.GetProperty("source");
 string src=Path.Combine(root,source.GetProperty("path").GetString()!);Check(Hash(src)==source.GetProperty("sha256").GetString(),"Woden source identity differs");
 var artifacts=manifest.RootElement.GetProperty("artifacts");
 foreach(string name in new[]{"config","profile"}){var artifact=artifacts.GetProperty(name);Check(Hash(Path.Combine(root,artifact.GetProperty("path").GetString()!))==artifact.GetProperty("sha256").GetString(),"Woden config/profile identity differs");}
 using var config=JsonDocument.Parse(File.ReadAllText(Path.Combine(root,"force-config.json")));var c=config.RootElement;
 var options=new ForceOptions(c.GetProperty("strengthPercent").GetSingle(),c.GetProperty("peakPercent").GetSingle(),c.GetProperty("loadReference").GetSingle(),c.GetProperty("slipScale").GetSingle(),c.GetProperty("smoothingMs").GetSingle(),c.GetProperty("damping").GetSingle(),c.GetProperty("invert").GetBoolean());
 var model=new ForceSignal();var mixed=new CadenceImpactMixer(options.PeakPercent/100,1.5f,.25f);
 long? previousReset=null;double? previousOutput=null;int rows=0,deltas=0,safetyZeros=0;double maxPreviewError=0,maxDelta=0;var changedRows=new List<object>();double? previousDelivered=null;
 foreach(var line in File.ReadLines(src)){
  using var record=JsonDocument.Parse(line);if(!record.RootElement.TryGetProperty("sample",out var sampleRecord))continue;var channels=sampleRecord.GetProperty("channels");
  if(!channels.TryGetProperty("ffb.modelResetBefore",out var beforeElement))continue;
  long before=beforeElement.GetInt64(),after=channels.GetProperty("ffb.modelResetAfter").GetInt64();
  if(previousReset.HasValue&&before!=previousReset){model.Reset();mixed.Reset();previousOutput=null;previousDelivered=null;}
  double sim=channels.GetProperty("sample.simulationSeconds").GetDouble();double outputTime=args.Length>3&&args[3]=="simulation"?sim:sampleRecord.GetProperty("elapsedSeconds").GetDouble();
  var sample=new TelemetrySample{SimulationSeconds=sim,State=channels.GetProperty("sample.driving").GetDouble()==1?"driving":"inactive",Discontinuity=channels.GetProperty("sample.discontinuity").GetDouble()==1?"recorded-discontinuity":null};
  foreach(var p in channels.EnumerateObject())sample.Channels[p.Name]=p.Value.GetDouble();
  var actual=model.Evaluate(sample,options);double baseline=channels.GetProperty("ffb.preview").GetDouble();maxPreviewError=Math.Max(maxPreviewError,Math.Abs(actual.Preview-baseline));Check(Math.Abs(actual.Preview-baseline)<2e-6,"Existing Woden model changed");
  bool active=channels.GetProperty("ffb.gate").GetInt32()==0;
  double start=previousOutput??Math.Max(0,outputTime-.02);float candidate=mixed.Mix(actual.Preview,start,outputTime,active,false);
  double deliveredBaseline=active?channels.GetProperty("ffb.sent").GetDouble():0;double delta=Math.Abs(candidate-deliveredBaseline);
  if(delta>2e-6){deltas++;changedRows.Add(new{sequence=sampleRecord.GetProperty("sequence").GetInt64(),simulationSeconds=sim,outputSeconds=outputTime,intervalStart=start,baseline=deliveredBaseline,candidate,delta,baselineOutputClockSlew=previousDelivered.HasValue&&outputTime>start?Math.Abs(deliveredBaseline-previousDelivered.Value)/(outputTime-start):(double?)null,gate=channels.GetProperty("ffb.gate").GetInt32(),speedMps=channels.TryGetProperty("motion.speed",out var speed)?speed.GetDouble():(double?)null,alignment=actual.Alignment,damping=actual.Damping});}if(active&&mixed.LastInputRejected)safetyZeros++;
  Check(Math.Abs(candidate)<=options.PeakPercent/100+1e-6,"Recorded no-event candidate exceeded unchanged cap");
  maxDelta=Math.Max(maxDelta,delta);if(!active){model.Reset();previousOutput=null;}else previousOutput=outputTime;
  previousDelivered=active?deliveredBaseline:(double?)null;previousReset=after;rows++;
 }
 recordedWoden=new{outputClock=args.Length>3&&args[3]=="simulation"?"simulation (comparison only)":"recorded monotonic sample elapsedSeconds (native call timestamp not separately recorded)",sourceSha256=Hash(src),configSha256=Hash(Path.Combine(root,"force-config.json")),profileSha256=Hash(Path.Combine(root,"capture-profile.json")),modelRows=rows,maxExistingPreviewError=maxPreviewError,proposedNoEventBoundedRouteChangedRows=deltas,maxProposedCommandDelta=maxDelta,staleIntervalSafetyZeros=safetyZeros,collisionEvents=0,changedRows,scope="Actual ForceSignal unchanged; stricter opt-in final budget/interval path assessed, not adopted"};
}
#endif
if(args.Length>2){
 Check(Hash(args[2])=="6f99f10a5828885c761919a514321236b4fb64d4ee1a85b973716f3ca2cf910d","Art immutable source identity differs");
 int rows=0;double error=0;foreach(string line in File.ReadLines(args[2]).Skip(1)){
  var a=line.Split(',').Select(x=>double.Parse(x,CultureInfo.InvariantCulture)).ToArray();float target=AxleForceCurve.Normalised((float)a[1],(float)a[2],(float)a[3],(float)a[4],(float)a[5],(float)a[6],a[7]!=0);
  float actual=AxleForceCurve.Smooth((float)a[9],target,(float)a[8]);error=Math.Max(error,Math.Abs(actual-a[10]));Check(Math.Abs(actual-a[10])<2e-6,"Art compatibility baseline changed");rows++;
 }
 recordedArt=new{rows,sourceSha256=Hash(args[2]),maximumBaselineError=error,collisionOutputCompared=false,scope="Actual released steering curve only; Art finite120ms crash waveform unchanged and not replaced"};
}
var evidence=new{kind="shared-cadence-offline-review",checks,series,parityRows,baselineApiChanged=false,collisionMappingActivated=false,physicalOutput=false,
 sharedArtifactSha256=Hash(typeof(CadenceImpactMixer).Assembly.Location),recordedWoden,recordedArt,seriesRows};
File.WriteAllText(Path.Combine(output,"evidence.json"),JsonSerializer.Serialize(evidence,new JsonSerializerOptions{WriteIndented=true}));
Console.WriteLine($"PASS cadence evidence: {checks} checks, {series} rate/phase/sign/bias series, {parityRows} managed/native rows; no devices.");
