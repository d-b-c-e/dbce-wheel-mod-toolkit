// Fixtures for dbce_renderer_config.hpp: the triple renderer's config.ini read and written by the F6 panel.
#include "dbce_renderer_config.hpp"
#include <stdio.h>

static int g_checks, g_failed;
static void check(bool ok, const char *what) { ++g_checks; if (!ok) { ++g_failed; printf("FAIL %s\n", what); } }

int main()
{
    using namespace dbce::renderer;
    const std::string rig =
        "# Rig geometry for the triple renderer (mm, degrees).\r\n"
        "Triple = Auto\r\n"
        "PanelWidthMm = 708.4\r\n"
        "SideAngle = 70\r\n"
        "BezelMm = 8\r\n";
    check(TopValue(rig, "Triple") == "Auto" && TopValue(rig, "sideangle") == "70" && TopValue(rig, "Screens").empty(), "values, case-insensitive keys");
    check(TopValue("# Triple = Off\r\nTriple = On\r\n", "Triple") == "On", "comments are not keys");
    check(TopValue("Triple = Auto\r\n[Panel]\r\nTriple = Off\r\n", "Triple") == "Auto", "keys after a section are not the renderer's");
    check(TopValue("Triple = Auto\r\nTriple = Off\r\n", "Triple") == "Off", "the last one counts, as the renderer reads it");

    // Set in place, keep every other line and the CRLF line ends.
    check(SetTopValue(rig, "SideAngle", "0") == "# Rig geometry for the triple renderer (mm, degrees).\r\nTriple = Auto\r\nPanelWidthMm = 708.4\r\nSideAngle = 0\r\nBezelMm = 8\r\n",
          "replaced in place");
    // A new key goes after the last key line, before the panel's section.
    const std::string withPanel = rig + "\r\n[Panel]\r\nView = Simple\r\n";
    check(SetTopValue(withPanel, "Screens", "Surround") ==
              "# Rig geometry for the triple renderer (mm, degrees).\r\nTriple = Auto\r\nPanelWidthMm = 708.4\r\nSideAngle = 70\r\nBezelMm = 8\r\nScreens = Surround\r\n\r\n[Panel]\r\nView = Simple\r\n",
          "added after the last key, before [Panel]");
    check(SetTopValue("[Panel]\nView = Simple\n", "Triple", "Off") == "Triple = Off\n[Panel]\nView = Simple\n", "no top keys: before the first section, LF kept");
    check(SetTopValue("", "Triple", "Off") == "Triple = Off\r\n", "empty file");
    check(SetTopValue("Triple = Auto", "Triple", "Off") == "Triple = Off\r\n", "no final line end: one is added");
    check(SetTopValue("Triple = Auto\r\nTriple = On\r\n", "Triple", "Off") == "Triple = Off\r\nTriple = Off\r\n", "every copy replaced");

    // STD-022: Off / Surround / Separate monitors over the renderer's Triple = Auto | On | Off.
    check(ScreensOf(rig, true) == 2 && ScreensOf(rig, false) == 1, "no Screens: the connected layout decides");
    check(ScreensOf("Triple = Off\r\nScreens = Surround\r\n", true) == 0, "Off wins");
    check(ScreensOf("Triple = Auto\r\nScreens = Surround\r\n", true) == 1 && ScreensOf("Triple = Auto\r\nScreens = Separate monitors\r\n", false) == 2,
          "the player's choice is kept");
    std::string t = WithScreens(rig, 0);
    check(TopValue(t, "Triple") == "Off" && TopValue(t, "Screens").empty() && TopValue(t, "PanelWidthMm") == "708.4", "Off writes Triple = Off only");
    t = WithScreens(t, 1);
    check(TopValue(t, "Triple") == "Auto" && TopValue(t, "Screens") == "Surround", "Surround: Auto + Screens");
    t = WithScreens(t, 2);
    check(TopValue(t, "Triple") == "Auto" && TopValue(t, "Screens") == "Separate monitors" && ScreensOf(t, false) == 2, "Separate monitors");
    t = WithScreens("Triple = On\r\n", 1);
    check(TopValue(t, "Triple") == "On", "a forced On stays On");

    if (g_failed) printf("FAIL %d of %d config checks\n", g_failed, g_checks);
    else printf("PASS %d config checks\n", g_checks);
    return g_failed ? 1 : 0;
}
