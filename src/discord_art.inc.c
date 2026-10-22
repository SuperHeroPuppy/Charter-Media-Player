/* Discord can fetch public artwork URLs, but not a local file or HBITMAP. */
static BOOL discord_youtube_art_url(const wchar_t *media_path,
                                    char *url, size_t capacity) {
    if (!url || capacity == 0) return FALSE;
    url[0] = '\0';
    if (!media_path || !*media_path) return FALSE;

    const wchar_t *file = wcsrchr(media_path, L'\\');
    file = file ? file + 1 : media_path;
    const wchar_t *extension = wcsrchr(file, L'.');
    if (!extension || extension - file < 13 || extension[-1] != L']')
        return FALSE;
    const wchar_t *id = extension - 13;
    if (*id++ != L'[') return FALSE;
    char video_id[12];
    for (int i = 0; i < 11; ++i) {
        wchar_t ch = id[i];
        if (!((ch >= L'a' && ch <= L'z') ||
              (ch >= L'A' && ch <= L'Z') ||
              (ch >= L'0' && ch <= L'9') || ch == L'-' || ch == L'_'))
            return FALSE;
        video_id[i] = (char)ch;
    }
    video_id[11] = '\0';

    /* A matching yt-dlp sidecar distinguishes a download from a local file
       whose name happens to end in an eleven-character bracketed string. */
    wchar_t sidecar[MAX_PATH * 4];
    size_t stem = (size_t)(extension - media_path);
    if (stem + ARRAY_LEN(L".info.json") > ARRAY_LEN(sidecar)) return FALSE;
    wmemcpy(sidecar, media_path, stem);
    wcscpy(sidecar + stem, L".info.json");
    DWORD attributes = GetFileAttributesW(sidecar);
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY)) return FALSE;

    int length = snprintf(url, capacity,
                          "https://i.ytimg.com/vi/%s/hqdefault.jpg", video_id);
    if (length <= 0 || (size_t)length >= capacity) {
        url[0] = '\0';
        return FALSE;
    }
    return TRUE;
}
