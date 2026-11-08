#pragma once // [extra]
/* Horizon OS CMIF IPC command router and session dispatch. */
#include "kernel/horizon_defs.h"

static inline int is_cmif_command(uint32_t type) {
    return type == CMIF_REQUEST_COMMAND || type == CMIF_REQUEST_COMMAND_WITH_CTX;
}

static inline int is_cmif_control(uint32_t type) {
    return type == CMIF_REQUEST_CONTROL || type == CMIF_REQUEST_CONTROL_WITH_CTX;
}

// [extra] Language code resolution from environment, larecomp.ini or fallback // [extra]
static inline uint64_t resolve_system_language(uint64_t fallback) {
    const char *env_lang = getenv("AC_LANGUAGE");
    if (!env_lang || !*env_lang) env_lang = getenv("LARECOMP_LANG");
    if (!env_lang || !*env_lang || !strcmp(env_lang, "auto")) {
        FILE *f = fopen("larecomp.ini", "r");
        static char ini_val[32] = {0};
        if (f) {
            char line[128];
            while (fgets(line, sizeof(line), f)) {
                char k[32], v[32];
                if (sscanf(line, " %31[^= ] = %31[^\n]", k, v) == 2 && !strcmp(k, "language")) {
                    strncpy(ini_val, v, sizeof(ini_val) - 1);
                    break;
                }
            }
            fclose(f);
        }
        if (ini_val[0] && strcmp(ini_val, "auto") != 0) {
            env_lang = ini_val;
        } else {
            const char *loc = getenv("LC_ALL");
            if (!loc || !*loc) loc = getenv("LC_MESSAGES");
            if (!loc || !*loc) loc = getenv("LANG");
            if (loc && *loc) {
                if (!strncmp(loc, "ja", 2)) env_lang = "ja";
                else if (!strncmp(loc, "pt", 2)) env_lang = "en-US"; // [extra]
                else if (!strncmp(loc, "es", 2)) env_lang = "es";
                else if (!strncmp(loc, "fr", 2)) env_lang = "fr";
                else if (!strncmp(loc, "de", 2)) env_lang = "de";
                else if (!strncmp(loc, "it", 2)) env_lang = "it";
                else if (!strncmp(loc, "ru", 2)) env_lang = "ru";
                else if (!strncmp(loc, "ko", 2)) env_lang = "ko";
                else if (!strncmp(loc, "nl", 2)) env_lang = "nl";
                else if (!strncmp(loc, "zh", 2)) env_lang = (strstr(loc, "TW") || strstr(loc, "HK")) ? "zh-TW" : "zh-CN";
            }
        }
    }
    if (env_lang && *env_lang && strcmp(env_lang, "auto") != 0) {
        // [extra]
        if (!strcmp(env_lang, "pt-BR") || !strcmp(env_lang, "pt") || !strncmp(env_lang, "pt_", 3)) {
            env_lang = "en-US"; // Virtual language: guest engine uses Western/English base slot
        }
        // [extra]
        uint64_t code = 0;
        for (int i = 0; env_lang[i] && i < 8; i++) {
            code |= ((uint64_t)(uint8_t)env_lang[i]) << (i * 8);
        }
        return code;
    }
    return fallback;
} // [extra]

