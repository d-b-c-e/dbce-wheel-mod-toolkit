// MIT. Rectangle IMGUI adapter following iRacing Arcade's stripped IL2CPP renderer.
// No GUILayout, device, profile or file calls. Host owns its Unity callback and edit gates.
#nullable disable
#if DBCE_F6_IL2CPP
extern alias interop;
using interop::UnityEngine;
#else
using UnityEngine;
#endif
using System;

namespace Dbce.Wheel.Ui.Unity
{
    public sealed class RectBindingPanel : ISharedBindingPanelUi
    {
        private float x, y, width, scale;
        private GUIStyle label, help, button;
        public float Bottom { get { return y; } }

        public void Begin(float left, float top, float bodyWidth, float uiScale)
        {
            x = left; y = top; width = bodyWidth; scale = uiScale;
            if (label == null)
            {
                label = new GUIStyle(GUI.skin.label);
                help = new GUIStyle(GUI.skin.label);
                button = new GUIStyle(GUI.skin.button);
                label.wordWrap = help.wordWrap = button.wordWrap = true;
                label.normal.textColor = new Color(F6Palette.Text.R, F6Palette.Text.G, F6Palette.Text.B, 1);
                help.normal.textColor = new Color(F6Palette.Secondary.R, F6Palette.Secondary.G, F6Palette.Secondary.B, 1);
            }
            label.fontSize = (int)Math.Round(F6Metrics.BodySize * scale);
            help.fontSize = (int)Math.Round(F6Metrics.HelpSize * scale);
            button.fontSize = (int)Math.Round(F6Metrics.BodySize * scale);
        }

        private Rect Next(string text, int size, float minimum)
        {
            // Conservative wrapping estimate avoids a dependency on stripped CalcHeight.
            int columns = Math.Max(12, (int)(width / Math.Max(1, size * scale * .54f)));
            int lines = Math.Max(1, (text.Length + columns - 1) / columns);
            float height = Math.Max(minimum * scale, lines * (size + 4) * scale);
            var rect = new Rect(x, y, width, height);
            y += height + 5 * scale;
            return rect;
        }
        public void Label(string text) { GUI.Label(Next(text, F6Metrics.BodySize, 24), text, label); }
        public void Help(string text) { GUI.Label(Next(text, F6Metrics.HelpSize, 20), text, help); }
        public bool Button(string text, bool enabled = true)
        {
            bool prior = GUI.enabled;
            try { GUI.enabled = prior && enabled; return GUI.Button(Next(text, F6Metrics.BodySize, 36), text, button); }
            finally { GUI.enabled = prior; }
        }
    }
}
