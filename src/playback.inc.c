/* Audio engine, Discord presence transport, and playback controls. */

typedef struct WaveBufferSlot {
    WAVEHDR header;
    BYTE *data;
    DWORD bytes;
    BOOL prepared;
} WaveBufferSlot;

static void free_audio_outputs(void) {
    g_audio_output_count = 0;
}

static void enumerate_audio_outputs(void) {
    free_audio_outputs();
    if (g_output) SendMessageW(g_output, CB_RESETCONTENT, 0, 0);

    ZeroMemory(&g_audio_outputs[0], sizeof(g_audio_outputs[0]));
    wcsncpy(g_audio_outputs[0].name, L"System default", ARRAY_LEN(g_audio_outputs[0].name) - 1);
    g_audio_outputs[0].device_id = WAVE_MAPPER;
    g_audio_output_count = 1;
    if (g_output) SendMessageW(g_output, CB_ADDSTRING, 0, (LPARAM)g_audio_outputs[0].name);

    UINT count = waveOutGetNumDevs();
    for (UINT i = 0; i < count && g_audio_output_count < (int)ARRAY_LEN(g_audio_outputs); ++i) {
        WAVEOUTCAPSW caps;
        ZeroMemory(&caps, sizeof(caps));
        if (waveOutGetDevCapsW(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) continue;
        AudioOutput *out = &g_audio_outputs[g_audio_output_count];
        ZeroMemory(out, sizeof(*out));
        wcsncpy(out->name, caps.szPname, ARRAY_LEN(out->name) - 1);
        out->device_id = i;
        if (g_output) SendMessageW(g_output, CB_ADDSTRING, 0, (LPARAM)out->name);
        ++g_audio_output_count;
    }
    g_audio_output_index = 0;
    for (int i = 0; i < g_audio_output_count; ++i) {
        if (_wcsicmp(g_audio_outputs[i].name, g_audio_output_preference) == 0) {
            g_audio_output_index = i;
            break;
        }
    }
    if (g_output) SendMessageW(g_output, CB_SETCURSEL, g_audio_output_index, 0);
}

static void apply_player_volume(void) {
    DWORD channel = (DWORD)MulDiv(max(0, min(100, g_volume_percent)), 0xFFFF, 100);
    DWORD packed = channel | (channel << 16);
    EnterCriticalSection(&g_audio_lock);
    if (g_waveout) waveOutSetVolume(g_waveout, packed);
    LeaveCriticalSection(&g_audio_lock);
    if (g_video_player) IMFPMediaPlayer_SetVolume(g_video_player,
        (float)max(0, min(100, g_volume_percent)) / 100.0f);
}

static LONGLONG player_duration_100ns(void) {
    if (g_video_player) {
        PROPVARIANT value;
        PropVariantInit(&value);
        if (SUCCEEDED(IMFPMediaPlayer_GetDuration(g_video_player, &MFP_POSITIONTYPE_100NS, &value))) {
            LONGLONG result = value.vt == VT_I8 ? value.hVal.QuadPart :
                              value.vt == VT_UI8 ? (LONGLONG)value.uhVal.QuadPart : 0;
            PropVariantClear(&value);
            if (result > 0) InterlockedExchange64(&g_player_duration, result);
        } else {
            PropVariantClear(&value);
        }
    }
    return InterlockedCompareExchange64(&g_player_duration, 0, 0);
}

static LONGLONG player_position_100ns(void) {
    if (g_video_player) {
        PROPVARIANT value;
        PropVariantInit(&value);
        if (SUCCEEDED(IMFPMediaPlayer_GetPosition(g_video_player, &MFP_POSITIONTYPE_100NS, &value))) {
            LONGLONG result = value.vt == VT_I8 ? value.hVal.QuadPart :
                              value.vt == VT_UI8 ? (LONGLONG)value.uhVal.QuadPart : 0;
            PropVariantClear(&value);
            if (result >= 0) InterlockedExchange64(&g_player_position, result);
        } else {
            PropVariantClear(&value);
        }
    }
    return InterlockedCompareExchange64(&g_player_position, 0, 0);
}

static void discord_close_pipe(void) {
    if (g_discord_pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(g_discord_pipe);
        g_discord_pipe = INVALID_HANDLE_VALUE;
    }
    g_discord_last_update = 0;
}

static BOOL discord_write_all(const void *data, DWORD size) {
    const BYTE *cursor = (const BYTE *)data;
    while (size > 0) {
        DWORD written = 0;
        if (!WriteFile(g_discord_pipe, cursor, size, &written, NULL) || written == 0) return FALSE;
        cursor += written;
        size -= written;
    }
    return TRUE;
}

static BOOL discord_write_frame(uint32_t opcode, const char *payload, uint32_t payload_size) {
    if (g_discord_pipe == INVALID_HANDLE_VALUE) return FALSE;
    uint32_t header[2] = { opcode, payload_size };
    return discord_write_all(header, sizeof(header)) &&
           (payload_size == 0 || discord_write_all(payload, payload_size));
}

static BOOL discord_read_all(void *data, DWORD size) {
    BYTE *cursor = (BYTE *)data;
    while (size > 0) {
        DWORD got = 0;
        if (!ReadFile(g_discord_pipe, cursor, size, &got, NULL) || got == 0) return FALSE;
        cursor += got;
        size -= got;
    }
    return TRUE;
}

static BOOL discord_drain_pipe(void) {
    if (g_discord_pipe == INVALID_HANDLE_VALUE) return FALSE;
    for (int frame_count = 0; frame_count < 12; ++frame_count) {
        uint32_t header[2] = {0, 0};
        DWORD peeked = 0, available = 0;
        if (!PeekNamedPipe(g_discord_pipe, header, sizeof(header), &peeked, &available, NULL))
            return FALSE;
        if (available < sizeof(header) || peeked < sizeof(header)) return TRUE;
        uint32_t payload_size = header[1];
        if (payload_size > 64 * 1024) return FALSE;
        if (available < sizeof(header) + payload_size) return TRUE;

        if (!discord_read_all(header, sizeof(header))) return FALSE;
        char *payload = (char *)calloc(1, (size_t)payload_size + 1);
        if (!payload) return FALSE;
        BOOL ok = payload_size == 0 || discord_read_all(payload, payload_size);
        if (ok && header[0] == 3) ok = discord_write_frame(4, payload, payload_size);
        if (ok && header[0] == 2) ok = FALSE;
        if (ok && strstr(payload, "\"evt\":\"READY\"") != NULL)
            discord_set_status(L"Connected to Discord");
        free(payload);
        if (!ok) return FALSE;
    }
    return TRUE;
}

static size_t discord_json_escape(const wchar_t *source, size_t source_limit,
                                  char *output, size_t output_size) {
    if (!output || output_size == 0) return 0;
    output[0] = '\0';
    if (!source) return 0;
    size_t source_len = wcslen(source);
    if (source_len > source_limit) source_len = source_limit;
    if (source_len > 1024) source_len = 1024;

    char utf8[4096];
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, source, (int)source_len,
                                       utf8, (int)sizeof(utf8), NULL, NULL);
    if (utf8_len <= 0) return 0;

    size_t used = 0;
    for (int i = 0; i < utf8_len && used + 1 < output_size; ++i) {
        unsigned char c = (unsigned char)utf8[i];
        const char *escape = NULL;
        if (c == '"') escape = "\\\"";
        else if (c == '\\') escape = "\\\\";
        else if (c == '\b') escape = "\\b";
        else if (c == '\f') escape = "\\f";
        else if (c == '\n') escape = "\\n";
        else if (c == '\r') escape = "\\r";
        else if (c == '\t') escape = "\\t";
        if (escape) {
            size_t count = strlen(escape);
            if (used + count >= output_size) break;
            memcpy(output + used, escape, count);
            used += count;
        } else if (c < 0x20) {
            if (used + 6 >= output_size) break;
            int count = snprintf(output + used, output_size - used, "\\u%04x", c);
            if (count <= 0) break;
            used += (size_t)count;
        } else {
            output[used++] = (char)c;
        }
    }
    output[used] = '\0';
    return used;
}

