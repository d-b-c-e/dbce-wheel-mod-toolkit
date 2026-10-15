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
        private int control, pressed = -1;
        private GUIStyle label, help, button;
        public float Bottom { get { return y; } }

        public void Begin(float left, float top, float bodyWidth, float uiScale)
        {
            x = left; y = top; width = bodyWidth; scale = uiScale;
            control = 0;
            if (Event.current.type == EventType.MouseUp || !GUI.enabled) pressed = -1;
            if (label == null)
            {
                // The current Truckful IL2CPP proxy has no GUIStyle copy ctor.
                // Own plain styles instead of aliasing or mutating GUI.skin.
                label = new GUIStyle();
                help = new GUIStyle();
                button = new GUIStyle();
                button.alignment = TextAnchor.MiddleCenter;
                label.wordWrap = help.wordWrap = button.wordWrap = true;
                label.normal.textColor = new Color(F6Palette.Text.R, F6Palette.Text.G, F6Palette.Text.B, 1);
                help.normal.textColor = new Color(F6Palette.Secondary.R, F6Palette.Secondary.G, F6Palette.Secondary.B, 1);
                // Defensively clear every style background, including selected/
                // pressed/focused states. Flat fills below own the presentation.
                foreach (var state in new[] { button.normal, button.hover, button.active, button.focused,
                    button.onNormal, button.onHover, button.onActive, button.onFocused }) state.background = null;
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
            Color priorColor = GUI.color;
            int id = control++;
            var rect = Next(text, F6Metrics.BodySize, 36);
            bool active = prior && enabled;
            bool hover = active && rect.Contains(Event.current.mousePosition);
            if (hover && Event.current.type == EventType.MouseDown && Event.current.button == 0) pressed = id;
            var fill = hover ? pressed == id ? F6Palette.Selected : F6Palette.ButtonHover : F6Palette.Button;
            var foreground = active ? F6Palette.Text : F6Palette.Disabled;
            try
            {
                GUI.enabled = true;
                GUI.color = new Color(F6Palette.ButtonBorder.R, F6Palette.ButtonBorder.G, F6Palette.ButtonBorder.B, 1);
                GUI.DrawTexture(rect, Texture2D.whiteTexture);
                GUI.color = new Color(fill.R, fill.G, fill.B, active ? 1 : .65f);
                GUI.DrawTexture(new Rect(rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2), Texture2D.whiteTexture);
                foreach (var state in new[] { button.normal, button.hover, button.active, button.focused,
                    button.onNormal, button.onHover, button.onActive, button.onFocused })
                    state.textColor = new Color(foreground.R, foreground.G, foreground.B, 1);
                GUI.color = Color.white; GUI.enabled = active;
                return GUI.Button(rect, text, button);
            }
            finally { GUI.enabled = prior; GUI.color = priorColor; }
        }
    }
}
