// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Runtime <-> userdata/launcher.ini bidirectional synchronization service.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void ac_config_sync_set_path(const char* path);
void ac_config_sync_init(void);
void ac_config_sync_poll(void);
void ac_config_sync_poll_force(void);
void ac_config_sync_save(void);

#ifdef __cplusplus
}
#endif