static BOOL discord_send_clear(void) {
    char payload[384];
    unsigned long long nonce = ++g_discord_nonce;
    int count = snprintf(payload, sizeof(payload),
        "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%lu,\"activity\":null},\"nonce\":\"%llu\"}",
        GetCurrentProcessId(), nonce);
    return count > 0 && (size_t)count < sizeof(payload) &&
           discord_write_frame(1, payload, (uint32_t)count);
}

static void discord_disconnect(BOOL clear_presence) {
    if (g_discord_pipe != INVALID_HANDLE_VALUE && clear_presence) discord_send_clear();
    discord_close_pipe();
}

static BOOL discord_connect(void) {
    g_discord_last_attempt = GetTickCount64();
    for (int i = 0; i < 10; ++i) {
        wchar_t pipe_name[64];
        swprintf(pipe_name, ARRAY_LEN(pipe_name), L"\\\\?\\pipe\\discord-ipc-%d", i);
        HANDLE pipe = CreateFileW(pipe_name, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                  OPEN_EXISTING, 0, NULL);
        if (pipe == INVALID_HANDLE_VALUE) continue;
        g_discord_pipe = pipe;

        char app_id[64];
        int app_id_len = WideCharToMultiByte(CP_UTF8, 0, g_discord_app_id, -1,
                                             app_id, (int)sizeof(app_id), NULL, NULL);
        if (app_id_len <= 1) {
            discord_close_pipe();
            return FALSE;
        }
        char handshake[160];
        int count = snprintf(handshake, sizeof(handshake),
                             "{\"v\":1,\"client_id\":\"%s\"}", app_id);
        if (count > 0 && (size_t)count < sizeof(handshake) &&
            discord_write_frame(0, handshake, (uint32_t)count)) {
            discord_set_status(L"Connecting to Discord...");
            discord_mark_dirty();
            return TRUE;
        }
        discord_close_pipe();
    }
    discord_set_status(L"Discord is not running; Charter will keep trying.");
    return FALSE;
}

