#ifndef CHARTER_GAPI_H
#define CHARTER_GAPI_H

#include "charter_gapi_core.h"
#include <windows.h>
#include <stddef.h>

#define CGAPI_VERSION_MAJOR 0
#define CGAPI_VERSION_MINOR 2
#define CGAPI_VERSION_PATCH 0

#ifdef __cplusplus
extern "C" {
#endif

/* Small, reusable Win32 presentation helpers. All coordinates are client pixels. */
typedef struct CGapiMove {
    HWND window;
    int x, y, width, height;
} CGapiMove;

typedef struct CGapiLayout {
    HWND parent;
    CGapiMove *moves;
    size_t count, capacity;
    CGapiMove inline_moves[64];
} CGapiLayout;

void cgapi_layout_begin(CGapiLayout *layout, HWND parent, size_t reserve);
BOOL cgapi_layout_move(CGapiLayout *layout, HWND child,
                       int x, int y, int width, int height);
BOOL cgapi_layout_end(CGapiLayout *layout);

BOOL cgapi_set_visible(HWND window, BOOL visible);
BOOL cgapi_set_text(HWND window, const wchar_t *text);
void cgapi_redraw(HWND window, BOOL include_children, BOOL immediately);

typedef struct CGapiPaint {
    HWND window;
    PAINTSTRUCT paint;
    HDC target, buffer;
    HBITMAP bitmap;
    HGDIOBJ previous_bitmap;
    RECT dirty;
} CGapiPaint;

BOOL cgapi_paint_begin(CGapiPaint *frame, HWND window);
HDC cgapi_paint_dc(const CGapiPaint *frame);
void cgapi_paint_end(CGapiPaint *frame);

void cgapi_fill_round_rect(HDC dc, RECT bounds, int radius, COLORREF color);
void cgapi_stroke_round_rect(HDC dc, RECT bounds, int radius, COLORREF color);
void cgapi_draw_line(HDC dc, int x1, int y1, int x2, int y2,
                     COLORREF color, int width);
int cgapi_draw_text(HDC dc, const wchar_t *text, RECT bounds, HFONT font,
                    COLORREF color, UINT format);

#ifdef __cplusplus
}
#endif

#endif
