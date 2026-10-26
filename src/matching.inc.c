/* Match a user-provided song name (or Spotify track preview) to YouTube. */

static void update_button_enabled_state(void);

typedef struct MatchCandidate {
    wchar_t id[16];
    wchar_t title[256];
    wchar_t channel[160];
    int score;
    HBITMAP thumbnail;
} MatchCandidate;

typedef struct MatchRequest {
    int source;
    LONG generation;
    wchar_t input[4096];
    wchar_t query[256];
    wchar_t error[256];
    MatchCandidate candidates[5];
    int count;
} MatchRequest;

typedef struct MatchThumbnailUpdate {
    LONG generation;
    int index;
    HBITMAP bitmap;
} MatchThumbnailUpdate;

static HANDLE g_match_thread;
static volatile LONG g_match_searching;
static MatchRequest *g_match_results;
static int g_match_scroll;
static int g_queue_scroll;
static volatile LONG g_match_generation;
static BOOL valid_youtube_id(const wchar_t *id);

static void free_match_request(MatchRequest *request) {
    if (!request) return;
    for (int i = 0; i < request->count; ++i)
        if (request->candidates[i].thumbnail)
            DeleteObject(request->candidates[i].thumbnail);
    free(request);
}

static void post_match_thumbnails(wchar_t ids[5][16], int count,
                                  LONG generation) {
    wchar_t temp_dir[MAX_PATH], temp_file[MAX_PATH];
    DWORD length = GetTempPathW(ARRAY_LEN(temp_dir), temp_dir);
    if (!length || length >= ARRAY_LEN(temp_dir)) return;
    for (int i = 0; i < count; ++i) {
        if (InterlockedCompareExchange(&g_match_generation, 0, 0) != generation)
            break;
        if (!valid_youtube_id(ids[i]) ||
            !GetTempFileNameW(temp_dir, L"cmt", 0, temp_file)) continue;
        wchar_t image_file[MAX_PATH + 8], image_url[128];
        swprintf(image_file, ARRAY_LEN(image_file), L"%ls.jpg", temp_file);
        swprintf(image_url, ARRAY_LEN(image_url),
                 L"https://i.ytimg.com/vi/%ls/mqdefault.jpg", ids[i]);
        HBITMAP bitmap = NULL;
        if (SUCCEEDED(URLDownloadToFileW(NULL, image_url, image_file, 0, NULL)))
            bitmap = shell_thumbnail_for_path(image_file, 160);
        DeleteFileW(image_file);
        DeleteFileW(temp_file);
        if (!bitmap) continue;
        MatchThumbnailUpdate *update = (MatchThumbnailUpdate *)calloc(1, sizeof(*update));
        if (!update) { DeleteObject(bitmap); continue; }
        update->generation = generation;
        update->index = i;
        update->bitmap = bitmap;
        if (InterlockedCompareExchange(&g_match_generation, 0, 0) != generation ||
            !PostMessageW(g_main, WM_APP_MATCH_THUMBNAIL, 0, (LPARAM)update)) {
            DeleteObject(bitmap);
            free(update);
        }
    }
}

static BOOL encode_url_query(const wchar_t *input, wchar_t *output, size_t capacity) {
    char utf8[12288];
    int bytes = WideCharToMultiByte(CP_UTF8, 0, input, -1, utf8,
                                    (int)sizeof(utf8), NULL, NULL);
    if (bytes <= 0) return FALSE;
    size_t used = 0;
    const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < bytes - 1; ++i) {
        unsigned char c = (unsigned char)utf8[i];
        BOOL plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                     (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        if (used + (plain ? 1 : 3) >= capacity) return FALSE;
        if (plain) output[used++] = (wchar_t)c;
        else {
            output[used++] = L'%';
            output[used++] = (wchar_t)hex[c >> 4];
            output[used++] = (wchar_t)hex[c & 15];
        }
    }
    output[used] = L'\0';
    return TRUE;
}