static BOOL discord_send_presence(void) {
    wchar_t state_w[256];
    const wchar_t *details_w = L"Browsing the library";
    wcscpy(state_w, L"Charter Media Player");
    BOOL active_track = g_playing_path[0] && (g_is_playing || g_is_paused);
    if (active_track) {
        details_w = g_playing_title[0] ? g_playing_title : L"Untitled media";
        swprintf(state_w, ARRAY_LEN(state_w), L"%ls \x2022 %.180ls",
                 g_is_paused ? L"Paused" : (g_playing_is_video ? L"Watching" : L"Listening"),
                 g_playing_artist[0] ? g_playing_artist : L"Unknown artist");
    }

    char details[1024], state[1536];
    discord_json_escape(details_w, 128, details, sizeof(details));
    discord_json_escape(state_w, 128, state, sizeof(state));

    char timestamps[256] = "";
    if (active_track && g_is_playing) {
        LONGLONG duration = player_duration_100ns();
        LONGLONG position = player_position_100ns();
        if (duration > 0) {
            long long now = (long long)time(NULL);
            long long start = now - max(0LL, position / 10000000LL);
            long long end = start + duration / 10000000LL;
            snprintf(timestamps, sizeof(timestamps),
                     "\"timestamps\":{\"start\":%lld,\"end\":%lld},", start, end);
        }
    }

    char payload[6144];
    unsigned long long nonce = ++g_discord_nonce;
    int count = snprintf(payload, sizeof(payload),
        "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%lu,\"activity\":{"
        "\"details\":\"%s\",\"state\":\"%s\",%s\"instance\":false}},\"nonce\":\"%llu\"}",
        GetCurrentProcessId(), details, state, timestamps, nonce);
    if (count <= 0 || (size_t)count >= sizeof(payload)) return FALSE;
    if (!discord_write_frame(1, payload, (uint32_t)count)) return FALSE;
    g_discord_last_update = GetTickCount64();
    InterlockedExchange(&g_discord_dirty, 0);
    discord_set_status(active_track ? L"Connected - sharing current playback"
                                    : L"Connected - sharing library activity");
    return TRUE;
}

static void discord_tick(void) {
    if (!g_discord_enabled) return;
    if (!discord_app_id_valid(g_discord_app_id)) {
        discord_disconnect(FALSE);
        discord_set_status(L"Enter a valid numeric Discord Application ID.");
        return;
    }

    ULONGLONG now = GetTickCount64();
    if (g_discord_pipe == INVALID_HANDLE_VALUE) {
        if (g_discord_last_attempt && now - g_discord_last_attempt < 10000) return;
        if (!discord_connect()) return;
    } else if (!discord_drain_pipe()) {
        discord_close_pipe();
        discord_set_status(L"Discord disconnected; Charter will reconnect.");
        g_discord_last_attempt = now;
        return;
    }

    if (InterlockedCompareExchange(&g_discord_dirty, 0, 0) ||
        !g_discord_last_update || now - g_discord_last_update >= 15000) {
        if (!discord_send_presence()) {
            discord_close_pipe();
            discord_set_status(L"Could not update Discord; Charter will retry.");
            g_discord_last_attempt = now;
        }
    }
}

static void release_wave_slot(HWAVEOUT wave, WaveBufferSlot *slot) {
    if (!slot) return;
    if (slot->prepared) {
        waveOutUnprepareHeader(wave, &slot->header, sizeof(slot->header));
        slot->prepared = FALSE;
    }
    free(slot->data);
    slot->data = NULL;
    slot->bytes = 0;
    ZeroMemory(&slot->header, sizeof(slot->header));
}

static BOOL wave_slot_done(const WaveBufferSlot *slot) {
    return !slot->prepared || (slot->header.dwFlags & WHDR_DONE) != 0;
}

