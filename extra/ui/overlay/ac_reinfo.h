/* [extra] Read-only "RE Info" ImGui window: live values for every reverse
 * engineered gameplay slot (area, boss, mission, playtime, difficulty) plus
 * the address map and the id->name tables extracted from the game text.
 * Pure C ABI so the C overlay driver can call it without C++ headers. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Draws the window for the current ImGui frame. No-op until the caller has an
 * ImGui frame open. Reads guest memory; never writes to it. */
void ac_reinfo_frame(void);

/* Whether the window is open (env AC_REINFO=0 starts it closed). */
int ac_reinfo_visible(void);
void ac_reinfo_toggle(void);

#ifdef __cplusplus
}
#endif