static BOOL fetch_spotify_title(const wchar_t *track_url, wchar_t *title, size_t title_count) {
    wchar_t encoded[16384];
    if (!encode_url_query(track_url, encoded, ARRAY_LEN(encoded))) return FALSE;
    wchar_t endpoint[16432];
    swprintf(endpoint, ARRAY_LEN(endpoint), L"https://open.spotify.com/oembed?url=%ls", encoded);

    wchar_t temp_dir[MAX_PATH], temp_file[MAX_PATH];
    DWORD length = GetTempPathW(ARRAY_LEN(temp_dir), temp_dir);
    if (!length || length >= ARRAY_LEN(temp_dir) ||
        !GetTempFileNameW(temp_dir, L"cmp", 0, temp_file)) return FALSE;
    HRESULT hr = URLDownloadToFileW(NULL, endpoint, temp_file, 0, NULL);
    if (FAILED(hr)) { DeleteFileW(temp_file); return FALSE; }

    HANDLE file = CreateFileW(temp_file, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) { DeleteFileW(temp_file); return FALSE; }
    char json[65537];
    DWORD read = 0;
    BOOL ok = ReadFile(file, json, sizeof(json) - 1, &read, NULL);
    CloseHandle(file);
    DeleteFileW(temp_file);
    if (!ok || !read) return FALSE;
    json[read] = '\0';
    if (!strstr(json, "/embed/track/")) return FALSE;

    const char *field = strstr(json, "\"title\"");
    if (!field) return FALSE;
    field = strchr(field + 7, ':');
    if (!field) return FALSE;
    ++field;
    while (*field == ' ' || *field == '\t') ++field;
    if (*field++ != '"') return FALSE;

    char raw[1024];
    size_t used = 0;
    while (*field && *field != '"' && used + 4 < sizeof(raw)) {
        if (*field == '\\') {
            ++field;
            if (!*field) break;
            if (*field == 'n' || *field == 'r' || *field == 't') raw[used++] = ' ';
            else if (*field == 'u') {
                /* oEmbed normally sends UTF-8; preserve common escaped ASCII. */
                if (strlen(field) < 5) break;
                char digits[5] = { field[1], field[2], field[3], field[4], 0 };
                char *end = NULL;
                unsigned long cp = strtoul(digits, &end, 16);
                if (!end || *end) return FALSE;
                if (cp < 0x80) raw[used++] = (char)cp;
                else if (cp < 0x800) {
                    raw[used++] = (char)(0xC0 | (cp >> 6));
                    raw[used++] = (char)(0x80 | (cp & 63));
                } else {
                    raw[used++] = (char)(0xE0 | (cp >> 12));
                    raw[used++] = (char)(0x80 | ((cp >> 6) & 63));
                    raw[used++] = (char)(0x80 | (cp & 63));
                }
                field += 4;
            } else raw[used++] = *field;
        } else raw[used++] = *field;
        ++field;
    }
    raw[used] = '\0';
    return MultiByteToWideChar(CP_UTF8, 0, raw, -1, title,
                                (int)title_count) > 1;
}

static void clean_match_query(const wchar_t *source, wchar_t *out, size_t capacity) {
    size_t used = 0;
    BOOL space = FALSE;
    for (const wchar_t *at = source; *at && used + 1 < capacity; ++at) {
        wchar_t c = *at;
        if (c == L'"' || c == L'\\' || c == L'|' || iswcntrl(c)) c = L' ';
        if (iswspace(c)) {
            if (used) space = TRUE;
        } else {
            if (space && used + 2 < capacity) out[used++] = L' ';
            space = FALSE;
            out[used++] = c;
        }
    }
    out[used] = L'\0';
}

static BOOL valid_youtube_id(const wchar_t *id) {
    if (wcslen(id) != 11) return FALSE;
    for (int i = 0; i < 11; ++i) {
        wchar_t c = id[i];
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
              (c >= L'0' && c <= L'9') || c == L'_' || c == L'-')) return FALSE;
    }
    return TRUE;
}

static void match_utf8_slice(const char *start, size_t bytes, wchar_t *out, size_t capacity) {
    if (!capacity) return;
    if (bytes > 1023) bytes = 1023;
    char raw[1024];
    memcpy(raw, start, bytes);
    raw[bytes] = '\0';
    if (!MultiByteToWideChar(CP_UTF8, 0, raw, -1, out, (int)capacity)) out[0] = L'\0';
}