static BOOL is_native_audio_extension(const wchar_t *path) {
    const wchar_t *dot = wcsrchr(path, L'.');
    if (!dot) return FALSE;
    static const wchar_t *native[] = {
        L".mp3", L".wav", L".wma", L".m4a", L".aac", L".flac"
    };
    for (size_t i = 0; i < ARRAY_LEN(native); ++i) {
        if (_wcsicmp(dot, native[i]) == 0) return TRUE;
    }
    return FALSE;
}

static BOOL make_compatible_audio_copy(const wchar_t *source, wchar_t *dest, size_t dest_count) {
    refresh_tool_paths();
    if (!g_ffmpeg_path[0]) return FALSE;
    wchar_t temp_root[MAX_PATH * 2] = L"";
    DWORD got = GetTempPathW((DWORD)ARRAY_LEN(temp_root), temp_root);
    if (!got || got >= ARRAY_LEN(temp_root)) return FALSE;
    swprintf(dest, dest_count, L"%lsCharterMedia-%lu-%lu.wav", temp_root,
             GetCurrentProcessId(), GetCurrentThreadId());
    DeleteFileW(dest);

    wchar_t command[MAX_PATH * 14];
    swprintf(command, ARRAY_LEN(command),
             L"\"%ls\" -nostdin -loglevel error -y -i \"%ls\" -vn "
             L"-ac 2 -ar 48000 -c:a pcm_s16le -f wav \"%ls\"",
             g_ffmpeg_path, source, dest);
    return run_process_wait(command) == 0 && file_exists(dest);
}

