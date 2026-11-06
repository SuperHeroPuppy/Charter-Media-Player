#include "charter_gapi_core.h"

CGapiPalette cgapi_palette_default_dark(void) {
    return (CGapiPalette){
        {28, 28, 28, 255},   /* background */
        {43, 43, 43, 255},   /* surface */
        {245, 166, 12, 255}, /* accent */
        {245, 245, 245, 255},/* text */
        {171, 178, 188, 255},/* muted text */
        {66, 66, 66, 255}    /* border */
    };
}

CGapiMetrics cgapi_metrics_for_width(int width) {
    CGapiMetrics metrics = {12, 260, 220};
    if (width < 600) {
        metrics.sidebar_width = 0;
        metrics.media_height = 210;
    } else if (width >= 1000) {
        metrics.spacing = 16;
        metrics.sidebar_width = 300;
        metrics.media_height = 280;
    }
    return metrics;
}

CGapiColor cgapi_color_blend(CGapiColor from, CGapiColor to, int to_percent) {
    if (to_percent < 0) to_percent = 0;
    if (to_percent > 100) to_percent = 100;
    int from_percent = 100 - to_percent;
    return (CGapiColor){
        (uint8_t)((from.red * from_percent + to.red * to_percent) / 100),
        (uint8_t)((from.green * from_percent + to.green * to_percent) / 100),
        (uint8_t)((from.blue * from_percent + to.blue * to_percent) / 100),
        (uint8_t)((from.alpha * from_percent + to.alpha * to_percent) / 100)
    };
}

uint32_t cgapi_color_argb32(CGapiColor color) {
    return ((uint32_t)color.alpha << 24) | ((uint32_t)color.red << 16) |
           ((uint32_t)color.green << 8) | color.blue;
}