static int score_match(const wchar_t *query, const MatchCandidate *candidate) {
    int score = 0;
    const wchar_t *at = query;
    while (*at) {
        while (*at && !iswalnum(*at)) ++at;
        wchar_t token[48];
        size_t used = 0;
        while (*at && iswalnum(*at)) {
            if (used + 1 < ARRAY_LEN(token)) token[used++] = *at;
            ++at;
        }
        token[used] = L'\0';
        if (used < 2) continue;
        if (url_contains_ci(candidate->title, token)) score += (int)used * 4;
        else if (url_contains_ci(candidate->channel, token)) score += (int)used * 2;
        else score -= (int)used * 2;
    }
    const wchar_t *variants[] = { L"cover", L"karaoke", L"reaction", L"nightcore", L"slowed", L"live" };
    for (size_t i = 0; i < ARRAY_LEN(variants); ++i)
        if (url_contains_ci(candidate->title, variants[i]) &&
            !url_contains_ci(query, variants[i])) score -= 25;
    return score;
}

static void accept_match_line(MatchRequest *request, const char *line, size_t length) {
    const char *first = memchr(line, '|', length);
    if (!first) return;
    const char *second = memchr(first + 1, '|', length - (size_t)(first + 1 - line));
    if (!second) return;
    MatchCandidate item;
    ZeroMemory(&item, sizeof(item));
    match_utf8_slice(line, (size_t)(first - line), item.id, ARRAY_LEN(item.id));
    if (!valid_youtube_id(item.id)) return;
    match_utf8_slice(first + 1, (size_t)(second - first - 1),
                     item.title, ARRAY_LEN(item.title));
    match_utf8_slice(second + 1, length - (size_t)(second + 1 - line),
                     item.channel, ARRAY_LEN(item.channel));
    if (!item.title[0]) return;
    item.score = score_match(request->query, &item);
    if (request->count >= (int)ARRAY_LEN(request->candidates)) return;
    int place = request->count++;
    while (place > 0 && request->candidates[place - 1].score < item.score) {
        request->candidates[place] = request->candidates[place - 1];
        --place;
    }
    request->candidates[place] = item;
}

