/* Downloader, portable tools, and format selection. */

static void set_status(const wchar_t *text) {
    if (!text) text = L"";
    wcsncpy(g_status_text, text, ARRAY_LEN(g_status_text) - 1);
    g_status_text[ARRAY_LEN(g_status_text) - 1] = L'\0';
    if (g_main) InvalidateRect(g_main, NULL, FALSE);
}

static void set_download_status(const wchar_t *text, int percent) {
    if (!text) text = L"";
    wcsncpy(g_download_status, text, ARRAY_LEN(g_download_status) - 1);
    g_download_status[ARRAY_LEN(g_download_status) - 1] = L'\0';
    g_download_percent = percent;
    if (g_main) InvalidateRect(g_main, NULL, FALSE);
}

static BOOL file_exists(const wchar_t *path) {
    if (!path || !path[0]) return FALSE;
    DWORD attr = GetFileAttributesW(path);
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static BOOL is_http_url(const wchar_t *url) {
    return url && (_wcsnicmp(url, L"https://", 8) == 0 || _wcsnicmp(url, L"http://", 7) == 0);
}

static BOOL find_tool(const wchar_t *name, wchar_t *out, size_t out_count) {
    if (!name || !out || out_count == 0) return FALSE;
    out[0] = L'\0';

    if (g_tools_root[0]) {
        wchar_t local[MAX_PATH * 4];
        swprintf(local, ARRAY_LEN(local), L"%ls\\%ls", g_tools_root, name);
        if (file_exists(local)) {
            wcsncpy(out, local, out_count - 1);
            out[out_count - 1] = L'\0';
            return TRUE;
        }
    }

    wchar_t local_app[MAX_PATH * 4] = L"";
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app, (DWORD)ARRAY_LEN(local_app));
    if (n > 0 && n < ARRAY_LEN(local_app)) {
        wchar_t link_path[MAX_PATH * 4];
        swprintf(link_path, ARRAY_LEN(link_path), L"%ls\\Microsoft\\WinGet\\Links\\%ls", local_app, name);
        if (file_exists(link_path)) {
            wcsncpy(out, link_path, out_count - 1);
            out[out_count - 1] = L'\0';
            return TRUE;
        }

        wchar_t windows_apps[MAX_PATH * 4];
        swprintf(windows_apps, ARRAY_LEN(windows_apps), L"%ls\\Microsoft\\WindowsApps\\%ls", local_app, name);
        if (file_exists(windows_apps)) {
            wcsncpy(out, windows_apps, out_count - 1);
            out[out_count - 1] = L'\0';
            return TRUE;
        }
    }

    DWORD got = SearchPathW(NULL, name, NULL, (DWORD)out_count, out, NULL);
    return got > 0 && got < out_count && file_exists(out);
}

static BOOL find_file_recursive(const wchar_t *root, const wchar_t *filename,
                                wchar_t *out, size_t out_count, int depth) {
    if (!root || !filename || !out || out_count == 0 || depth > 8) return FALSE;

    wchar_t pattern[MAX_PATH * 4];
    swprintf(pattern, ARRAY_LEN(pattern), L"%ls\\*", root);

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return FALSE;

    BOOL found = FALSE;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

        wchar_t path[MAX_PATH * 4];
        swprintf(path, ARRAY_LEN(path), L"%ls\\%ls", root, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (find_file_recursive(path, filename, out, out_count, depth + 1)) {
                found = TRUE;
                break;
            }
        } else if (_wcsicmp(fd.cFileName, filename) == 0) {
            wcsncpy(out, path, out_count - 1);
            out[out_count - 1] = L'\0';
            found = TRUE;
            break;
        }
    } while (FindNextFileW(h, &fd));

    FindClose(h);
    return found;
}

static void refresh_tool_paths(void) {
    find_tool(L"yt-dlp.exe", g_ytdlp_path, ARRAY_LEN(g_ytdlp_path));
    find_tool(L"qjs.exe", g_qjs_path, ARRAY_LEN(g_qjs_path));
    if (!find_tool(L"ffmpeg.exe", g_ffmpeg_path, ARRAY_LEN(g_ffmpeg_path))) {
        g_ffmpeg_path[0] = L'\0';
        if (g_tools_root[0]) {
            find_file_recursive(g_tools_root, L"ffmpeg.exe", g_ffmpeg_path,
                                ARRAY_LEN(g_ffmpeg_path), 0);
        }
    }
}