static HRESULT create_pcm_reader(const wchar_t *path, IMFSourceReader **reader_out,
                                 IMFMediaType **requested_out, IMFMediaType **actual_out,
                                 WAVEFORMATEX **wfx_out, UINT32 *wfx_size_out) {
    IMFSourceReader *reader = NULL;
    IMFMediaType *requested = NULL;
    IMFMediaType *actual = NULL;
    WAVEFORMATEX *wfx = NULL;
    UINT32 wfx_size = 0;

    HRESULT hr = MFCreateSourceReaderFromURL(path, NULL, &reader);
    if (FAILED(hr) || !reader) goto failed;
    IMFSourceReader_SetStreamSelection(reader, MF_SOURCE_READER_ALL_STREAMS, FALSE);
    IMFSourceReader_SetStreamSelection(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
    hr = MFCreateMediaType(&requested);
    if (FAILED(hr) || !requested) goto failed;
    IMFMediaType_SetGUID(requested, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(requested, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
    hr = IMFSourceReader_SetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                                             NULL, requested);
    if (FAILED(hr)) goto failed;
    hr = IMFSourceReader_GetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, &actual);
    if (FAILED(hr) || !actual) goto failed;
    hr = MFCreateWaveFormatExFromMFMediaType(actual, &wfx, &wfx_size,
                                             MFWaveFormatExConvertFlag_Normal);
    if (FAILED(hr) || !wfx) goto failed;

    *reader_out = reader;
    *requested_out = requested;
    *actual_out = actual;
    *wfx_out = wfx;
    *wfx_size_out = wfx_size;
    return S_OK;

failed:
    if (wfx) CoTaskMemFree(wfx);
    if (actual) IMFMediaType_Release(actual);
    if (requested) IMFMediaType_Release(requested);
    if (reader) IMFSourceReader_Release(reader);
    return FAILED(hr) ? hr : E_FAIL;
}

static DWORD WINAPI playback_thread_proc(LPVOID param) {
    wchar_t *path = (wchar_t *)param;
    wchar_t compatible_path[MAX_PATH * 4] = L"";
    const wchar_t *decode_path = path;
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    BOOL co_initialized = SUCCEEDED(hr);
    IMFSourceReader *reader = NULL;
    IMFMediaType *requested = NULL;
    IMFMediaType *actual = NULL;
    WAVEFORMATEX *wfx = NULL;
    UINT32 wfx_size = 0;
    HANDLE wave_event = NULL;
    HWAVEOUT wave = NULL;
    WaveBufferSlot slots[5];
    ZeroMemory(slots, sizeof(slots));
    BOOL failed = FALSE;
    BOOL natural_end = FALSE;
    LONGLONG start_position = InterlockedCompareExchange64(&g_player_position, 0, 0);

    if (!is_native_audio_extension(path) &&
        make_compatible_audio_copy(path, compatible_path, ARRAY_LEN(compatible_path))) {
        decode_path = compatible_path;
    }

    hr = create_pcm_reader(decode_path, &reader, &requested, &actual, &wfx, &wfx_size);
    if (FAILED(hr) && !compatible_path[0] &&
        make_compatible_audio_copy(path, compatible_path, ARRAY_LEN(compatible_path))) {
        decode_path = compatible_path;
        hr = create_pcm_reader(decode_path, &reader, &requested, &actual, &wfx, &wfx_size);
    }
    if (FAILED(hr)) { failed = TRUE; goto done; }

    PROPVARIANT duration;
    PropVariantInit(&duration);
    if (SUCCEEDED(IMFSourceReader_GetPresentationAttribute(reader, MF_SOURCE_READER_MEDIASOURCE,
                                                            &MF_PD_DURATION, &duration)) &&
        duration.vt == VT_UI8) {
        InterlockedExchange64(&g_player_duration, (LONG64)duration.uhVal.QuadPart);
    } else if (duration.vt == VT_I8) {
        InterlockedExchange64(&g_player_duration, (LONG64)duration.hVal.QuadPart);
    }
    PropVariantClear(&duration);

    if (start_position > 0) {
        PROPVARIANT pos;
        PropVariantInit(&pos);
        pos.vt = VT_I8;
        pos.hVal.QuadPart = start_position;
        IMFSourceReader_SetCurrentPosition(reader, &GUID_NULL, &pos);
        PropVariantClear(&pos);
    }

    wave_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!wave_event) { failed = TRUE; goto done; }

    UINT device_id = WAVE_MAPPER;
    if (g_audio_output_index >= 0 && g_audio_output_index < g_audio_output_count)
        device_id = g_audio_outputs[g_audio_output_index].device_id;

    MMRESULT mm = waveOutOpen(&wave, device_id, wfx, (DWORD_PTR)wave_event, 0, CALLBACK_EVENT);
    if (mm != MMSYSERR_NOERROR || !wave) { failed = TRUE; goto done; }

    EnterCriticalSection(&g_audio_lock);
    g_waveout = wave;
    LeaveCriticalSection(&g_audio_lock);
    apply_player_volume();
    if (g_is_paused) waveOutPause(wave);

    for (;;) {
        if (WaitForSingleObject(g_playback_stop_event, 0) == WAIT_OBJECT_0) break;
        if (g_is_paused) {
            Sleep(15);
            continue;
        }

        int slot_index = -1;
        for (int i = 0; i < (int)ARRAY_LEN(slots); ++i) {
            if (wave_slot_done(&slots[i])) {
                if (slots[i].prepared) release_wave_slot(wave, &slots[i]);
                slot_index = i;
                break;
            }
        }
        if (slot_index < 0) {
            HANDLE waits[2] = { g_playback_stop_event, wave_event };
            DWORD wr = WaitForMultipleObjects(2, waits, FALSE, 50);
            if (wr == WAIT_OBJECT_0) break;
            continue;
        }

        DWORD flags = 0;
        LONGLONG timestamp = 0;
        IMFSample *sample = NULL;
        hr = IMFSourceReader_ReadSample(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0,
                                        NULL, &flags, &timestamp, &sample);
        if (FAILED(hr)) {
            if (sample) IMFSample_Release(sample);
            failed = TRUE;
            break;
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            if (sample) IMFSample_Release(sample);
            natural_end = TRUE;
            break;
        }
        if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
            if (sample) IMFSample_Release(sample);
            failed = TRUE;
            break;
        }
        if (!sample) continue;

        IMFMediaBuffer *buffer = NULL;
        hr = IMFSample_ConvertToContiguousBuffer(sample, &buffer);
        IMFSample_Release(sample);
        if (FAILED(hr) || !buffer) { failed = TRUE; break; }

        BYTE *data = NULL;
        DWORD max_len = 0, cur_len = 0;
        hr = IMFMediaBuffer_Lock(buffer, &data, &max_len, &cur_len);
        if (FAILED(hr) || !data || cur_len == 0) {
            if (SUCCEEDED(hr)) IMFMediaBuffer_Unlock(buffer);
            IMFMediaBuffer_Release(buffer);
            if (FAILED(hr)) failed = TRUE;
            continue;
        }

        WaveBufferSlot *slot = &slots[slot_index];
        slot->data = (BYTE *)malloc(cur_len);
        if (!slot->data) {
            IMFMediaBuffer_Unlock(buffer);
            IMFMediaBuffer_Release(buffer);
            failed = TRUE;
            break;
        }
        memcpy(slot->data, data, cur_len);
        slot->bytes = cur_len;
        IMFMediaBuffer_Unlock(buffer);
        IMFMediaBuffer_Release(buffer);

        ZeroMemory(&slot->header, sizeof(slot->header));
        slot->header.lpData = (LPSTR)slot->data;
        slot->header.dwBufferLength = cur_len;
        mm = waveOutPrepareHeader(wave, &slot->header, sizeof(slot->header));
        if (mm != MMSYSERR_NOERROR) { failed = TRUE; break; }
        slot->prepared = TRUE;
        mm = waveOutWrite(wave, &slot->header, sizeof(slot->header));
        if (mm != MMSYSERR_NOERROR) { failed = TRUE; break; }

        InterlockedExchange64(&g_player_position, timestamp);
    }

    if (natural_end && !failed && WaitForSingleObject(g_playback_stop_event, 0) != WAIT_OBJECT_0) {
        BOOL pending = TRUE;
        while (pending && WaitForSingleObject(g_playback_stop_event, 0) != WAIT_OBJECT_0) {
            pending = FALSE;
            for (int i = 0; i < (int)ARRAY_LEN(slots); ++i) {
                if (slots[i].prepared && !wave_slot_done(&slots[i])) { pending = TRUE; break; }
            }
            if (pending) WaitForSingleObject(wave_event, 30);
        }
        InterlockedExchange64(&g_player_position, player_duration_100ns());
    }