static BOOL find_youtube_matches(MatchRequest *request) {
    wchar_t command[MAX_PATH * 4 + 1024];
    swprintf(command, ARRAY_LEN(command),
        L"\"%ls\" --flat-playlist --no-warnings --no-color --encoding utf-8 "
        L"--print \"%%(id)s|%%(title)s|%%(channel)s\" \"ytsearch5:%ls\"",
        g_ytdlp_path, request->query);

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE reader = NULL, writer = NULL;
    if (!CreatePipe(&reader, &writer, &sa, 0)) return FALSE;
    SetHandleInformation(reader, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writer;
    si.hStdError = writer;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    BOOL started = CreateProcessW(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                                  NULL, NULL, &si, &pi);
    CloseHandle(writer);
    if (!started) { CloseHandle(reader); return FALSE; }
    CloseHandle(pi.hThread);

    char buffer[2048], line[2048];
    size_t used = 0;
    DWORD got;
    while (ReadFile(reader, buffer, sizeof(buffer), &got, NULL) && got) {
        for (DWORD i = 0; i < got; ++i) {
            if (buffer[i] == '\n') {
                if (used) accept_match_line(request, line, used);
                used = 0;
            } else if (buffer[i] != '\r' && used + 1 < sizeof(line)) line[used++] = buffer[i];
        }
    }
    if (used) accept_match_line(request, line, used);
    CloseHandle(reader);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    return request->count > 0;
}

static DWORD WINAPI match_thread_proc(LPVOID param) {
    MatchRequest *request = (MatchRequest *)param;
    HRESULT com_result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    wchar_t title[256] = L"";
    if (request->source == ID_SOURCE_SPOTIFY) {
        if (!fetch_spotify_title(request->input, title, ARRAY_LEN(title)))
            wcscpy(request->error, L"Could not read that Spotify track name. Enter its title and artist to search manually.");
    } else wcsncpy(title, request->input, ARRAY_LEN(title) - 1);
    if (!request->error[0]) {
        clean_match_query(title, request->query, ARRAY_LEN(request->query));
        if (wcslen(request->query) < 3)
            wcscpy(request->error, L"The song name is too short to search.");
        else if (!find_youtube_matches(request))
            wcscpy(request->error, L"No YouTube matches were found. Try a different title and artist.");
    }
    wchar_t ids[5][16] = {{0}};
    int count = request->error[0] ? 0 : request->count;
    LONG generation = request->generation;
    for (int i = 0; i < count; ++i)
        wcscpy(ids[i], request->candidates[i].id);
    if (!PostMessageW(g_main, WM_APP_MATCH_READY, 0, (LPARAM)request)) {
        free_match_request(request);
        if (SUCCEEDED(com_result)) CoUninitialize();
        return 0;
    }
    if (count) post_match_thumbnails(ids, count, generation);
    if (SUCCEEDED(com_result)) CoUninitialize();
    return 0;
}

static void start_match_request(const wchar_t *input, int source) {
    if (InterlockedCompareExchange(&g_match_searching, 1, 0)) {
        set_download_status(L"A YouTube match search is already running. Try again when it finishes.", -1);
        return;
    }
    MatchRequest *request = (MatchRequest *)calloc(1, sizeof(MatchRequest));
    if (!request) {
        InterlockedExchange(&g_match_searching, 0);
        set_download_status(L"Could not start the YouTube match search.", -1);
        return;
    }
    request->source = source;
    request->generation = InterlockedIncrement(&g_match_generation);
    wcsncpy(request->input, input, ARRAY_LEN(request->input) - 1);
    free_match_request(g_match_results);
    g_match_results = NULL;
    g_match_scroll = 0;
    set_download_status(L"Finding the closest YouTube match...", -1);
    InvalidateRect(g_main, NULL, FALSE);
    g_match_thread = CreateThread(NULL, 0, match_thread_proc, request, 0, NULL);
    if (!g_match_thread) {
        free(request);
        InterlockedExchange(&g_match_searching, 0);
        set_download_status(L"Could not start the YouTube match search.", -1);
    }
}

static void handle_match_ready(MatchRequest *request) {
    if (g_match_thread) { CloseHandle(g_match_thread); g_match_thread = NULL; }
    InterlockedExchange(&g_match_searching, 0);
    if (!request) return;
    if (request->error[0]) {
        if (request->source == ID_SOURCE_SPOTIFY) {
            set_download_source(ID_SOURCE_SEARCH);
            SetWindowTextW(g_url_edit, L"");
            SetFocus(g_url_edit);
        }
        set_download_status(request->error, -1);
        free_match_request(request);
        update_button_enabled_state();
        return;
    }
    g_match_results = request;
    wchar_t status[192];
    swprintf(status, ARRAY_LEN(status),
             L"Found %d YouTube matches. Choose one to download.", request->count);
    set_download_status(status, -1);
    InvalidateRect(g_main, NULL, FALSE);
    update_button_enabled_state();
}

static void handle_match_thumbnail(MatchThumbnailUpdate *update) {
    if (!update) return;
    if (g_match_results && update->bitmap &&
        update->generation == g_match_results->generation &&
        update->index >= 0 && update->index < g_match_results->count) {
        MatchCandidate *candidate = &g_match_results->candidates[update->index];
        if (candidate->thumbnail) DeleteObject(candidate->thumbnail);
        candidate->thumbnail = update->bitmap;
        update->bitmap = NULL;
        if (g_page == PAGE_DOWNLOADS) InvalidateRect(g_main, NULL, FALSE);
    }
    if (update->bitmap) DeleteObject(update->bitmap);
    free(update);
}

static void clear_match_search(void) {
    if (InterlockedCompareExchange(&g_match_searching, 0, 0)) return;
    /* A thumbnail download may still be finishing after the results appeared. */
    InterlockedIncrement(&g_match_generation);
    free_match_request(g_match_results);
    g_match_results = NULL;
    g_match_scroll = 0;
    if (g_url_edit) {
        SetWindowTextW(g_url_edit, L"");
        SetFocus(g_url_edit);
    }
    if (!InterlockedCompareExchange(&g_downloading, 0, 0))
        set_download_status(L"Search cleared.", -1);
    else if (g_main && g_page == PAGE_DOWNLOADS)
        InvalidateRect(g_main, NULL, FALSE);
    update_button_enabled_state();
}

static void download_match_at(int index) {
    if (!g_match_results || index < 0 || index >= g_match_results->count) return;
    wchar_t video_url[96];
    swprintf(video_url, ARRAY_LEN(video_url), L"https://www.youtube.com/watch?v=%ls",
             g_match_results->candidates[index].id);
    set_download_source(ID_SOURCE_YOUTUBE);
    SetWindowTextW(g_url_edit, video_url);
    start_download();
}

static void shutdown_match_search(void) {
    InterlockedIncrement(&g_match_generation);
    free_match_request(g_match_results);
    g_match_results = NULL;
    if (g_match_thread) {
        CloseHandle(g_match_thread);
        g_match_thread = NULL;
    }
}