static int format_menu_id(void) {
    if (_wcsicmp(g_download_format, L"MP3") == 0) return ID_FMT_MP3;
    if (_wcsicmp(g_download_format, L"M4A") == 0) return ID_FMT_M4A;
    if (_wcsicmp(g_download_format, L"OPUS") == 0) return ID_FMT_OPUS;
    if (_wcsicmp(g_download_format, L"FLAC") == 0) return ID_FMT_FLAC;
    if (_wcsicmp(g_download_format, L"WAV") == 0) return ID_FMT_WAV;
    if (_wcsicmp(g_download_format, L"MP4") == 0) return ID_FMT_MP4;
    return ID_FMT_ORIGINAL;
}

static BOOL format_needs_ffmpeg(void) {
    /* FFmpeg is also Charter's fallback decoder for an original-format download. */
    return TRUE;
}

static const wchar_t *format_args(void) {
    static wchar_t video_args[1024];
    if (_wcsicmp(g_download_format, L"MP3") == 0)
        return L"-x --audio-format mp3 --audio-quality 0 --embed-thumbnail";
    if (_wcsicmp(g_download_format, L"M4A") == 0)
        return L"-x --audio-format m4a --audio-quality 0 --embed-thumbnail";
    if (_wcsicmp(g_download_format, L"OPUS") == 0)
        return L"-x --audio-format opus --audio-quality 0 --embed-thumbnail";
    if (_wcsicmp(g_download_format, L"FLAC") == 0)
        return L"-x --audio-format flac --audio-quality 0 --embed-thumbnail";
    if (_wcsicmp(g_download_format, L"WAV") == 0)
        return L"-x --audio-format wav --audio-quality 0";
    if (_wcsicmp(g_download_format, L"MP4") == 0) {
        if (_wcsicmp(g_video_resolution, L"BEST") == 0) {
            wcscpy(video_args,
                L"-f \"bv*[vcodec^=avc1]+ba[acodec^=mp4a]/b[ext=mp4]/best\" "
                L"--merge-output-format mp4 --recode-video mp4 --embed-thumbnail");
        } else {
            swprintf(video_args, ARRAY_LEN(video_args),
                L"-f \"bv*[height<=%ls][vcodec^=avc1]+ba[acodec^=mp4a]/"
                L"b[height<=%ls][ext=mp4]/best[height<=%ls]\" "
                L"--merge-output-format mp4 --recode-video mp4 --embed-thumbnail",
                g_video_resolution, g_video_resolution, g_video_resolution);
        }
        return video_args;
    }
    return L"-f bestaudio/best";
}

static void post_download_update(int percent, const wchar_t *text) {
    DownloadUpdate *update = (DownloadUpdate *)calloc(1, sizeof(DownloadUpdate));
    if (!update) return;
    update->percent = percent;
    update->job = NULL;
    wcsncpy(update->text, text ? text : L"", ARRAY_LEN(update->text) - 1);
    update->text[ARRAY_LEN(update->text) - 1] = L'\0';
    if (!PostMessageW(g_main, WM_APP_DOWNLOAD_UPDATE, 0, (LPARAM)update)) {
        free(update);
    }
}

static void post_job_update(void *job, int percent, const wchar_t *text) {
    DownloadUpdate *update = (DownloadUpdate *)calloc(1, sizeof(DownloadUpdate));
    if (!update) return;
    update->job = job;
    update->percent = percent;
    wcsncpy(update->text, text ? text : L"", ARRAY_LEN(update->text) - 1);
    if (!PostMessageW(g_main, WM_APP_DOWNLOAD_UPDATE, 0, (LPARAM)update)) free(update);
}

static int parse_progress_percent(const wchar_t *line) {
    if (!line) return -1;
    const wchar_t *pct = wcschr(line, L'%');
    while (pct) {
        const wchar_t *start = pct;
        while (start > line) {
            wchar_t c = start[-1];
            if (iswdigit(c) || c == L'.' || c == L' ') start--;
            else break;
        }
        while (*start == L' ') start++;
        wchar_t *end = NULL;
        double value = wcstod(start, &end);
        if (end && end <= pct && value >= 0.0 && value <= 100.0) {
            return (int)(value + 0.5);
        }
        pct = wcschr(pct + 1, L'%');
    }
    return -1;
}

typedef struct DownloadJob {
    wchar_t *command;
    HANDLE process;
    HANDLE thread;
    unsigned id;
    wchar_t url[4096];
    wchar_t format[64];
    wchar_t status[256];
    int percent;
    wchar_t last_error[1024];
} DownloadJob;

