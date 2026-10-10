using System.Runtime.InteropServices;
using System.Text;
using Dbce.Wheel.Input;

int checks=0;
void Check(bool value,string why) { checks++; if(!value)throw new Exception(why); }
const string raw="inject raw axis 5 dev={00000000-0000-0000-0000-000000000001} value=1743 ms=500";
void Reply(byte[] buffer,string text) { var b=Encoding.UTF8.GetBytes(text+"\0"); Array.Copy(b,buffer,b.Length); }
var absent=NativeTestInjectionClient.FromResidentExports(null);
Check(!absent.Available && !absent.BlockAssignments,"missing is inert");
Check(!absent.Arm() && absent.BlockAssignments && !absent.Armed,"missing arm blocks assignment");
Check(!absent.Arm(),"missing remains refused");
for(int missing=0;missing<4;missing++) {
    int index=0;
    NativeTestInjectionClient.EnableCall enable=_=>1;
    NativeTestInjectionClient.SubmitCall submit=(c,r,n)=>1;
    NativeTestInjectionClient.StatusCall status=(r,n)=>1;
    NativeTestInjectionClient.InjectedCall injected=_=>1;
    var delegates=new Delegate[]{enable,submit,status,injected};
    var c=NativeTestInjectionClient.FromResidentExports(_=>index++==missing?IntPtr.Zero:Marshal.GetFunctionPointerForDelegate(delegates[index-1]));
    Check(!c.Available && !c.Arm() && c.BlockAssignments,"partial ABI never arms");
    GC.KeepAlive(delegates);
}
int enables=0,submits=0,reads=0; bool nativeArmed=false; int delivered=0;
var client=new NativeTestInjectionClient(flags=>{Check(flags==1,"flags exact");enables++;nativeArmed=true;return 1;},
    (bytes,why,size)=>{submits++;Check(bytes[bytes.Length-1]==0,"null terminated command");Check(Encoding.UTF8.GetString(bytes,0,bytes.Length-1)==raw,"raw bytes unchanged");Reply(why,"accepted");return 1;},
    (bytes,size)=>{Check(size==bytes.Length,"status buffer size");Reply(bytes,nativeArmed?"armed: no force":"off");return nativeArmed?1:0;},
    slot=>{reads++;Check(slot==3,"same slot");return delivered;});
Check(!client.SubmitRaw(raw,out _) && submits==0,"no submit before arm");
Check(!client.LastReadInjected(3) && reads==0,"no read evidence before arm");
Check(client.Arm() && client.Armed && client.BlockAssignments,"confirmed latch");
Check(client.Arm() && enables==1,"never re-enable");
Check(client.SubmitRaw(raw,out var reason) && reason=="accepted" && submits==1,"accepted not delivered");
Check(!client.LastReadInjected(3),"zero delivery"); delivered=1; Check(client.LastReadInjected(3),"actual delivery");
foreach(var bad in new[]{"inject action brake value=1 ms=500",raw+"\0",raw+"\n",raw+"\t",raw+new string('x',1024),raw+"\ud800",null,""})
    Check(!client.SubmitRaw(bad,out _) && client.Armed && submits==1,"bad command not submitted or disarming");
Check(!client.LastReadInjected(-1) && reads==2,"invalid slot not queried");
nativeArmed=false; Check(!client.RefreshStatus() && !client.Armed && client.BlockAssignments,"lost latch is terminal");
nativeArmed=true; Check(!client.Arm() && !client.SubmitRaw(raw,out _) && enables==1 && submits==1,"cannot resume uncertain test");
foreach(int returned in new[]{0,-1,2}) {
    int attempted=0;
    var c=new NativeTestInjectionClient(_=>{attempted++;return returned;},(x,r,n)=>1,(r,n)=>{Reply(r,"refused");return 0;},_=>1);
    Check(!c.Arm() && c.BlockAssignments && !c.Arm() && attempted==1,"refused latch sticky");
}
for(int mode=0;mode<5;mode++) {
    int m=mode; int calls=0;
    var c=new NativeTestInjectionClient(_=>1,(x,r,n)=>1,(r,n)=>{
        calls++;
        if(m==0)throw new IOException("uncertain");
        if(m==1)return 1; // no terminator; buffer was poisoned
        if(m==2){r[0]=0xc0;r[1]=0;return 1;} // invalid UTF8
        Reply(r,"unknown");return m==3?2:0;
    },_=>1);
    Check(!c.Arm() && c.BlockAssignments && !c.Armed,"malformed status terminal");
    Check(!c.Arm() && calls==1,"malformed status not retried");
}
for(int mode=0;mode<4;mode++) {
    int m=mode; int calls=0;
    var c=new NativeTestInjectionClient(_=>1,(x,r,n)=>{
        calls++; if(m==0)throw new IOException("partial submit");
        if(m==1)return 1;
        Reply(r,"reply");return m==2?-1:0;
    },(r,n)=>{Reply(r,"armed");return 1;},_=>1);
    Check(c.Arm(),"submission test armed"); Check(!c.SubmitRaw(raw,out _),"bad/refused submission not accepted");
    Check(c.Armed==(m==3) && c.BlockAssignments,"refusal vs unknown result");
    if(m!=3)Check(!c.SubmitRaw(raw,out _) && calls==1,"unknown submit not retried");
}
foreach(int deliveredResult in new[]{-1,2}) {
    var c=new NativeTestInjectionClient(_=>1,(x,r,n)=>1,(r,n)=>{Reply(r,"armed");return 1;},_=>deliveredResult);
    Check(c.Arm() && !c.LastReadInjected(2) && !c.Armed && c.BlockAssignments,"bad delivery result cannot resume");
}
var throws=new NativeTestInjectionClient(_=>throw new IOException("uncertain enable"),(x,r,n)=>1,(r,n)=>1,_=>1);
Check(!throws.Arm() && throws.BlockAssignments,"throw after enable is sticky");
int boundCalls=0;
var bounded=new NativeTestInjectionClient(_=>1,(x,r,n)=>{boundCalls++;Reply(r,"ok");Check(x.Length==513,"512 printable bytes plus NUL");return 1;},(r,n)=>{Reply(r,"armed");return 1;},_=>0);
Check(bounded.Arm(),"bounds client armed");
string maximum="inject raw "+new string('x',512-11);
Check(bounded.SubmitRaw(maximum,out _),"exact native length limit");
foreach(var bad in new[]{maximum+"x",raw+"é",raw+"\u007f"})Check(!bounded.SubmitRaw(bad,out _) && boundCalls==1,"length/non-ASCII refused before native");
Console.WriteLine($"PASS {checks} managed test-injection checks; fake delegates only, no module/device/game opened.");
