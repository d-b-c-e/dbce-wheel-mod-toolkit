// MIT. Shared Unity layer, skeleton: the descriptor a game declares, and the runtime its build was compiled for.
// Engine-free, no tuples or LINQ, so it compiles on Mono 2.0, Mono 4.x and IL2CPP plugins alike. See unity/README.md.
#nullable disable
namespace Dbce.Unity
{
    public enum UnityRuntime { Mono, Mono20, Il2Cpp }
    public enum ImguiRoute { Layout, Rect }
    public enum RenderPipeline { BuiltIn, Urp }
    public enum SettingsHost { BepInEx, Umm }

    /// <summary>What a game is: its ids and its build choices. Adapters and hooks stay in the game.</summary>
    public sealed class ModDescriptor
    {
        public string Id = "";
        public string Name = "";
        public UnityRuntime Runtime;
        public ImguiRoute Imgui;
        public RenderPipeline Pipeline;
        public SettingsHost Settings;

        /// <summary>Null when the descriptor matches the defines Dbce.Unity.props compiled this plugin with.</summary>
        public string MismatchWithBuild()
        {
            if (Runtime != DbceBuild.Runtime) return "descriptor runtime " + Runtime + ", build " + DbceBuild.Runtime;
            if (Imgui != DbceBuild.Imgui) return "descriptor IMGUI " + Imgui + ", build " + DbceBuild.Imgui;
            if (Pipeline != DbceBuild.Pipeline) return "descriptor pipeline " + Pipeline + ", build " + DbceBuild.Pipeline;
            return null;
        }
    }

    /// <summary>The choices this plugin was compiled with, from the Dbce.Unity.props defines.</summary>
    public static class DbceBuild
    {
#if DBCE_IL2CPP
        public const UnityRuntime Runtime = UnityRuntime.Il2Cpp;
#elif DBCE_NET35
        public const UnityRuntime Runtime = UnityRuntime.Mono20;
#else
        public const UnityRuntime Runtime = UnityRuntime.Mono;
#endif
#if DBCE_IMGUI_RECT
        public const ImguiRoute Imgui = ImguiRoute.Rect;
#else
        public const ImguiRoute Imgui = ImguiRoute.Layout;
#endif
#if DBCE_URP
        public const RenderPipeline Pipeline = RenderPipeline.Urp;
#else
        public const RenderPipeline Pipeline = RenderPipeline.BuiltIn;
#endif
    }
}