static void post_utf8_line(DownloadJob *job, const char *line, int len) {
    if (!line || len <= 0) return;
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) len--;
    if (len <= 0) return;

    int needed = MultiByteToWideChar(CP_UTF8, 0, line, len, NULL, 0);
    if (needed <= 0) return;
    wchar_t *wide = (wchar_t *)calloc((size_t)needed + 1, sizeof(wchar_t));
    if (!wide) return;
    MultiByteToWideChar(CP_UTF8, 0, line, len, wide, needed);
    wide[needed] = L'\0';

    int percent = parse_progress_percent(wide);
    if (wcsstr(wide, L"ERROR:") || wcsstr(wide, L"Error:") || wcsstr(wide, L"error:")) {
        if (job) {
            wcsncpy(job->last_error, wide, ARRAY_LEN(job->last_error) - 1);
            job->last_error[ARRAY_LEN(job->last_error) - 1] = L'\0';
        }
    }
    post_job_update(job, percent, wide);
    free(wide);
}

static DownloadJob *g_download_jobs[32];
static size_t g_download_job_count;
static unsigned g_next_download_id = 1;

static void remember_download_job(DownloadJob *job) {
    if (job && g_download_job_count < ARRAY_LEN(g_download_jobs))
        g_download_jobs[g_download_job_count++] = job;
}

static void forget_download_job(DownloadJob *job) {
    for (size_t i = 0; i < g_download_job_count; ++i) {
        if (g_download_jobs[i] != job) continue;
        if (i + 1 < g_download_job_count)
            memmove(&g_download_jobs[i], &g_download_jobs[i + 1],
                    (g_download_job_count - i - 1) * sizeof(g_download_jobs[0]));
        --g_download_job_count;
        return;
    }
}

