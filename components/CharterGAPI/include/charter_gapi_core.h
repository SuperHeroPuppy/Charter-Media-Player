#ifndef CHARTER_GAPI_CORE_H
#define CHARTER_GAPI_CORE_H

#include <stdint.h>

#define CGAPI_CORE_VERSION_MAJOR 0
#define CGAPI_CORE_VERSION_MINOR 2
#define CGAPI_CORE_VERSION_PATCH 0

#ifdef __cplusplus
extern "C" {
#endif

/* Platform independent colors. UI adapters convert these to native colors. */
typedef struct CGapiColor {
    uint8_t red, green, blue, alpha;
} CGapiColor;

typedef struct CGapiPalette {
    CGapiColor background;
    CGapiColor surface;
    CGapiColor accent;
    CGapiColor text;
    CGapiColor muted_text;
    CGapiColor border;
} CGapiPalette;

/* Logical pixels: dp on Android, points on iOS, CSS pixels on GTK. */
typedef struct CGapiMetrics {
    int spacing;
    int sidebar_width;
    int media_height;
} CGapiMetrics;

CGapiPalette cgapi_palette_default_dark(void);
CGapiMetrics cgapi_metrics_for_width(int width);
CGapiColor cgapi_color_blend(CGapiColor from, CGapiColor to, int to_percent);
uint32_t cgapi_color_argb32(CGapiColor color);

#ifdef __cplusplus
}
#endif

#endif