static int ipc_svc(Cpu *c, uint32_t imm, uint64_t pc) {
    if (imm == SVC_CONNECT_TO_NAMED_PORT) {
        Region r = query(c->x[1]);
        if (!(r.permission & PERM_R) || r.size - (c->x[1] - r.base) < 4) {
            boundary("port-name-memory", c, pc);
        }
        if (memcmp((void*)(uintptr_t)c->x[1], "sm:", 4)) {
            boundary("unsupported-named-port", c, pc);
        }
        c->x[1] = new_session(SESSION_SM);
        c->x[0] = RESULT_SUCCESS;
        puts("PORT sm: opened");
        return 1;
    }

    if (imm == SVC_CLOSE_HANDLE &&
        c->x[0] >= HANDLE_SESSION_BASE &&
        c->x[0] < HANDLE_SESSION_LIMIT &&
        session_handles[c->x[0] - HANDLE_SESSION_BASE]) {
        unsigned handle = c->x[0] - HANDLE_SESSION_BASE;
        unsigned closing = session_index((uint32_t)c->x[0]);
        session_handles[handle] = 0;
        session_peer_closed[handle] = 0;
        if (--session_refs[closing]) {
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }

        save_file_release(&sessions[closing].save_file);
        for (unsigned i = 0; i < 32; i++) {
            save_file_release(&sessions[closing].object_save_file[i]);
        }
        audout_release(&sessions[closing].audio_output);
        for (unsigned i = 0; i < 32; i++) {
            audout_release(&sessions[closing].object_audio_output[i]);
        }
        vi_release_displays(&sessions[closing].vi_displays);
        for (unsigned i = 0; i < 33; i++) {
            vi_release_layers(closing, i);
        }
        for (unsigned i = 0; i < 32; i++) {
            vi_release_displays(&sessions[closing].object_vi_displays[i]);
        }
        nv_release_devices(closing);
        if (sessions[closing].nv_transfer) {
            transfer_release(sessions[closing].nv_transfer);
            sessions[closing].nv_transfer = 0;
        }
        if (sessions[closing].storage) {
            storage_release(sessions[closing].storage);
        }
        for (unsigned i = 0; i < 32; i++) {
            if (sessions[closing].object_storage[i]) {
                storage_release(sessions[closing].object_storage[i]);
            }
        }
        if (!sessions[closing].domain && sessions[closing].kind == SESSION_CONTROLLER_APPLET) {
            close_library_applet();
        }
        for (unsigned i = 0; i < 32; i++) {
            if (sessions[closing].objects[i] == SESSION_CONTROLLER_APPLET) {
                close_library_applet();
            }
        }
        sessions[closing].active = 0;
        c->x[0] = RESULT_SUCCESS;
        return 1;
    }

    if (imm == SVC_SEND_SYNC_REQUEST &&
        c->x[0] >= HANDLE_SESSION_BASE &&
        c->x[0] < HANDLE_SESSION_LIMIT &&
        session_handles[c->x[0] - HANDLE_SESSION_BASE]) {
        unsigned handle = c->x[0] - HANDLE_SESSION_BASE;
        unsigned session = session_index((uint32_t)c->x[0]);
        Region tls = query(c->tpidrro);
        if (tls.permission != PERM_RW || tls.size - (c->tpidrro - tls.base) < 256) {
            boundary("ipc-tls-memory", c, pc);
        }
        uint8_t *buffer = (uint8_t*)(uintptr_t)c->tpidrro;
        if (session_peer_closed[handle]) {
            c->x[0] = RESULT_PORT_CLOSED;
            return 1;
        }
        if (ipc_u32(buffer) == CMIF_REQUEST_CLOSE && ipc_u32(buffer + 4) == 0) {
            session_peer_closed[handle] = 1;
            c->x[0] = RESULT_PORT_CLOSED;
            puts("IPC session peer closed");
            return 1;
        }

        IpcRequest request;
        const char *error = ipc_parse_mode(buffer, 256, sessions[session].domain, &request);
        if (error) {
            printf("IPC parse: %s; first 64 bytes:", error);
            for (unsigned i = 0; i < 16; i++) {
                printf(" %08x", ipc_u32(buffer + i * 4));
            }
            puts("");
            boundary("ipc-layout", c, pc);
        }

        int kind = sessions[session].kind;
        if (request.ambiguous_inputs && kind != SESSION_PREPO) {
            puts("IPC parse: ambiguous input buffers");
            boundary("ipc-layout", c, pc);
        }

        /* Synchronous server-side pointer-buffer copy. Alias buffers retain their
         * guest mapping; pointer inputs are snapshotted before service dispatch. */
        uint8_t pointer_buffer[4096];
        const uint8_t *input = (const uint8_t*)(uintptr_t)request.send_address;
        if (request.send_is_pointer) {
            Region source = query(request.send_address);
            if (request.send_size > sizeof(pointer_buffer)) {
                boundary("ipc-pointer-buffer-size", c, pc);
            }
            if (request.send_size && (!(source.permission & PERM_R) ||
                request.send_size > source.size - (request.send_address - source.base))) {
                boundary("ipc-pointer-buffer-memory", c, pc);
            }
            if (request.send_size) {
                memcpy(pointer_buffer, input, (size_t)request.send_size);
            }
            input = pointer_buffer;
        }

        if (request.domain) {
            if (!request.object_id || request.object_id > 32 || !sessions[session].objects[request.object_id - 1]) {
                boundary("invalid-domain-object", c, pc);
            }
            kind = sessions[session].objects[request.object_id - 1];
            if (request.domain_command == CMIF_DOMAIN_CLOSE_OBJECT) {
                save_file_release(&sessions[session].object_save_file[request.object_id - 1]);
                audout_release(&sessions[session].object_audio_output[request.object_id - 1]);
                vi_release_layers(session, request.object_id);
                vi_release_displays(&sessions[session].object_vi_displays[request.object_id - 1]);
                if (kind == SESSION_CONTROLLER_APPLET) {
                    close_library_applet();
                }
                unsigned storage = sessions[session].object_storage[request.object_id - 1];
                if (storage) {
                    storage_release(storage);
                }
                sessions[session].object_storage[request.object_id - 1] = 0;
                sessions[session].objects[request.object_id - 1] = 0;
                sessions[session].object_account_pid[request.object_id - 1] = 0;
                memset(sessions[session].object_account_user[request.object_id - 1], 0, 16);
                memset(&sessions[session].object_policy[request.object_id - 1], 0, sizeof(PctlState));
                memset(sessions[session].object_vibration[request.object_id - 1], 0, sizeof(sessions[session].object_vibration[0]));
                memset(&sessions[session].object_audio_device[request.object_id - 1], 0, sizeof(AudioDeviceState));
                reply_result(buffer, &request, 0);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
        }

        HOT_LOG("IPC kind=%d type=%u command=%u size=%zu pid=%u domain=%u object=%u\n",
                kind, request.type, request.command, request.argument_size,
                request.send_pid, request.domain, request.object_id);

        if (kind == SESSION_SAVE_FILESYSTEM && is_cmif_command(request.type)) {
            save_filesystem_dispatch(session, buffer, &request, input);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_SAVE_FILE && is_cmif_command(request.type)) {
            save_file_dispatch(session, buffer, &request);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_ACCOUNT_MANAGER && is_cmif_command(request.type)) {
            account_manager_dispatch(session, buffer, &request);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_ACCOUNT && is_cmif_command(request.type)) {
            account_dispatch(session, buffer, &request);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_TIME_ZONE && is_cmif_command(request.type)) {
            time_zone_dispatch(buffer, &request);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_PREPO && is_cmif_command(request.type)) {
            prepo_dispatch(buffer, &request);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if ((kind == SESSION_NIFM_ROOT || kind == SESSION_NIFM_GENERAL) && is_cmif_command(request.type)) {
            nifm_dispatch(session, kind, buffer, &request);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_AUDOUT_MANAGER && is_cmif_command(request.type)) {
            audout_dispatch(session, buffer, &request);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_AUDIO_OUTPUT && is_cmif_command(request.type)) {
            audout_stream_dispatch(session, buffer, &request);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }

        if (request.send_count > 1 && !(kind == SESSION_NVDRV && request.command == 11 && is_cmif_command(request.type))) {
            boundary("unsupported-service-second-input", c, pc);
        }
        if (request.send_is_pointer2 && request.send_size2 > 4096 - request.send_size * (request.send_is_pointer != 0)) {
            boundary("ipc-pointer-buffer-size", c, pc);
        }
        if (request.copy_count == 2 && !(kind == SESSION_NVDRV && request.command == 3 && is_cmif_command(request.type))) {
            boundary("unsupported-service-copy-handles", c, pc);
        }
        if (request.input_object_count && !(kind == SESSION_CONTROLLER_APPLET && request.command == 100 && request.domain)) {
            boundary("unsupported-service-input-object", c, pc);
        }
        if (request.receive_count &&
            !((kind == SESSION_SETTINGS && request.command == 5 ||
               kind == SESSION_AUDIO_DEVICE && (request.command == 0 || request.command == 6) ||
               kind == SESSION_STORAGE_ACCESSOR && request.command == 11 ||
               kind == SESSION_ROMFS_STORAGE && request.command == 0 ||
               kind == SESSION_NVDRV && (request.command == 1 || request.command == 11) ||
               kind == SESSION_VI_APPLICATION && request.command == 2020 ||
               kind == SESSION_VI_RELAY && (request.command == 0 || request.command == 3)) &&
              is_cmif_command(request.type))) {
            boundary("unsupported-service-output-buffer", c, pc);
        }
        if (request.receive_is_pointer && request.receive_size > 4096) {
            boundary("output-pointer-buffer-capacity", c, pc);
        }
        if (request.receive_mode &&
            kind != SESSION_ROMFS_STORAGE &&
            !(kind == SESSION_AUDIO_DEVICE && request.command == 6)) {
            boundary("unsupported-service-output-mode", c, pc);
        }

        if (is_cmif_control(request.type)) {
            if (request.command == 2 || (request.command == 4 && kind == SESSION_NVDRV)) {
                if (!sessions[session].domain && kind != SESSION_NVDRV) {
                    boundary("unsupported-nondomain-session-clone", c, pc);
                }
                if (request.command == 4 && (request.argument_size < 4 || ipc_u32(request.arguments) > 1)) {
                    boundary("nv-clone-tag", c, pc);
                }
                ipc_reply_handle(buffer, 256, clone_session(session), request.token);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (request.command == 3) {
                ipc_reply_u32(buffer, 256, sizeof(pointer_buffer), request.token);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (request.command == 0 && !sessions[session].domain) {
                if (kind == SESSION_NVDRV) {
                    boundary("nv-domain-conversion", c, pc);
                }
                vi_convert_layers(session);
                sessions[session].domain = 1;
                sessions[session].objects[0] = sessions[session].kind;
                sessions[session].object_policy[0] = sessions[session].policy;
                memcpy(sessions[session].object_account_user[0], sessions[session].account_user, 16);
                memset(sessions[session].account_user, 0, 16);
                sessions[session].object_account_pid[0] = sessions[session].account_pid;
                sessions[session].account_pid = 0;
                sessions[session].object_audio_device[0] = sessions[session].audio_device;
                sessions[session].object_save_file[0] = sessions[session].save_file;
                sessions[session].save_file = NULL;
                sessions[session].object_audio_output[0] = sessions[session].audio_output;
                sessions[session].audio_output = 0;
                sessions[session].object_vi_policy[0] = sessions[session].vi_policy;
                sessions[session].object_vi_level[0] = sessions[session].vi_level;
                sessions[session].object_vi_displays[0] = sessions[session].vi_displays;
                sessions[session].vi_displays = 0;
                sessions[session].object_storage[0] = sessions[session].storage;
                sessions[session].storage = 0;
                memcpy(sessions[session].object_vibration[0], sessions[session].vibration, sizeof(sessions[session].vibration));
                memset(sessions[session].vibration, 0, sizeof(sessions[session].vibration));
                ipc_reply_u32(buffer, 256, 1, request.token);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            boundary("ipc-control", c, pc);
        }

        if (kind == SESSION_SM && request.command == 0 && request.send_pid && !sessions[session].initialized) {
            sessions[session].initialized = 1;
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_SM && request.command == 1 && sessions[session].initialized && request.argument_size >= 8) {
            printf("SERVICE requested: ");
            for (unsigned i = 0; i < 8 && request.arguments[i]; i++) {
                printf("%c", request.arguments[i] >= 32 && request.arguments[i] < 127 ? request.arguments[i] : '?');
            }
            puts("");
            if (!memcmp(request.arguments, "lm\0\0\0\0\0\0", 8)) {
                reply_object(session, buffer, &request, SESSION_LM);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (!memcmp(request.arguments, "apm\0\0\0\0\0", 8)) {
                reply_object(session, buffer, &request, SESSION_APM);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (!memcmp(request.arguments, "appletOE", 8)) {
                reply_object(session, buffer, &request, SESSION_APPLET_OE);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (!memcmp(request.arguments, "aoc:u\0\0\0", 8)) {
                reply_object(session, buffer, &request, SESSION_AOC);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (!memcmp(request.arguments, "time:u\0\0", 8)) {
                reply_object(session, buffer, &request, SESSION_TIME_ROOT);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (!memcmp(request.arguments, "set\0\0\0\0\0", 8)) {
                reply_object(session, buffer, &request, SESSION_SETTINGS);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (!memcmp(request.arguments, "hid\0\0\0\0\0", 8)) {
                reply_object(session, buffer, &request, SESSION_HID);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (!memcmp(request.arguments, "hwopus\0\0", 8)) {
                reply_object(session, buffer, &request, SESSION_HWOPUS);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (!memcmp(request.arguments, "audren:u", 8)) {
                reply_object(session, buffer, &request, SESSION_AUDREN_MANAGER);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (!memcmp(request.arguments, "prepo:u\0", 8)) {
                reply_object(session, buffer, &request, SESSION_PREPO);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (!memcmp(request.arguments, "nifm:u\0\0", 8)) {
                reply_object(session, buffer, &request, SESSION_NIFM_ROOT);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (!memcmp(request.arguments, "acc:u0\0\0", 8)) {
                reply_object(session, buffer, &request, SESSION_ACCOUNT);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (!memcmp(request.arguments, "audout:u", 8)) {
                reply_object(session, buffer, &request, SESSION_AUDOUT_MANAGER);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (!memcmp(request.arguments, "fsp-srv\0", 8)) {
                reply_object(session, buffer, &request, SESSION_FS_PROXY);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (!memcmp(request.arguments, "nvdrv\0\0\0", 8)) {
                if (request.domain) {
                    boundary("nv-domain-service", c, pc);
                }
                reply_object(session, buffer, &request, SESSION_NVDRV);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (!memcmp(request.arguments, "vi:m\0\0\0\0", 8) ||
                !memcmp(request.arguments, "vi:s\0\0\0\0", 8) ||
                !memcmp(request.arguments, "vi:u\0\0\0\0", 8)) {
                unsigned level = request.arguments[3] == 'm' ? 2 : request.arguments[3] == 's' ? 1 : 0;
                vi_reply_object(session, buffer, &request, SESSION_VI_ROOT, 0, level);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (!memcmp(request.arguments, "pctl\0\0\0\0", 8) ||
                !memcmp(request.arguments, "pctl:s\0\0", 8) ||
                !memcmp(request.arguments, "pctl:a\0\0", 8) ||
                !memcmp(request.arguments, "pctl:r\0\0", 8)) {
                reply_object(session, buffer, &request, SESSION_PCTL_ROOT);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
        }

        if (kind == SESSION_NVDRV) {
            nv_dispatch(session, buffer, &request);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_HWOPUS) {
            hwopus_dispatch(buffer, &request);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_AUDREN_MANAGER) {
            audren_dispatch(session, buffer, &request);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_AUDIO_DEVICE) {
            audio_device_dispatch(session, buffer, &request);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind >= SESSION_VI_ROOT && kind <= SESSION_VI_INDIRECT) {
            vi_dispatch(session, kind, buffer, &request);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_APPLET_OE &&
            request.command == 0 &&
            request.send_pid &&
            request.copy_count == 1 &&
            request.copy_handle == HANDLE_CURRENT_PROCESS &&
            request.argument_size >= 8) {
            reply_object(session, buffer, &request, SESSION_APPLICATION_PROXY);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_FS_PROXY && request.command == 1 && request.send_pid && request.argument_size >= 8) {
            sessions[session].initialized = 1;
            printf("FILESYSTEM registered process=%lu\n", GetCurrentProcessId());
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_FS_PROXY && request.command == 1005) {
            if (!sessions[session].initialized) {
                boundary("filesystem-process-not-registered", c, pc);
            }
            reply_value(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_FS_PROXY && request.command == 51) {
            application_mount_save(session, buffer, &request);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_FS_PROXY && request.command == 200) {
            if (!sessions[session].initialized) {
                boundary("filesystem-process-not-registered", c, pc);
            }
            romfs_open();
            reply_object(session, buffer, &request, SESSION_ROMFS_STORAGE);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_ROMFS_STORAGE && request.command == 4) {
            romfs_open();
            reply_wide_value(buffer, &request, romfs_size);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_ROMFS_STORAGE && request.command == 0 && request.argument_size >= 16) {
            uint64_t offset, length;
            memcpy(&offset, request.arguments, 8);
            memcpy(&length, request.arguments + 8, 8);
            if (request.receive_count != 1 || length > request.receive_size) {
                boundary("romfs-read-output-descriptor", c, pc);
            }
            romfs_read(offset, length, request.receive_address);
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_HID && request.command == 0 && request.send_pid && request.argument_size >= 8) {
            uint64_t aruid;
            memcpy(&aruid, request.arguments, 8);
            if (aruid != GetCurrentProcessId()) {
                boundary("invalid-hid-resource-user", c, pc);
            }
            hid_initialize();
            reply_object(session, buffer, &request, SESSION_HID_RESOURCE);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_HID_RESOURCE && request.command == 0) {
            if (!hid_page) {
                boundary("hid-resource-not-initialized", c, pc);
            }
            uint32_t handle = copy_shared_object(hid_memory, PERM_R, HID_MEMORY_SIZE);
            ipc_reply_copy(buffer, 256, handle, request.domain, request.token);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_HID && request.command == 203) {
            reply_object(session, buffer, &request, SESSION_VIBRATION_LIST);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_HID && request.command == 11 && request.send_pid && request.argument_size >= 8) {
            uint64_t aruid;
            memcpy(&aruid, request.arguments, 8);
            if (!hid_page || aruid != GetCurrentProcessId()) {
                boundary("invalid-hid-resource-user", c, pc);
            }
            hid_touch_active = 1;
            hid_touch_publish(rt_ticks());
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_HID && request.command == 31 && request.send_pid && request.argument_size >= 8) {
            uint64_t aruid;
            memcpy(&aruid, request.arguments, 8);
            if (!hid_page || aruid != GetCurrentProcessId()) {
                boundary("invalid-hid-resource-user", c, pc);
            }
            keyboard_active = 1;
            keyboard_publish(rt_ticks(), app_input_focus && app_focus == 1 && !app_exit_requested);
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_HID && request.command == 21 && request.send_pid && request.argument_size >= 8) {
            uint64_t aruid;
            memcpy(&aruid, request.arguments, 8);
            if (!hid_page || aruid != GetCurrentProcessId()) {
                boundary("invalid-hid-resource-user", c, pc);
            }
            mouse_active = 1;
            mouse_publish(rt_ticks(), app_input_focus && app_focus == 1 && !app_exit_requested);
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_HID &&
            (request.command == 66 || request.command == 67) &&
            request.send_pid &&
            request.argument_size >= 16) {
            uint64_t aruid;
            memcpy(&aruid, request.arguments + 8, 8);
            if (!hid_page || aruid != GetCurrentProcessId()) {
                boundary("invalid-hid-resource-user", c, pc);
            }
            unsigned handle = ipc_u32(request.arguments);
            unsigned index = hid_sensor_index(handle);
            if (request.command == 66) {
                hid_sensor_started[index / 64] |= 1ull << (index % 64);
            } else {
                hid_sensor_started[index / 64] &= ~(1ull << (index % 64));
            }
            printf("HID sixaxis handle=0x%x started=%u (no host sensor)\n", handle, request.command == 66);
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_VIBRATION_LIST && request.command == 0 && request.argument_size >= 4) {
            uint32_t handle = ipc_u32(request.arguments);
            unsigned index = hid_vibration_index(handle);
            uint64_t *registered = request.domain
                ? sessions[session].object_vibration[request.object_id - 1]
                : sessions[session].vibration;
            registered[index / 64] |= 1ull << (index % 64);
            printf("HID vibration registered handle=0x%x (no host actuator)\n", handle);
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_HID && request.command == 200 && request.argument_size >= 4) {
            hid_vibration_index(ipc_u32(request.arguments));
            reply_wide_value(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_HID && request.command == 201) {
            hid_accept_vibration(&request);
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_HID && request.command == 109 && request.send_pid && request.argument_size >= 16) {
            uint64_t aruid;
            memcpy(&aruid, request.arguments + 8, 8);
            unsigned revision = ipc_u32(request.arguments);
            printf("HID activate revision=%u\n", revision);
            if (!hid_page || aruid != GetCurrentProcessId()) {
                boundary("invalid-hid-resource-user", c, pc);
            }
            if (revision != 2) {
                boundary("unsupported-hid-revision", c, pc);
            }
            hid_revision = revision;
            hid_active = 1;
            hid_publish(rt_ticks(), app_input_focus);
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_HID && request.command == 100 && request.send_pid && request.argument_size >= 16) {
            uint64_t aruid;
            memcpy(&aruid, request.arguments + 8, 8);
            if (!hid_page || aruid != GetCurrentProcessId()) {
                boundary("invalid-hid-resource-user", c, pc);
            }
            unsigned styles = ipc_u32(request.arguments);
            if (styles & ~NPAD_STYLE_SUPPORT_ALL) {
                boundary("unsupported-npad-style-mask", c, pc);
            }
            hid_styles = styles;
            hid_publish(rt_ticks(), app_input_focus);
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_HID && request.command == 101 && request.send_pid && request.argument_size >= 8) {
            uint64_t aruid;
            memcpy(&aruid, request.arguments, 8);
            if (!hid_page || aruid != GetCurrentProcessId()) {
                boundary("invalid-hid-resource-user", c, pc);
            }
            reply_value(buffer, &request, hid_styles);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_HID && request.command == 102 && request.send_pid && request.argument_size >= 8) {
            uint64_t aruid;
            memcpy(&aruid, request.arguments, 8);
            if (!hid_page || aruid != GetCurrentProcessId()) {
                boundary("invalid-hid-resource-user", c, pc);
            }
            if (!request.send_is_pointer || request.send_size % 4 || request.send_size > 40) {
                boundary("invalid-npad-id-buffer", c, pc);
            }
            unsigned ids = 0;
            for (unsigned i = 0; i < request.send_size / 4; i++) {
                ids |= 1u << hid_slot(ipc_u32(input + i * 4));
            }
            hid_ids = ids;
            hid_publish(rt_ticks(), app_input_focus);
            printf("HID supported id mask=0x%x\n", ids);
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_HID &&
            (request.command == 120 || request.command == 121 || request.command == 128 || request.command == 129) &&
            request.send_pid && request.argument_size >= 8) {
            uint64_t aruid;
            memcpy(&aruid, request.arguments, 8);
            if (!hid_page || aruid != GetCurrentProcessId()) {
                boundary("invalid-hid-resource-user", c, pc);
            }
            uint64_t *policy = request.command < 128 ? &hid_joy_hold_type : &hid_handheld_mode;
            if (!(request.command & 1)) {
                if (request.argument_size < 16) {
                    boundary("short-controller-policy", c, pc);
                }
                uint64_t mode;
                memcpy(&mode, request.arguments + 8, 8);
                if (mode > (request.command == 128 ? 2u : 1u)) {
                    boundary("invalid-controller-policy", c, pc);
                }
                *policy = mode;
                printf("HID policy command=%u value=%llu\n", request.command, (unsigned long long)mode);
                reply_result(buffer, &request, 0);
            } else {
                reply_wide_value(buffer, &request, *policy);
            }
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_TIME_ROOT && request.command <= 4 && !request.send_pid && !request.copy_count) {
            static const int clocks[] = {
                SESSION_TIME_USER,
                SESSION_TIME_NETWORK,
                SESSION_TIME_STEADY,
                SESSION_TIME_ZONE,
                SESSION_TIME_LOCAL
            };
            reply_object(session, buffer, &request, clocks[request.command]);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_TIME_ROOT && request.command == 20 && !request.send_pid && !request.copy_count) {
            clock_initialize();
            uint32_t handle = copy_shared_handle(clock_memory, PERM_R);
            if (!ipc_reply_copy(buffer, 256, handle, request.domain, request.token)) {
                boundary("clock-handle-reply", c, pc);
            }
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_PCTL_ROOT &&
            (request.command == 0 || request.command == 1) &&
            request.send_pid && request.argument_size >= 8) {
            reply_object(session, buffer, &request, SESSION_PCTL);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_PCTL) {
            PctlState *policy = request.domain
                ? &sessions[session].object_policy[request.object_id - 1]
                : &sessions[session].policy;
            if (request.command == 1) {
                *policy = (PctlState){1, 0, 0};
                reply_result(buffer, &request, 0);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (!policy->initialized) {
                boundary("parental-policy-not-initialized", c, pc);
            }
            if (request.command == 1031 || request.command == 1403) {
                reply_value(buffer, &request, request.command == 1031 ? policy->restrictions_enabled : policy->paired);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
        }
        if (kind == SESSION_APPLICATION_PROXY) {
            int child = 0;
            switch (request.command) {
                case 0: child = SESSION_COMMON_STATE; break;
                case 1: child = SESSION_SELF_CONTROLLER; break;
                case 2: child = SESSION_WINDOW_CONTROLLER; break;
                case 3: child = SESSION_AUDIO_CONTROLLER; break;
                case 4: child = SESSION_DISPLAY_CONTROLLER; break;
                case 11: child = SESSION_LIBRARY_CREATOR; break;
                case 20: child = SESSION_APPLICATION_FUNCTIONS; break;
                case 1000: child = SESSION_DEBUG_FUNCTIONS; break;
            }
            if (child) {
                reply_object(session, buffer, &request, child);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
        }
        if (kind == SESSION_APM && request.command == 0) {
            reply_object(session, buffer, &request, SESSION_APM_SESSION);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_APPLICATION_FUNCTIONS && request.command == 1) {
            account_launch_parameter(session, buffer, &request);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_APPLICATION_FUNCTIONS && request.command == 20) {
            application_ensure_save(buffer, &request);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_APPLICATION_FUNCTIONS && request.command == 21) { // [extra]
            uint64_t lang = resolve_system_language(APPLICATION_LANGUAGE);
            reply_wide_value(buffer, &request, lang ? lang : APPLICATION_LANGUAGE);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        } // [extra]
        if (kind == SESSION_APPLICATION_FUNCTIONS && request.command == 22 && request.argument_size >= 4) {
            application_terminate_result = ipc_u32(request.arguments);
            application_terminate_result_set = 1;
            printf("APPLICATION terminate result set=0x%x\n", application_terminate_result);
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_SELF_CONTROLLER && request.command == 40) {
            if (request.copy_count || request.send_pid || request.send_size ||
                request.receive_count || request.input_object_count) {
                boundary("vi-managed-layer-layout", c, pc);
            }
            reply_wide_value(buffer, &request, vi_create_managed_layer());
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_SELF_CONTROLLER && request.command == 9) {
            uint32_t handle = copy_event(&library_launch_event);
            if (!(library_applet_running ? ResetEvent(library_launch_event) : SetEvent(library_launch_event))) {
                boundary("library-launch-event", c, pc);
            }
            ipc_reply_copy(buffer, 256, handle, request.domain, request.token);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_LIBRARY_CREATOR && request.command == 0 && request.argument_size >= 8) {
            uint32_t applet_id = ipc_u32(request.arguments);
            uint32_t mode = ipc_u32(request.arguments + 4);
            printf("LIBRARY requested id=0x%x mode=%u\n", applet_id, mode);
            if ((applet_id != APPLET_ID_CONTROLLER && applet_id != APPLET_ID_SOFTWARE_KEYBOARD) || mode != 0) {
                boundary("unsupported-library-applet", c, pc);
            }
            if (library_applet_created) {
                boundary("library-applet-slot-occupied", c, pc);
            }
            library_applet_created = 1;
            library_applet_id = applet_id;
            reply_object(session, buffer, &request, SESSION_CONTROLLER_APPLET);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_LIBRARY_CREATOR && request.command == 10 && request.argument_size >= 8) {
            uint64_t size;
            memcpy(&size, request.arguments, 8);
            printf("LIBRARY storage size=%llu\n", (unsigned long long)size);
            reply_storage(session, buffer, &request, SESSION_STORAGE, storage_create(size));
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_LIBRARY_CREATOR && request.command == 11 && request.argument_size >= 16 && request.copy_count == 1) {
            uint64_t size;
            memcpy(&size, request.arguments + 8, 8);
            unsigned i = transfer_index(request.copy_handle);
            if (!size || size > transfer_memory[i].size) {
                boundary("transfer-storage-size", c, pc);
            }
            unsigned storage = storage_create(size);
            memcpy(applet_storage[storage - 1].data, transfer_memory[i].data, (size_t)size);
            printf("LIBRARY transfer storage size=%llu writable=%u\n", (unsigned long long)size, request.arguments[0]);
            reply_storage(session, buffer, &request, SESSION_STORAGE, storage);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_STORAGE || kind == SESSION_STORAGE_ACCESSOR) {
            unsigned storage = request.domain
                ? sessions[session].object_storage[request.object_id - 1]
                : sessions[session].storage;
            storage_validate(storage);
            if (kind == SESSION_STORAGE && request.command == 0) {
                reply_storage(session, buffer, &request, SESSION_STORAGE_ACCESSOR, storage);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (kind == SESSION_STORAGE_ACCESSOR && request.command == 0) {
                reply_wide_value(buffer, &request, applet_storage[storage - 1].size);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
            if (kind == SESSION_STORAGE_ACCESSOR && (request.command == 10 || request.command == 11) && request.argument_size >= 8) {
                uint64_t offset;
                memcpy(&offset, request.arguments, 8);
                int writing = (request.command == 10);
                uint64_t length = writing ? request.send_size : request.receive_size;
                uint64_t address = writing ? request.send_address : request.receive_address;
                if (!writing && request.receive_count != 1) {
                    boundary("storage-read-output-descriptor", c, pc);
                }
                uint64_t size = applet_storage[storage - 1].size;
                if (offset > size || length > size - offset) {
                    boundary("applet-storage-transfer-extent", c, pc);
                }
                Region memory = query(address);
                if (length && (!(memory.permission & (writing ? PERM_R : PERM_W)) || length > memory.size - (address - memory.base))) {
                    boundary("applet-storage-transfer-memory", c, pc);
                }
                if (length) {
                    if (writing) {
                        memcpy(applet_storage[storage - 1].data + offset, input, (size_t)length);
                    } else {
                        memcpy((void*)(uintptr_t)address, applet_storage[storage - 1].data + offset, (size_t)length);
                    }
                }
                reply_result(buffer, &request, 0);
                c->x[0] = RESULT_SUCCESS;
                return 1;
            }
        }
        if (kind == SESSION_CONTROLLER_APPLET && request.command == 0) {
            if (!library_applet_created) {
                boundary("library-applet-not-created", c, pc);
            }
            ipc_reply_copy(buffer, 256, copy_event(&library_state_event), request.domain, request.token);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_CONTROLLER_APPLET && request.command == 10) {
            if (library_applet_id == APPLET_ID_SOFTWARE_KEYBOARD) {
                swkbd_applet_start();
            } else {
                controller_applet_start();
            }
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_CONTROLLER_APPLET && request.command == 1) {
            if (!library_applet_created) {
                boundary("library-applet-not-created", c, pc);
            }
            reply_value(buffer, &request, library_completed ? 1 : 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_CONTROLLER_APPLET && (request.command == 105 || request.command == 106)) {
            static HANDLE interactive_event;
            if (!library_applet_created) {
                boundary("library-applet-not-created", c, pc);
            }
            uint32_t handle = copy_event(request.command == 105 ? &library_state_event : &interactive_event);
            ipc_reply_copy(buffer, 256, handle, request.domain, request.token);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_CONTROLLER_APPLET && request.command == 30) {
            if (!library_completed) {
                boundary("library-result-before-completion", c, pc);
            }
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_CONTROLLER_APPLET && request.command == 101) {
            if (!library_completed || !library_output) {
                boundary("library-output-unavailable", c, pc);
            }
            reply_storage(session, buffer, &request, SESSION_STORAGE, library_output);
            storage_release(library_output);
            library_output = 0;
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_CONTROLLER_APPLET && request.command == 100 && request.domain && request.input_object_count == 1) {
            unsigned object = request.input_object;
            if (!library_applet_created || library_applet_running || library_completed || library_input_count == 4) {
                boundary("library-input-state", c, pc);
            }
            if (object > 32 || sessions[session].objects[object - 1] != SESSION_STORAGE) {
                boundary("library-input-storage-object", c, pc);
            }
            unsigned storage = sessions[session].object_storage[object - 1];
            storage_retain(storage);
            library_inputs[library_input_count++] = storage;
            printf("LIBRARY input %u size=%llu prefix=", library_input_count, (unsigned long long)applet_storage[storage - 1].size);
            for (unsigned i = 0; i < 32 && i < applet_storage[storage - 1].size; i++) {
                printf("%02x", applet_storage[storage - 1].data[i]);
            }
            puts("");
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_SETTINGS && request.command == 0) { // [extra]
            uint64_t lang = resolve_system_language(SYSTEM_LANGUAGE);
            reply_wide_value(buffer, &request, lang ? lang : SYSTEM_LANGUAGE);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        } // [extra]
        if (kind == SESSION_SETTINGS && (request.command == 5 || request.command == 6)) {
            static const char languages[][8] = {
                "ja", "en-US", "fr", "de", "it", "es", "zh-CN", "ko", "nl",
                "pt", "ru", "zh-TW", "en-GB", "fr-CA", "es-419", "zh-Hans", "zh-Hant", "pt-BR"
            };
            unsigned count = sizeof(languages) / sizeof(languages[0]);
            if (request.command == 5) {
                if (request.receive_count != 1 || request.send_size ||
                    request.receive_size > 1024 * 1024 || request.receive_size % 8) {
                    boundary("invalid-language-output-buffer", c, pc);
                }
                if (request.receive_size) {
                    Region output = query(request.receive_address);
                    if (!(output.permission & PERM_W) ||
                        request.receive_size > output.size - (request.receive_address - output.base)) {
                        boundary("language-output-not-writable", c, pc);
                    }
                }
                if (count > request.receive_size / 8) {
                    count = (unsigned)(request.receive_size / 8);
                }
                if (count) {
                    memcpy((void*)(uintptr_t)request.receive_address, languages, count * 8);
                }
            }
            reply_value(buffer, &request, count);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_APM && request.command == 1) {
            app_modes_load();
            reply_value(buffer, &request, app_performance_mode);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_APM_SESSION && request.command <= 1 && request.argument_size >= 4) {
            unsigned mode = ipc_u32(request.arguments);
            if (mode > 1) {
                boundary("invalid-performance-mode", c, pc);
            }
            if (request.command == 0) {
                if (request.argument_size < 8) {
                    boundary("short-performance-configuration", c, pc);
                }
                uint32_t configuration = ipc_u32(request.arguments + 4);
                printf("APM configuration mode=%u value=0x%x\n", mode, configuration);
                if (!((mode == APM_PERF_MODE_HANDHELD && configuration == APM_CONFIG_HANDHELD) ||
                      (mode == APM_PERF_MODE_DOCKED && configuration == APM_CONFIG_DOCKED))) {
                    boundary("unsupported-performance-configuration", c, pc);
                }
                performance_configuration[mode] = configuration;
                performance_configuration_set[mode] = 1;
                reply_result(buffer, &request, 0);
            } else {
                if (!performance_configuration_set[mode]) {
                    boundary("uninitialized-performance-configuration", c, pc);
                }
                reply_value(buffer, &request, performance_configuration[mode]);
            }
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_AOC && request.command == 8) {
            ipc_reply_copy(buffer, 256, copy_event(&addon_change_event), request.domain, request.token);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_WINDOW_CONTROLLER && request.command == 1) {
            reply_wide_value(buffer, &request, GetCurrentProcessId());
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_WINDOW_CONTROLLER && request.command == 10) {
            if (!app_window || !IsWindowVisible(app_window) || app_focus != 1) {
                boundary("applet-not-active", c, pc);
            }
            app_foreground_owned = 1;
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_SELF_CONTROLLER && request.command == 91) {
            ipc_reply_copy(buffer, 256, copy_event(&app_suspend_event), request.domain, request.token);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_SELF_CONTROLLER && request.command == 90) {
            reply_wide_value(buffer, &request, app_suspended_ticks);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_SELF_CONTROLLER && request.command == 13 && request.argument_size >= 3) {
            const uint8_t *p = request.arguments;
            if (!((p[0] == 0 && p[1] == 0 && p[2] == 1) ||
                  (p[0] == 1 && p[1] == 1 && p[2] == 0) ||
                  (p[0] == 1 && p[1] == 0 && p[2] == 1))) {
                boundary("unsupported-focus-policy", c, pc);
            }
            app_focus_policy = 1;
            app_suspend_background = p[2];
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_SELF_CONTROLLER && request.command == 16 && request.argument_size >= 1) {
            if (request.arguments[0] > 1) {
                boundary("invalid-suspension-flag", c, pc);
            }
            app_suspend_out_of_focus = request.arguments[0];
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_SELF_CONTROLLER && request.command == 14 && request.argument_size >= 1) {
            if (request.arguments[0] > 1) {
                boundary("invalid-resume-notification-flag", c, pc);
            }
            app_resume_notifications = request.arguments[0];
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_SELF_CONTROLLER && (request.command == 11 || request.command == 12) && request.argument_size >= 1) {
            if (request.arguments[0] > 1) {
                boundary("invalid-mode-notification-flag", c, pc);
            }
            if (request.command == 11) {
                app_operation_notifications = request.arguments[0];
            } else {
                app_performance_notifications = request.arguments[0];
            }
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_COMMON_STATE && request.command == 0) {
            uint32_t handle = copy_event(&app_message_event);
            app_open();
            ipc_reply_copy(buffer, 256, handle, request.domain, request.token);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_COMMON_STATE && request.command == 1) {
            unsigned message;
            if (app_receive(&message)) {
                reply_value(buffer, &request, message);
            } else {
                reply_result(buffer, &request, RESULT_NO_MESSAGE);
            }
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_COMMON_STATE && request.command == 9) {
            reply_value(buffer, &request, app_focus);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_COMMON_STATE && (request.command == 5 || request.command == 6)) {
            app_modes_load();
            reply_value(buffer, &request, request.command == 5 ? app_operation_mode : app_performance_mode);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_LM && request.command == 0 && request.send_pid) {
            reply_object(session, buffer, &request, SESSION_LOGGER);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_LOGGER && request.command == 0) {
            Region data = query(request.send_address);
            if (!(data.permission & PERM_R) ||
                request.send_size < 24 ||
                request.send_size > 1024 * 1024 ||
                request.send_size > data.size - (request.send_address - data.base)) {
                boundary("log-buffer-extent", c, pc);
            }
            const uint8_t *packet = input;
            uint32_t payload = ipc_u32(packet + 20);
            if (payload != request.send_size - 24 || !(packet[16] & 4)) {
                boundary("log-packet-layout", c, pc);
            }
            if (sessions[session].destination) {
                char path[1024];
                if (!snapshot_path || snprintf(path, sizeof(path), "%s.logs", snapshot_path) >= (int)sizeof(path)) {
                    boundary("log-path", c, pc);
                }
                if (log_bytes + request.send_size > 16 * 1024 * 1024) {
                    boundary("log-budget", c, pc);
                }
                FILE *log = fopen(path, "ab");
                uint32_t length = (uint32_t)request.send_size;
                if (!log || fwrite(&length, 4, 1, log) != 1 ||
                    fwrite(packet, 1, length, log) != length || fclose(log)) {
                    boundary("log-write", c, pc);
                }
                log_bytes += length;
            }
            printf("LOG captured %llu bytes\n", (unsigned long long)request.send_size);
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        if (kind == SESSION_LOGGER && request.command == 1 && request.argument_size >= 4) {
            unsigned destination = ipc_u32(request.arguments);
            if (destination & ~3u) {
                boundary("log-destination", c, pc);
            }
            sessions[session].destination = destination;
            reply_result(buffer, &request, 0);
            c->x[0] = RESULT_SUCCESS;
            return 1;
        }
        printf("UNIMPLEMENTED session=%u kind=%d command=%u args=%u copies=%u\n",
               session, (int)kind, request.command, (unsigned)request.argument_size, (unsigned)request.copy_count);
        boundary("unimplemented-service-command", c, pc);
    }
    return 0;
}
// [extra]