done:
    if (wave) {
        waveOutReset(wave);
        for (int i = 0; i < (int)ARRAY_LEN(slots); ++i) release_wave_slot(wave, &slots[i]);
        EnterCriticalSection(&g_audio_lock);
        if (g_waveout == wave) g_waveout = NULL;
        LeaveCriticalSection(&g_audio_lock);
        waveOutClose(wave);
    } else {
        for (int i = 0; i < (int)ARRAY_LEN(slots); ++i) free(slots[i].data);
    }
    if (wave_event) CloseHandle(wave_event);
    if (wfx) CoTaskMemFree(wfx);
    if (actual) IMFMediaType_Release(actual);
    if (requested) IMFMediaType_Release(requested);
    if (reader) IMFSourceReader_Release(reader);
    if (compatible_path[0]) DeleteFileW(compatible_path);
    free(path);
    if (co_initialized) CoUninitialize();

    InterlockedExchange(&g_playback_failed, failed ? 1 : 0);
    if (WaitForSingleObject(g_playback_stop_event, 0) != WAIT_OBJECT_0) {
        PostMessageW(g_main, WM_APP_AUDIO_COMPLETE, (WPARAM)(failed ? 1 : 0), 0);
    }
    return 0;
}

static void release_graph(void) {
    if (g_video_player) {
        IMFPMediaPlayer_Stop(g_video_player);
        IMFPMediaPlayer_Shutdown(g_video_player);
        IMFPMediaPlayer_Release(g_video_player);
        g_video_player = NULL;
    }
    if (g_video_window) ShowWindow(g_video_window, SW_HIDE);
    if (g_video_seek) slider_set_value(g_video_seek, 0);
    if (g_playback_stop_event) SetEvent(g_playback_stop_event);
    EnterCriticalSection(&g_audio_lock);
    if (g_waveout) waveOutReset(g_waveout);
    LeaveCriticalSection(&g_audio_lock);
    if (g_playback_thread) {
        WaitForSingleObject(g_playback_thread, INFINITE);
        CloseHandle(g_playback_thread);
        g_playback_thread = NULL;
    }
    if (g_playback_stop_event) {
        CloseHandle(g_playback_stop_event);
        g_playback_stop_event = NULL;
    }
    EnterCriticalSection(&g_audio_lock);
    g_waveout = NULL;
    LeaveCriticalSection(&g_audio_lock);
}

static void set_playing_snapshot(const Track *track) {
    if (!track) return;
    wcsncpy(g_playing_path, track->path ? track->path : L"", ARRAY_LEN(g_playing_path) - 1);
    wcsncpy(g_playing_title, track->title ? track->title : L"Untitled media",
            ARRAY_LEN(g_playing_title) - 1);
    wcsncpy(g_playing_artist, track->artist ? track->artist : L"Unknown artist",
            ARRAY_LEN(g_playing_artist) - 1);
    g_playing_path[ARRAY_LEN(g_playing_path) - 1] = L'\0';
    g_playing_title[ARRAY_LEN(g_playing_title) - 1] = L'\0';
    g_playing_artist[ARRAY_LEN(g_playing_artist) - 1] = L'\0';
    g_playing_is_video = track->is_video;
}

