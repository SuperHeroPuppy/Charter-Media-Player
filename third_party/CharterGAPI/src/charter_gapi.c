#include "charter_gapi.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

static BOOL cgapi_place(const CGapiMove *move) {
    return SetWindowPos(move->window, NULL, move->x, move->y,
                        move->width, move->height,
                        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
}

void cgapi_layout_begin(CGapiLayout *layout, HWND parent, size_t reserve) {
    if (!layout) return;
    memset(layout, 0, sizeof(*layout));
    layout->parent = parent;
    layout->moves = layout->inline_moves;
    layout->capacity = sizeof(layout->inline_moves) / sizeof(layout->inline_moves[0]);
    if (reserve > layout->capacity && reserve <= SIZE_MAX / sizeof(CGapiMove)) {
        layout->moves = (CGapiMove *)malloc(reserve * sizeof(CGapiMove));
        if (layout->moves) layout->capacity = reserve;
        else layout->moves = layout->inline_moves;
    }
}

BOOL cgapi_layout_move(CGapiLayout *layout, HWND child,
                       int x, int y, int width, int height) {
    if (!layout || !child || !IsWindow(child) || width < 0 || height < 0)
        return FALSE;
    RECT current;
    if (GetWindowRect(child, &current)) {
        MapWindowPoints(HWND_DESKTOP, GetParent(child), (POINT *)&current, 2);
        if (current.left == x && current.top == y &&
            current.right - current.left == width &&
            current.bottom - current.top == height)
            return TRUE;
    }
    if (layout->count == layout->capacity) {
        size_t next = layout->capacity ? layout->capacity * 2 : 8;
        if (next < layout->capacity || next > SIZE_MAX / sizeof(CGapiMove))
            return cgapi_place(&(CGapiMove){ child, x, y, width, height });
        CGapiMove *grown = layout->moves == layout->inline_moves
            ? (CGapiMove *)malloc(next * sizeof(CGapiMove))
            : (CGapiMove *)realloc(layout->moves, next * sizeof(CGapiMove));
        if (!grown)
            return cgapi_place(&(CGapiMove){ child, x, y, width, height });
        if (layout->moves == layout->inline_moves)
            memcpy(grown, layout->inline_moves,
                   layout->count * sizeof(CGapiMove));
        layout->moves = grown;
        layout->capacity = next;
    }
    layout->moves[layout->count++] = (CGapiMove){ child, x, y, width, height };
    return TRUE;
}

BOOL cgapi_layout_end(CGapiLayout *layout) {
    if (!layout) return FALSE;
    BOOL success = TRUE;
    if (layout->count) {
        HDWP batch = BeginDeferWindowPos((int)layout->count);
        if (batch) {
            for (size_t i = 0; i < layout->count; ++i) {
                const CGapiMove *move = &layout->moves[i];
                batch = DeferWindowPos(batch, move->window, NULL,
                                       move->x, move->y, move->width, move->height,
                                       SWP_NOZORDER | SWP_NOACTIVATE |
                                       SWP_NOCOPYBITS);
                if (!batch) break;
            }
        }
        if (!batch || !EndDeferWindowPos(batch)) {
            success = TRUE;
            for (size_t i = 0; i < layout->count; ++i)
                if (!cgapi_place(&layout->moves[i])) success = FALSE;
        }
        cgapi_redraw(layout->parent, TRUE, FALSE);
    }
    if (layout->moves != layout->inline_moves) free(layout->moves);
    memset(layout, 0, sizeof(*layout));
    return success;
}

BOOL cgapi_set_visible(HWND window, BOOL visible) {
    if (!window) return FALSE;
    BOOL was_visible = (GetWindowLongPtrW(window, GWL_STYLE) & WS_VISIBLE) != 0;
    visible = visible != FALSE;
    if (was_visible == visible) return FALSE;
    ShowWindow(window, visible ? SW_SHOW : SW_HIDE);
    return TRUE;
}

BOOL cgapi_set_text(HWND window, const wchar_t *text) {
    if (!window) return FALSE;
    if (!text) text = L"";
    int length = GetWindowTextLengthW(window);
    wchar_t stack_text[128];
    wchar_t *current = length < (int)(sizeof(stack_text) / sizeof(stack_text[0]))
        ? stack_text : (wchar_t *)malloc(((size_t)length + 1) * sizeof(wchar_t));
    if (current) {
        GetWindowTextW(window, current, length + 1);
        BOOL same = wcscmp(current, text) == 0;
        if (current != stack_text) free(current);
        if (same) return FALSE;
    }
    if (!SetWindowTextW(window, text)) return FALSE;
    cgapi_redraw(window, FALSE, TRUE);
    return TRUE;
}

void cgapi_redraw(HWND window, BOOL include_children, BOOL immediately) {
    if (!window) return;
    UINT flags = RDW_INVALIDATE |
                 (include_children ? RDW_ALLCHILDREN : RDW_NOCHILDREN);
    if (immediately) flags |= RDW_UPDATENOW;
    RedrawWindow(window, NULL, NULL, flags);
}

BOOL cgapi_paint_begin(CGapiPaint *frame, HWND window) {
    if (!frame || !window) return FALSE;
    memset(frame, 0, sizeof(*frame));
    frame->window = window;
    frame->target = BeginPaint(window, &frame->paint);
    if (!frame->target) return FALSE;
    frame->dirty = frame->paint.rcPaint;
    int width = frame->dirty.right - frame->dirty.left;
    int height = frame->dirty.bottom - frame->dirty.top;
    if (width > 0 && height > 0) {
        frame->buffer = CreateCompatibleDC(frame->target);
        if (frame->buffer) frame->bitmap =
            CreateCompatibleBitmap(frame->target, width, height);
        if (frame->bitmap) {
            frame->previous_bitmap = SelectObject(frame->buffer, frame->bitmap);
            SetViewportOrgEx(frame->buffer, -frame->dirty.left,
                             -frame->dirty.top, NULL);
        } else if (frame->buffer) {
            DeleteDC(frame->buffer);
            frame->buffer = NULL;
        }
    }
    return TRUE;
}

HDC cgapi_paint_dc(const CGapiPaint *frame) {
    return frame ? (frame->bitmap ? frame->buffer : frame->target) : NULL;
}

void cgapi_paint_end(CGapiPaint *frame) {
    if (!frame || !frame->target) return;
    if (frame->bitmap) {
        int width = frame->dirty.right - frame->dirty.left;
        int height = frame->dirty.bottom - frame->dirty.top;
        SetViewportOrgEx(frame->buffer, 0, 0, NULL);
        BitBlt(frame->target, frame->dirty.left, frame->dirty.top,
               width, height, frame->buffer, 0, 0, SRCCOPY);
        SelectObject(frame->buffer, frame->previous_bitmap);
        DeleteObject(frame->bitmap);
    }
    if (frame->buffer) DeleteDC(frame->buffer);
    EndPaint(frame->window, &frame->paint);
    memset(frame, 0, sizeof(*frame));
}

void cgapi_fill_round_rect(HDC dc, RECT bounds, int radius, COLORREF color) {
    if (!dc) return;
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    if (!brush || !pen) {
        if (brush) DeleteObject(brush);
        if (pen) DeleteObject(pen);
        return;
    }
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    RoundRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom,
              radius, radius);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void cgapi_stroke_round_rect(HDC dc, RECT bounds, int radius, COLORREF color) {
    if (!dc) return;
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    if (!pen) return;
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom,
              radius, radius);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

void cgapi_draw_line(HDC dc, int x1, int y1, int x2, int y2,
                     COLORREF color, int width) {
    if (!dc) return;
    HPEN pen = CreatePen(PS_SOLID, width > 0 ? width : 1, color);
    if (!pen) return;
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, NULL);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
    DeleteObject(pen);
}

int cgapi_draw_text(HDC dc, const wchar_t *text, RECT bounds, HFONT font,
                    COLORREF color, UINT format) {
    if (!dc) return 0;
    int saved = SaveDC(dc);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    if (font) SelectObject(dc, font);
    int height = DrawTextW(dc, text ? text : L"", -1, &bounds,
                           format | DT_NOPREFIX);
    if (saved) RestoreDC(dc, saved);
    return height;
}