static DWORD WINAPI download_thread_proc(LPVOID param) {
    DownloadJob *job = (DownloadJob *)param;
    if (!job || !job->command) {
        PostMessageW(g_main, WM_APP_DOWNLOAD_DONE, 1, (LPARAM)job);
        return 0;
    }

    SECURITY_ATTRIBUTES sa;
    ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_pipe = NULL;
    HANDLE write_pipe = NULL;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        post_job_update(job, -1, L"Could not create the downloader output pipe.");
        PostMessageW(g_main, WM_APP_DOWNLOAD_DONE, 1, (LPARAM)job);
        return 0;
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    BOOL ok = CreateProcessW(NULL, job->command, NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(write_pipe);

    if (!ok) {
        wchar_t message[256];
        swprintf(message, ARRAY_LEN(message), L"Could not start yt-dlp. Windows error %lu.", GetLastError());
        post_job_update(job, -1, message);
        CloseHandle(read_pipe);
        PostMessageW(g_main, WM_APP_DOWNLOAD_DONE, 1, (LPARAM)job);
        return 0;
    }

    CloseHandle(pi.hThread);
    job->process = pi.hProcess;

    char buffer[2048];
    char line[8192];
    int line_len = 0;
    DWORD bytes_read = 0;

    while (ReadFile(read_pipe, buffer, sizeof(buffer), &bytes_read, NULL) && bytes_read > 0) {
        for (DWORD i = 0; i < bytes_read; ++i) {
            char c = buffer[i];
            if (c == '\n') {
                if (line_len > 0) post_utf8_line(job, line, line_len);
                line_len = 0;
            } else if (line_len < (int)sizeof(line) - 1) {
                line[line_len++] = c;
            }
        }
    }
    if (line_len > 0) post_utf8_line(job, line, line_len);

    CloseHandle(read_pipe);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    PostMessageW(g_main, WM_APP_DOWNLOAD_DONE, (WPARAM)exit_code, (LPARAM)job);
    return 0;
}

static void cancel_all_downloads(void) {
    for (size_t i = 0; i < g_download_job_count; ++i)
        if (g_download_jobs[i] && g_download_jobs[i]->process)
            TerminateProcess(g_download_jobs[i]->process, 2);
}

static void shutdown_download_jobs(void) {
    cancel_all_downloads();
    for (size_t i = 0; i < g_download_job_count; ++i) {
        DownloadJob *job = g_download_jobs[i];
        if (!job) continue;
        if (job->thread) WaitForSingleObject(job->thread, 3000);
        if (job->process) CloseHandle(job->process);
        if (job->thread) CloseHandle(job->thread);
        free(job->command);
        free(job);
    }
    g_download_job_count = 0;
    InterlockedExchange(&g_downloading, 0);
}

static void start_download(void) {
    if (g_download_job_count >= ARRAY_LEN(g_download_jobs)) {
        set_download_status(L"The download queue is full. Wait for an active download to finish.", -1);
        return;
    }
    wchar_t url[4096];
    GetWindowTextW(g_url_edit, url, ARRAY_LEN(url));
    if (!url[0]) {
        set_download_status(L"Paste a link first.", -1);
        SetFocus(g_url_edit);
        return;
    }
    if (!is_http_url(url)) {
        set_download_status(L"That does not look like an HTTP or HTTPS link.", -1);
        return;
    }

    refresh_tool_paths();
    BOOL need_ffmpeg = format_needs_ffmpeg();
    if (!g_ytdlp_path[0] || !g_qjs_path[0] || (need_ffmpeg && !g_ffmpeg_path[0])) {
        InterlockedExchange(&g_pending_download, 1);
        InterlockedExchange(&g_pending_need_ffmpeg, need_ffmpeg ? 1 : 0);
        start_install_tools();
        return;
    }

    wchar_t ffmpeg_arg[MAX_PATH * 4 + 64] = L"";
    if (g_ffmpeg_path[0]) {
        swprintf(ffmpeg_arg, ARRAY_LEN(ffmpeg_arg), L"--ffmpeg-location \"%ls\"", g_ffmpeg_path);
    }

    wchar_t js_arg[MAX_PATH * 4 + 96] = L"";
    if (g_qjs_path[0]) {
        swprintf(js_arg, ARRAY_LEN(js_arg),
                 L"--no-js-runtimes --js-runtimes \"quickjs:%ls\"", g_qjs_path);
    }

    const wchar_t *fmt = format_args();
    size_t command_cap = 16384;
    wchar_t *command = (wchar_t *)calloc(command_cap, sizeof(wchar_t));
    if (!command) {
        set_download_status(L"Not enough memory to start the download.", -1);
        return;
    }

    swprintf(command, command_cap,
             L"\"%ls\" --newline --no-color --encoding utf-8 --windows-filenames "
             L"--ignore-errors --embed-metadata --write-info-json --no-overwrites "
             L"%ls %ls %ls -P \"%ls\" "
             L"-o \"%%(title)s [%%(id)s].%%(ext)s\" \"%ls\"",
             g_ytdlp_path, js_arg, ffmpeg_arg, fmt, g_library_root, url);

    DownloadJob *job = (DownloadJob *)calloc(1, sizeof(DownloadJob));
    if (!job) {
        free(command);
        set_download_status(L"Not enough memory to start the download.", -1);
        return;
    }
    job->command = command;
    job->id = g_next_download_id++;
    wcsncpy(job->url, url, ARRAY_LEN(job->url) - 1);
    if (_wcsicmp(g_download_format, L"MP4") == 0 &&
        _wcsicmp(g_video_resolution, L"BEST") != 0) {
        swprintf(job->format, ARRAY_LEN(job->format), L"MP4 · %lsp", g_video_resolution);
    } else {
        wcsncpy(job->format, g_download_format, ARRAY_LEN(job->format) - 1);
    }
    wcscpy(job->status, L"Starting download...");
    job->percent = 0;

    InterlockedIncrement(&g_downloading);
    g_download_percent = 0;
    wchar_t queued[128];
    swprintf(queued, ARRAY_LEN(queued), L"Download #%u added. %ld active download%ls.",
             job->id, InterlockedCompareExchange(&g_downloading, 0, 0),
             InterlockedCompareExchange(&g_downloading, 0, 0) == 1 ? L"" : L"s");
    set_download_status(queued, 0);

    remember_download_job(job);
    job->thread = CreateThread(NULL, 0, download_thread_proc, job, 0, NULL);
    if (!job->thread) {
        forget_download_job(job);
        InterlockedDecrement(&g_downloading);
        free(job->command);
        free(job);
        set_download_status(L"Could not create the downloader worker thread.", -1);
    } else {
        SetWindowTextW(g_url_edit, L"");
        SetFocus(g_url_edit);
    }
}

static DWORD run_process_wait(wchar_t *command) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    if (!CreateProcessW(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi)) {
        return GetLastError() ? GetLastError() : 1;
    }

    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    return exit_code;
}

