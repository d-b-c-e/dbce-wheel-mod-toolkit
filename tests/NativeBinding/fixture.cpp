#include <windows.h>
#include <string.h>
static int fault,force,initCount,freeCount,active,hasName,hasGuid,strict,readCalls;
static int selectedIndex=-1;
static int povReadCalls;
static int constantCalls,constantMagnitude,constantDuration,constantStops,constantReleases;
static int shapeCalls,shapePhase,shapeFade;
static int burstCalls,burstMagnitude,burstFrequency,burstDuration,burstStops;
extern "C" {
__declspec(dllexport) int __cdecl InitDirectInput(int hwnd) { (void)hwnd; initCount++; active = fault == 1 ? 0 : 1; return active; }
__declspec(dllexport) void __cdecl Aquire(void) {  }
__declspec(dllexport) int __cdecl SetDeviceForcesXY(int x, int y) { (void)x; (void)y; force=x; return fault == 3 ? 0 : active; }
__declspec(dllexport) int __cdecl StartEffect(void) { return fault == 2 ? 0 : active; }
__declspec(dllexport) int __cdecl StopEffect(void) { return 1; }
__declspec(dllexport) int __cdecl SetAutoCenter(int enable) { (void)enable; return 1; }
__declspec(dllexport) void __cdecl FreeDirectInput(void) { active=0; freeCount++; }
__declspec(dllexport) void __cdecl SetPreferredDevice(const char* name) { (void)name; hasName=name && *name; }
__declspec(dllexport) void __cdecl SetPreferredDeviceGuid(const void* guid16) { (void)guid16; hasGuid=guid16 ? 1 : 0; }
__declspec(dllexport) void __cdecl SetPreferredDeviceIndex(int index) { (void)index; selectedIndex=index; }
__declspec(dllexport) int __cdecl EnumerateDevices(void) { return 2; }
__declspec(dllexport) int __cdecl GetDeviceName(int index, char* buffer, int size) { (void)index; (void)buffer; (void)size; strncpy_s(buffer,size,"same wheel",_TRUNCATE); return 1; }
__declspec(dllexport) int __cdecl EnumerateAllDevices(void) { return 2; }
__declspec(dllexport) int __cdecl GetAnyDeviceName(int index, char* buffer, int size) { (void)index; (void)buffer; (void)size; strncpy_s(buffer,size,"same wheel",_TRUNCATE); return 1; }
__declspec(dllexport) int __cdecl OpenAuxDevice(int index) { (void)index; return 1; }
__declspec(dllexport) int __cdecl ReadAuxButtons(unsigned char* buffer, int length) { (void)buffer; (void)length; if(length>0) buffer[0]=1; return length; }
__declspec(dllexport) void __cdecl CloseAuxDevice(void) {  }
__declspec(dllexport) int __cdecl OpenReadDevice(int index) { (void)index; return index; }
__declspec(dllexport) int __cdecl ReadDeviceState(int slot, int* axes, unsigned char* buttons, int buttonCount) { (void)slot; (void)axes; (void)buttons; (void)buttonCount; readCalls++; for(int i=0;i<8;i++) axes[i]=100+i; if(buttons && buttonCount>0) buttons[0]=1; return fault==4 ? 0 : 1; }
#if !defined(NO_POV) && !defined(NO_BURST) && !defined(NO_SHAPED) && !defined(NO_CONSTANT)
__declspec(dllexport) int __cdecl ReadDeviceStateWithPov(int slot,int* axes,int axisCount,unsigned char* buttons,int buttonCount,int* pov,int povCount) {
 (void)slot;povReadCalls++;
 if(axisCount!=8 || povCount!=4 || buttonCount<0 || buttonCount>128)return 0;
 for(int i=0;i<8;i++)axes[i]=200+i;
 for(int i=0;i<buttonCount;i++)buttons[i]=(i==0 || i==127)?1:0;
 for(int i=0;i<4;i++)pov[i]=i==3?-1:i*9000+4500;
 // Write poison and fail to verify managed failure clearing independently.
 return fault==12?0:1;
}
#endif
__declspec(dllexport) void __cdecl CloseReadDevices(void) {  }
__declspec(dllexport) int __cdecl GetDeviceInfo(int index, int* axes, int* buttons) { (void)index; (void)axes; (void)buttons; *axes=8; *buttons=128; return 1; }
__declspec(dllexport) int __cdecl GetDeviceGuid(int index, void* guid16) { (void)index; (void)guid16; memset(guid16,0,16); ((unsigned char*)guid16)[0]=(unsigned char)(index+1); return 1; }
__declspec(dllexport) int __cdecl GetAnyDeviceGuid(int index, void* guid16) { (void)index; (void)guid16; memset(guid16,0,16); ((unsigned char*)guid16)[0]=(unsigned char)(index+1); return 1; }
__declspec(dllexport) int __cdecl GetAnyDeviceInfo(int index, int* axes, int* buttons, int* ffb) { (void)index; (void)axes; (void)buttons; (void)ffb; *axes=8; *buttons=128; *ffb=1; return 1; }
__declspec(dllexport) void __cdecl PanicStop(void) { active=0; }
__declspec(dllexport) void __cdecl ZeroForces(void) { force=0; }
__declspec(dllexport) int __cdecl InstallExitGuards(void) { return 1; }
__declspec(dllexport) void __cdecl SetHoldTimeoutMs(int ms) { (void)ms;  }
__declspec(dllexport) void __cdecl SetLogPath(const char* path) { (void)path;  }
__declspec(dllexport) int __cdecl GetLastHResult(void) { return 1234; }
__declspec(dllexport) int __cdecl GetWheelFfbVersion(void) {
#ifdef NO_BURST
return 500;
#elif defined(NO_SHAPED)
return 601;
#elif defined(NO_CONSTANT)
return 700;
#elif defined(NO_POV)
return 800;
#else
return 900;
#endif
}
__declspec(dllexport) int __cdecl CreatePeriodicEffect(int freqHz) { (void)freqHz; return 1; }
__declspec(dllexport) int __cdecl UpdatePeriodicEffect(int slot, int magnitude, int freqMilliHz) { (void)slot; (void)magnitude; (void)freqMilliHz; return 1; }
__declspec(dllexport) void __cdecl ReleasePeriodicEffects(void) {  }
__declspec(dllexport) int __cdecl CreateConditionEffect(int type) { (void)type; return 1; }
__declspec(dllexport) int __cdecl UpdateConditionEffect(int slot, int coefficient, int saturation, int deadband, int offset) { (void)slot; (void)coefficient; (void)saturation; (void)deadband; (void)offset; return 1; }
__declspec(dllexport) void __cdecl ReleaseConditionEffects(void) {  }
__declspec(dllexport) void __cdecl SetStrictDeviceSelection(int enabled) { (void)enabled; strict=enabled; }
#if !defined(NO_BURST) && !defined(NO_SHAPED)
__declspec(dllexport) int __cdecl PlayShapedPeriodicBurst(int slot,int magnitude,int milliHz,int phase,int fade) {
 (void)slot; shapeCalls++; burstMagnitude=magnitude; burstFrequency=milliHz; shapePhase=phase; shapeFade=fade; return fault==8 || fade>burstDuration ? 0 : 1;
}
#endif
#ifndef NO_BURST
__declspec(dllexport) int __cdecl CreatePeriodicBurst(int hz,int ms) { burstFrequency=hz; burstDuration=ms; return fault==5 ? -1 : 0; }
__declspec(dllexport) int __cdecl PlayPeriodicBurst(int slot,int magnitude,int milliHz) { (void)slot; burstCalls++; burstMagnitude=magnitude; burstFrequency=milliHz; return fault==6 ? 0 : 1; }
__declspec(dllexport) int __cdecl StopPeriodicBurst(int slot) { (void)slot; burstStops++; return fault==7 ? 0 : 1; }
#endif
#if !defined(NO_BURST) && !defined(NO_SHAPED) && !defined(NO_CONSTANT)
__declspec(dllexport) int __cdecl CreateConstantBurst(int ms) {constantDuration=ms;return fault==9?-1:0;}
__declspec(dllexport) int __cdecl PlayConstantBurst(int slot,int magnitude) {(void)slot;constantCalls++;constantMagnitude=magnitude;return fault==10?0:1;}
__declspec(dllexport) int __cdecl StopConstantBurst(int slot) {(void)slot;constantStops++;return fault==11?0:1;}
__declspec(dllexport) void __cdecl ReleaseConstantBursts() {constantReleases++;}
#endif
__declspec(dllexport) void __cdecl TestFault(int value) { (void)value; fault=value; }
__declspec(dllexport) int __cdecl TestMetric(int key) {
 switch(key) {case 0:return initCount;case 1:return freeCount;case 2:return force;case 3:return hasName;case 4:return selectedIndex;case 5:return hasGuid;case 6:return strict;case 7:return readCalls;case 8:return burstCalls;case 9:return burstMagnitude;case 10:return burstFrequency;case 11:return burstDuration;case 12:return burstStops;case 13:return shapeCalls;case 14:return shapePhase;case 15:return shapeFade;case 16:return constantCalls;case 17:return constantMagnitude;case 18:return constantDuration;case 19:return constantStops;case 20:return constantReleases;case 21:return povReadCalls;default:return -1;}
}
}
