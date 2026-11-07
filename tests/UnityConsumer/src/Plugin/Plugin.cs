// Dummy plugin: proves that Dbce.Unity.props set the defines the test asked for, and that the shared layer's types
// resolve on that runtime (compiled-in sources on Mono20, the netstandard2.0 DLLs on Mono and Il2Cpp).
using System;
using Dbce.Unity;
using Dbce.Wheel.Display;
using Dbce.Wheel.Input;
using Dbce.Wheel.Support;
using Dbce.Wheel.Ui;

#if EXPECT_IL2CPP && (!DBCE_IL2CPP || DBCE_NET35)
#error Il2Cpp build without DBCE_IL2CPP, or with DBCE_NET35
#endif
#if EXPECT_MONO20 && (!DBCE_NET35 || !DBCE_INPUT_NET35 || !DBCE_SUPPORT_NET35 || DBCE_IL2CPP)
#error Mono20 build without DBCE_NET35, DBCE_INPUT_NET35 and DBCE_SUPPORT_NET35
#endif
#if EXPECT_MONO && (DBCE_IL2CPP || DBCE_NET35 || DBCE_INPUT_NET35 || DBCE_SUPPORT_NET35)
#error Mono 4.x build with another runtime's defines
#endif
#if EXPECT_IMGUI_RECT != DBCE_IMGUI_RECT
#error DbceImgui and DBCE_IMGUI_RECT disagree
#endif
#if EXPECT_PIPELINE_URP != DBCE_URP
#error DbcePipeline and DBCE_URP disagree
#endif

namespace DummyPlugin
{
    public static class Plugin
    {
        public static readonly ModDescriptor Descriptor = new ModDescriptor
        {
            Id = "dummy",
            Name = "Dummy consumer",
#if EXPECT_IL2CPP
            Runtime = UnityRuntime.Il2Cpp,
#elif EXPECT_MONO20
            Runtime = UnityRuntime.Mono20,
#else
            Runtime = UnityRuntime.Mono,
#endif
#if EXPECT_IMGUI_RECT
            Imgui = ImguiRoute.Rect,
#endif
#if EXPECT_PIPELINE_URP
            Pipeline = RenderPipeline.Urp,
#endif
            Settings = SettingsHost.BepInEx,
        };

        // One type from each engine-free library: Input, Display, Ui and the problem report.
        public static readonly Type[] Shared =
        {
            typeof(ControlBinding), typeof(SharedBindingCaptureFlow), typeof(RacingRowPolicy), typeof(WindowLandingPolicy),
            typeof(F6Palette), typeof(SharedBindingPanel), typeof(BindingSaveStatus), typeof(SupportReport),
        };

        public static string Check()
        {
            string mismatch = Descriptor.MismatchWithBuild();
            if (mismatch != null) return mismatch;
            ControlBinding binding;
            string reason;
            return ControlBinding.TryParse("bogus", out binding, out reason) ? "parsed a bogus binding" : null;
        }
    }
}