static void stop_playback(void) {
    release_graph();
    g_is_playing = FALSE;
    g_is_paused = FALSE;
    g_playing_path[0] = L'\0';
    g_playing_title[0] = L'\0';
    g_playing_artist[0] = L'\0';
    InterlockedExchange64(&g_player_position, 0);
    InterlockedExchange64(&g_player_duration, 0);
    if (g_seek) slider_set_value(g_seek, 0);
    discord_mark_dirty();
    if (g_main) {
        RECT rc;
        GetClientRect(g_main, &rc);
        rc.top = max(0, rc.bottom - S(110));
        InvalidateRect(g_main, &rc, FALSE);
    }
}

static BOOL play_track_index_at(size_t idx, LONGLONG start_position, BOOL start_paused) {
    if (idx >= g_track_count) return FALSE;
    if (!g_tracks[idx].is_video && !is_native_audio_extension(g_tracks[idx].path)) {
        refresh_tool_paths();
        if (!g_ffmpeg_path[0]) {
            g_pending_playback_index = idx;
            g_pending_playback_position = start_position;
            g_pending_playback_paused = start_paused;
            InterlockedExchange(&g_pending_playback, 1);
            InterlockedExchange(&g_pending_need_ffmpeg, 1);
            start_install_tools();
            set_status(L"Preparing Charter's compatibility decoder, then playback will start automatically...");
            return TRUE;
        }
    }
    release_graph();

    if (g_tracks[idx].is_video) {
        if (!g_video_window) {
            set_status(L"Charter could not create the in-app video surface.");
            return FALSE;
        }
        wchar_t caption[768];
        swprintf(caption, ARRAY_LEN(caption), L"%ls - Charter Video Player", g_tracks[idx].title);
        SetWindowTextW(g_video_window, caption);
        ShowWindow(g_video_window, SW_SHOW);
        SetForegroundWindow(g_video_window);

        HRESULT video_hr = MFPCreateMediaPlayer(g_tracks[idx].path, start_paused ? FALSE : TRUE,
            MFP_OPTION_NONE, &g_video_callback.iface, g_video_surface, &g_video_player);
        if (FAILED(video_hr) || !g_video_player) {
            ShowWindow(g_video_window, SW_HIDE);
            g_video_player = NULL;
            set_status(L"Charter could not open this video. The file may use an unavailable codec.");
            return FALSE;
        }
        IMFPMediaPlayer_SetBorderColor(g_video_player, RGB(0, 0, 0));
        IMFPMediaPlayer_SetAspectRatioMode(g_video_player, MFVideoARMode_PreservePicture);
        apply_player_volume();
        if (start_position > 0) {
            PROPVARIANT pos;
            PropVariantInit(&pos);
            pos.vt = VT_I8;
            pos.hVal.QuadPart = start_position;
            IMFPMediaPlayer_SetPosition(g_video_player, &MFP_POSITIONTYPE_100NS, &pos);
            PropVariantClear(&pos);
        }
        g_current_track_index = idx;
        g_is_paused = start_paused;
        g_is_playing = !start_paused;
        update_video_play_label();
        InterlockedExchange64(&g_player_position, max(0, start_position));
        InterlockedExchange64(&g_player_duration, 0);
        wchar_t status[1024];
        swprintf(status, ARRAY_LEN(status), start_paused ? L"Paused %ls" : L"Playing video %ls",
                 g_tracks[idx].title);
        set_status(status);
        set_playing_snapshot(&g_tracks[idx]);
        discord_mark_dirty();
        return TRUE;
    }

    wchar_t *path_copy = dup_wstr(g_tracks[idx].path);
    if (!path_copy) return FALSE;
    g_playback_stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g_playback_stop_event) { free(path_copy); return FALSE; }

    InterlockedExchange64(&g_player_position, max(0, start_position));
    InterlockedExchange64(&g_player_duration, 0);
    InterlockedExchange(&g_playback_failed, 0);
    g_current_track_index = idx;
    g_is_paused = start_paused;
    g_is_playing = !start_paused;

    g_playback_thread = CreateThread(NULL, 0, playback_thread_proc, path_copy, 0, NULL);
    if (!g_playback_thread) {
        CloseHandle(g_playback_stop_event);
        g_playback_stop_event = NULL;
        free(path_copy);
        g_is_playing = FALSE;
        g_is_paused = FALSE;
        set_status(L"Charter could not start the in-app audio engine.");
        return FALSE;
    }

    wchar_t status[1024];
    swprintf(status, ARRAY_LEN(status), start_paused ? L"Paused %ls" : L"Playing %ls",
             g_tracks[idx].title);
    set_status(status);
    set_playing_snapshot(&g_tracks[idx]);
    discord_mark_dirty();
    return TRUE;
}