static BOOL download_portable_file(const wchar_t *url, const wchar_t *dest,
                                   const wchar_t *status) {
    if (status) post_download_update(-1, status);

    wchar_t temp[MAX_PATH * 4];
    swprintf(temp, ARRAY_LEN(temp), L"%ls.download", dest);
    DeleteFileW(temp);

    HRESULT hr = URLDownloadToFileW(NULL, url, temp, 0, NULL);
    if (FAILED(hr) || !file_exists(temp)) {
        DeleteFileW(temp);
        return FALSE;
    }

    DeleteFileW(dest);
    if (!MoveFileExW(temp, dest, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
        DeleteFileW(temp);
        return FALSE;
    }
    return TRUE;
}

static BOOL prepare_ytdlp_portable(void) {
    wchar_t dest[MAX_PATH * 4];
    swprintf(dest, ARRAY_LEN(dest), L"%ls\\yt-dlp.exe", g_tools_root);
    if (file_exists(dest)) return TRUE;

    return download_portable_file(
        L"https://github.com/yt-dlp/yt-dlp-nightly-builds/releases/latest/download/yt-dlp.exe",
        dest,
        L"Preparing the downloader for first use...");
}

static BOOL prepare_qjs_portable(void) {
    wchar_t dest[MAX_PATH * 4];
    swprintf(dest, ARRAY_LEN(dest), L"%ls\\qjs.exe", g_tools_root);
    if (file_exists(dest)) return TRUE;

    return download_portable_file(
        L"https://github.com/quickjs-ng/quickjs/releases/latest/download/qjs-windows-x86_64.exe",
        dest,
        L"Adding the lightweight YouTube compatibility engine...");
}

static BOOL prepare_ffmpeg_portable(void) {
    refresh_tool_paths();
    if (g_ffmpeg_path[0]) return TRUE;

    wchar_t archive[MAX_PATH * 4];
    wchar_t extract_dir[MAX_PATH * 4];
    swprintf(archive, ARRAY_LEN(archive), L"%ls\\ffmpeg-essentials.zip", g_tools_root);
    swprintf(extract_dir, ARRAY_LEN(extract_dir), L"%ls\\ffmpeg", g_tools_root);

    if (!file_exists(archive)) {
        if (!download_portable_file(
                L"https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip",
                archive,
                L"Preparing universal media conversion support for first use...")) {
            return FALSE;
        }
    }

    create_directory_if_needed(extract_dir);

    wchar_t tar_path[MAX_PATH * 4] = L"";
    if (!find_tool(L"tar.exe", tar_path, ARRAY_LEN(tar_path))) {
        post_download_update(-1, L"Windows archive support could not be found.");
        return FALSE;
    }

    post_download_update(-1, L"Finishing one-time media converter setup...");
    wchar_t command[MAX_PATH * 12];
    swprintf(command, ARRAY_LEN(command),
             L"\"%ls\" -xf \"%ls\" -C \"%ls\"",
             tar_path, archive, extract_dir);
    DWORD result = run_process_wait(command);
    if (result != 0) return FALSE;

    DeleteFileW(archive);
    refresh_tool_paths();
    return g_ffmpeg_path[0] != L'\0';
}

static DWORD WINAPI tools_thread_proc(LPVOID param) {
    (void)param;

    BOOL failed = FALSE;
    BOOL need_ffmpeg = InterlockedCompareExchange(&g_pending_need_ffmpeg, 0, 0) != 0;

    if (!prepare_ytdlp_portable()) failed = TRUE;
    if (!failed && !prepare_qjs_portable()) failed = TRUE;
    if (!failed && need_ffmpeg && !prepare_ffmpeg_portable()) failed = TRUE;

    refresh_tool_paths();
    PostMessageW(g_main, WM_APP_TOOLS_DONE, (WPARAM)(failed ? 1 : 0), 0);
    return 0;
}

static void start_install_tools(void) {
    if (InterlockedCompareExchange(&g_installing_tools, 0, 0)) return;
    if (InterlockedCompareExchange(&g_downloading, 0, 0)) return;

    InterlockedExchange(&g_installing_tools, 1);
    set_download_status(L"Preparing downloader components automatically...", -1);
    g_tools_thread = CreateThread(NULL, 0, tools_thread_proc, NULL, 0, NULL);
    if (!g_tools_thread) {
        InterlockedExchange(&g_installing_tools, 0);
        set_download_status(L"Could not start automatic downloader setup.", -1);
    }
}

static void show_format_menu(void) {
    if (!g_format) return;

    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    AppendMenuW(menu, MF_STRING, ID_FMT_MP3, L"MP3");
    AppendMenuW(menu, MF_STRING, ID_FMT_M4A, L"M4A");
    AppendMenuW(menu, MF_STRING, ID_FMT_OPUS, L"OPUS");
    AppendMenuW(menu, MF_STRING, ID_FMT_FLAC, L"FLAC");
    AppendMenuW(menu, MF_STRING, ID_FMT_WAV, L"WAV");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, ID_FMT_MP4, L"MP4 video");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, ID_FMT_ORIGINAL, L"Original audio");

    CheckMenuRadioItem(menu, ID_FMT_MP3, ID_FMT_MP4, format_menu_id(), MF_BYCOMMAND);

    RECT r;
    GetWindowRect(g_format, &r);
    SetForegroundWindow(g_main);
    UINT choice = TrackPopupMenu(menu,
                                 TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                                 r.left, r.bottom + S(4), 0, g_main, NULL);
    DestroyMenu(menu);

    const wchar_t *chosen = NULL;
    switch (choice) {
        case ID_FMT_MP3: chosen = L"MP3"; break;
        case ID_FMT_M4A: chosen = L"M4A"; break;
        case ID_FMT_OPUS: chosen = L"OPUS"; break;
        case ID_FMT_FLAC: chosen = L"FLAC"; break;
        case ID_FMT_WAV: chosen = L"WAV"; break;
        case ID_FMT_MP4: chosen = L"MP4"; break;
        case ID_FMT_ORIGINAL: chosen = L"ORIGINAL"; break;
    }
    if (chosen) {
        wcsncpy(g_download_format, chosen, ARRAY_LEN(g_download_format) - 1);
        g_download_format[ARRAY_LEN(g_download_format) - 1] = L'\0';
        SetWindowTextW(g_format, _wcsicmp(chosen, L"ORIGINAL") == 0 ? L"Original" : chosen);
        InvalidateRect(g_format, NULL, FALSE);
        update_page_visibility();
        layout_ui(g_main);
    }
}

