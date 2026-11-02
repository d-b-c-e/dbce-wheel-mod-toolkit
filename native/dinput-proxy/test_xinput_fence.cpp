// The XInput output fence (dbce_xinput_fence.hpp) against an accepting fake XInputSetState: open forwards the game's
// own arguments and result untouched; closed never lets a motor speed through, keeps the real connection result and
// never writes the game's struct. No real pad or XInput library is used. Built x64 and x86 by build_xinput_fence_test.ps1.
#include "dbce_xinput_fence.hpp"
#include <stdio.h>

static int g_checks, g_fail;
static void check(bool ok, const char *what) { ++g_checks; if (!ok) { ++g_fail; printf("FAIL %s\n", what); } }

static struct { int calls; DWORD user; XINPUT_VIBRATION *ptr; WORD left, right; bool sawNull; } g_fake;
static DWORD g_answer = ERROR_SUCCESS;
static DWORD WINAPI fakeSetState(DWORD user, XINPUT_VIBRATION *v)
{
    ++g_fake.calls;
    g_fake.user = user;
    g_fake.ptr = v;
    g_fake.sawNull = !v;
    g_fake.left = v ? v->wLeftMotorSpeed : 0;
    g_fake.right = v ? v->wRightMotorSpeed : 0;
    return g_answer;
}
static void reset(DWORD answer) { g_fake = {}; g_answer = answer; }

int main()
{
    using namespace dbce::xinput_fence;
    XINPUT_VIBRATION game = {40000, 12345};

    // Open: the same pointer, user index and speeds reach the original; its result comes back.
    reset(ERROR_SUCCESS);
    check(SetState(fakeSetState, false, 2, &game) == ERROR_SUCCESS, "open: result forwarded");
    check(g_fake.calls == 1 && g_fake.user == 2 && g_fake.ptr == &game, "open: same user and pointer");
    check(g_fake.left == 40000 && g_fake.right == 12345, "open: speeds forwarded");
    reset(ERROR_DEVICE_NOT_CONNECTED);
    check(SetState(fakeSetState, false, 3, &game) == ERROR_DEVICE_NOT_CONNECTED, "open: not-connected forwarded");
    reset(ERROR_BAD_ARGUMENTS);
    check(SetState(fakeSetState, false, 0, nullptr) == ERROR_BAD_ARGUMENTS && g_fake.sawNull, "open: null forwarded");

    // Closed: the original is called once with both motors at zero, never with the game's struct.
    LONG blocked0 = counters().blocked;
    reset(ERROR_SUCCESS);
    check(SetState(fakeSetState, true, 1, &game) == ERROR_SUCCESS, "closed: connected result kept");
    check(g_fake.calls == 1 && g_fake.user == 1, "closed: one call, same user");
    check(g_fake.ptr && g_fake.ptr != &game, "closed: the game's struct is not passed");
    check(g_fake.left == 0 && g_fake.right == 0, "closed: motors at rest");
    check(game.wLeftMotorSpeed == 40000 && game.wRightMotorSpeed == 12345, "closed: the game's struct is unchanged");
    check(counters().blocked == blocked0 + 1, "closed: non-zero request counted as blocked");
    reset(ERROR_DEVICE_NOT_CONNECTED);
    check(SetState(fakeSetState, true, 0, &game) == ERROR_DEVICE_NOT_CONNECTED, "closed: not-connected result kept");
    check(g_fake.left == 0 && g_fake.right == 0, "closed: motors at rest when disconnected");
    XINPUT_VIBRATION one = {0, 1};
    reset(ERROR_SUCCESS);
    SetState(fakeSetState, true, 0, &one);
    check(g_fake.right == 0 && g_fake.left == 0, "closed: the smallest speed is refused");
    LONG zeroed0 = counters().zeroed;
    XINPUT_VIBRATION stop = {};
    reset(ERROR_SUCCESS);
    check(SetState(fakeSetState, true, 0, &stop) == ERROR_SUCCESS && g_fake.calls == 1, "closed: a stop still reaches the pad");
    check(counters().zeroed == zeroed0 + 1, "closed: a stop counted as zeroed");
    reset(ERROR_BAD_ARGUMENTS);
    check(SetState(fakeSetState, true, 0, nullptr) == ERROR_BAD_ARGUMENTS && g_fake.sawNull, "closed: null forwarded");

    // No original (export missing): nothing is called.
    reset(ERROR_SUCCESS);
    check(SetState(nullptr, false, 0, &game) == ERROR_DEVICE_NOT_CONNECTED && g_fake.calls == 0, "missing original, open");
    check(SetState(nullptr, true, 0, &game) == ERROR_DEVICE_NOT_CONNECTED && g_fake.calls == 0, "missing original, closed");

    printf("%s: %d/%d xinput fence checks (%s)\n", g_fail ? "FAIL" : "PASS", g_checks - g_fail, g_checks, sizeof(void *) == 8 ? "x64" : "x86");
    return g_fail ? 1 : 0;
}