static void player_seek_to(LONGLONG position) {
    if (g_current_track_index >= g_track_count) return;
    LONGLONG duration = player_duration_100ns();
    if (position < 0) position = 0;
    if (duration > 0 && position > duration) position = duration;
    if (g_video_player) {
        PROPVARIANT pos;
        PropVariantInit(&pos);
        pos.vt = VT_I8;
        pos.hVal.QuadPart = position;
        if (SUCCEEDED(IMFPMediaPlayer_SetPosition(g_video_player, &MFP_POSITIONTYPE_100NS, &pos))) {
            InterlockedExchange64(&g_player_position, position);
            discord_mark_dirty();
        }
        PropVariantClear(&pos);
        return;
    }
    BOOL paused = g_is_paused;
    play_track_index_at(g_current_track_index, position, paused);
}

static void play_selected(void) {
    size_t idx = selected_track_index();
    if (idx == (size_t)-1) {
        set_status(L"Select a track first.");
        return;
    }
    play_track_index_at(idx, 0, FALSE);
}

static void pause_resume(void) {
    if (!g_playing_path[0] || (!g_is_playing && !g_is_paused)) {
        play_selected();
        return;
    }
    if (g_video_player) {
        if (g_is_paused) IMFPMediaPlayer_Play(g_video_player);
        else IMFPMediaPlayer_Pause(g_video_player);
    } else {
        EnterCriticalSection(&g_audio_lock);
        if (g_waveout) {
            if (g_is_paused) waveOutRestart(g_waveout);
            else waveOutPause(g_waveout);
        }
        LeaveCriticalSection(&g_audio_lock);
    }

    if (g_is_paused) {
        g_is_paused = FALSE;
        g_is_playing = TRUE;
        set_status(L"Playback resumed.");
    } else {
        g_is_paused = TRUE;
        g_is_playing = FALSE;
        set_status(L"Playback paused.");
    }
    update_video_play_label();
    discord_mark_dirty();
    if (g_main) {
        RECT rc;
        GetClientRect(g_main, &rc);
        rc.top = max(0, rc.bottom - S(110));
        InvalidateRect(g_main, &rc, FALSE);
    }
}

static int current_row_in_list(void) {
    if (g_current_track_index == (size_t)-1) return -1;
    int count = ListView_GetItemCount(g_list);
    for (int row = 0; row < count; ++row) {
        LVITEMW item;
        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_PARAM;
        item.iItem = row;
        if (ListView_GetItem(g_list, &item) && (size_t)item.lParam == g_current_track_index) return row;
    }
    return -1;
}

static void play_row(int row) {
    int count = ListView_GetItemCount(g_list);
    if (count <= 0 || row < 0 || row >= count) return;
    LVITEMW item;
    ZeroMemory(&item, sizeof(item));
    item.mask = LVIF_PARAM;
    item.iItem = row;
    if (!ListView_GetItem(g_list, &item)) return;
    size_t idx = (size_t)item.lParam;
    ListView_SetItemState(g_list, row, LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(g_list, row, FALSE);
    play_track_index_at(idx, 0, FALSE);
}

static void play_next_track(void) {
    int count = ListView_GetItemCount(g_list);
    if (count <= 0) return;
    int row = current_row_in_list();
    if (row < 0) row = selected_row();
    if (g_shuffle_enabled && count > 1) {
        int previous = row;
        do { row = rand() % count; } while (row == previous);
    } else {
        row = (row < 0) ? 0 : ((row + 1) % count);
    }
    play_row(row);
}

static void play_after_completion(void) {
    if (g_loop_enabled && g_current_track_index < g_track_count)
        play_track_index_at(g_current_track_index, 0, FALSE);
    else
        play_next_track();
}

static void play_previous_track(void) {
    int count = ListView_GetItemCount(g_list);
    if (count <= 0) return;
    int row = current_row_in_list();
    if (row < 0) row = selected_row();
    row = (row < 0) ? 0 : ((row - 1 + count) % count);
    play_row(row);
}

static void restart_on_selected_output(void) {
    if (g_current_track_index == (size_t)-1 || (!g_is_playing && !g_is_paused)) return;
    if (g_video_player) {
        set_status(L"Video playback uses the Windows default output device.");
        return;
    }
    LONGLONG pos = player_position_100ns();
    BOOL paused = g_is_paused;
    play_track_index_at(g_current_track_index, pos, paused);
}

static void open_selected_folder(void) {
    Track *t = selected_track();
    if (!t) {
        set_status(L"Select a track first.");
        return;
    }

    wchar_t args[MAX_PATH * 4 + 32];
    swprintf(args, ARRAY_LEN(args), L"/select,\"%ls\"", t->path);
    ShellExecuteW(g_main, L"open", L"explorer.exe", args, NULL, SW_SHOWNORMAL);
}