static int resolution_menu_id(void) {
    if (_wcsicmp(g_video_resolution, L"2160") == 0) return ID_RES_2160;
    if (_wcsicmp(g_video_resolution, L"1440") == 0) return ID_RES_1440;
    if (_wcsicmp(g_video_resolution, L"1080") == 0) return ID_RES_1080;
    if (_wcsicmp(g_video_resolution, L"720") == 0) return ID_RES_720;
    if (_wcsicmp(g_video_resolution, L"480") == 0) return ID_RES_480;
    if (_wcsicmp(g_video_resolution, L"360") == 0) return ID_RES_360;
    return ID_RES_BEST;
}

static void show_resolution_menu(void) {
    if (!g_resolution) return;
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, ID_RES_BEST, L"Best available");
    AppendMenuW(menu, MF_STRING, ID_RES_2160, L"Up to 2160p (4K)");
    AppendMenuW(menu, MF_STRING, ID_RES_1440, L"Up to 1440p");
    AppendMenuW(menu, MF_STRING, ID_RES_1080, L"Up to 1080p");
    AppendMenuW(menu, MF_STRING, ID_RES_720, L"Up to 720p");
    AppendMenuW(menu, MF_STRING, ID_RES_480, L"Up to 480p");
    AppendMenuW(menu, MF_STRING, ID_RES_360, L"Up to 360p");
    CheckMenuRadioItem(menu, ID_RES_BEST, ID_RES_360, resolution_menu_id(), MF_BYCOMMAND);
    RECT r;
    GetWindowRect(g_resolution, &r);
    UINT choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                                 r.left, r.bottom + S(4), 0, g_main, NULL);
    DestroyMenu(menu);
    const wchar_t *value = NULL;
    switch (choice) {
        case ID_RES_BEST: value = L"BEST"; break;
        case ID_RES_2160: value = L"2160"; break;
        case ID_RES_1440: value = L"1440"; break;
        case ID_RES_1080: value = L"1080"; break;
        case ID_RES_720: value = L"720"; break;
        case ID_RES_480: value = L"480"; break;
        case ID_RES_360: value = L"360"; break;
    }
    if (value) {
        wcsncpy(g_video_resolution, value, ARRAY_LEN(g_video_resolution) - 1);
        g_video_resolution[ARRAY_LEN(g_video_resolution) - 1] = L'\0';
        wchar_t label[32];
        if (_wcsicmp(value, L"BEST") == 0) wcscpy(label, L"Best quality");
        else swprintf(label, ARRAY_LEN(label), L"%lsp", value);
        SetWindowTextW(g_resolution, label);
        InvalidateRect(g_resolution, NULL, FALSE);
    }
}
