#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/rtc.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr.h"

#define EXPR_BUF_SIZE 96
#define SCREEN_W 128
#define SCREEN_H 64

typedef enum {
    LOOP_WRAP = 0,
    LOOP_BOUNCE = 1,
    LOOP_FREE = 2
} LoopMode;

typedef struct {
    float xmin, xmax;
    float ymin, ymax;
    float tmin, tmax;
    float dt;
    LoopMode mode;
} GraphConfig;

static const char *loop_mode_short(LoopMode m)
{
    switch (m) {
        case LOOP_WRAP:   return "LOOP";
        case LOOP_BOUNCE: return "BNC";
        case LOOP_FREE:   return "FREE";
    }
    return "LOOP";
}

static const char *loop_mode_name(LoopMode m)
{
    switch (m) {
        case LOOP_WRAP:   return "Bucle (Tmin..Tmax)";
        case LOOP_BOUNCE: return "Rebote (Ida/Vuelta)";
        case LOOP_FREE:   return "Libre (Continuo)";
    }
    return "Bucle";
}

static void config_reset(GraphConfig *cfg)
{
    cfg->xmin = -6.2832f;
    cfg->xmax =  6.2832f;
    cfg->ymin = -2.5f;
    cfg->ymax =  2.5f;
    cfg->tmin =  0.0f;
    cfg->tmax =  6.2832f;
    cfg->dt   =  0.15f;
    cfg->mode = LOOP_WRAP;
}

/* ---------- Helper: format float without printf %f/%g (fxlibc safe) ---------- */
static void fmt_float(char *dst, int dst_size, float val, int decimals)
{
    if (!isfinite(val)) {
        snprintf(dst, (size_t)dst_size, "NaN");
        return;
    }
    bool neg = (val < 0.0f);
    if (neg) val = -val;

    int scale = 1;
    for (int i = 0; i < decimals; i++) scale *= 10;

    int32_t scaled = (int32_t)(val * (float)scale + 0.5f);
    int32_t whole = scaled / scale;
    int32_t frac  = scaled % scale;

    if (decimals <= 0) {
        snprintf(dst, (size_t)dst_size, "%s%ld", neg ? "-" : "", (long)whole);
    } else if (decimals == 1) {
        snprintf(dst, (size_t)dst_size, "%s%ld.%01ld", neg ? "-" : "", (long)whole, (long)frac);
    } else {
        snprintf(dst, (size_t)dst_size, "%s%ld.%02ld", neg ? "-" : "", (long)whole, (long)frac);
    }
}

/* ---------- Helper: draw bottom F-key bar ---------- */
static void draw_fkeys(const char *k1, const char *k2, const char *k3,
                       const char *k4, const char *k5, const char *k6)
{
    const char *labels[6] = { k1, k2, k3, k4, k5, k6 };
    const int y = 56;
    drect(0, y, 127, 63, C_WHITE);
    for (int i = 0; i < 6; i++) {
        int x0 = i * 21 + 1;
        int x1 = x0 + 19;
        if (labels[i] && labels[i][0] != '\0') {
            drect(x0, y, x1, 63, C_BLACK);
            dprint(x0 + 1, y + 1, C_WHITE, "%s", labels[i]);
        }
    }
}

/* ---------- Helper: insert string at cursor ---------- */
static void buf_insert(char *buf, int *cursor, int max_len, const char *ins)
{
    int cur_len = (int)strlen(buf);
    int ins_len = (int)strlen(ins);
    if (cur_len + ins_len >= max_len) return;

    memmove(buf + *cursor + ins_len, buf + *cursor, (size_t)(cur_len - *cursor + 1));
    memcpy(buf + *cursor, ins, (size_t)ins_len);
    *cursor += ins_len;
}

static void buf_backspace(char *buf, int *cursor)
{
    if (*cursor <= 0) return;
    int cur_len = (int)strlen(buf);
    memmove(buf + *cursor - 1, buf + *cursor, (size_t)(cur_len - *cursor + 1));
    (*cursor)--;
}

/* ---------- Popup menu for functions ---------- */
static const char *popup_functions(void)
{
    static const char *funcs[] = {
        "sin(", "cos(", "tan(", "sqrt(", "abs(",
        "exp(", "ln(",  "log(", "pi",    "e",
        "asin(", "acos(", "atan(", "floor(", "ceil("
    };
    const int count = (int)(sizeof(funcs) / sizeof(funcs[0]));
    int sel = 0;
    int top = 0;

    while (1) {
        dclear(C_WHITE);
        drect(0, 0, 127, 8, C_BLACK);
        dprint(2, 1, C_WHITE, "Insertar funcion");

        int visible = 5;
        if (sel < top) top = sel;
        if (sel >= top + visible) top = sel - visible + 1;

        for (int i = 0; i < visible && (top + i) < count; i++) {
            int idx = top + i;
            int y = 11 + i * 9;
            if (idx == sel) {
                drect(2, y - 1, 125, y + 7, C_BLACK);
                dprint(5, y, C_WHITE, "%s", funcs[idx]);
            } else {
                dprint(5, y, C_BLACK, "%s", funcs[idx]);
            }
        }
        dprint(2, 56, C_BLACK, "[EXE] OK   [EXIT] Volver");
        dupdate();

        key_event_t ev = getkey();
        if (ev.key == KEY_UP) {
            if (sel > 0) sel--;
        } else if (ev.key == KEY_DOWN) {
            if (sel < count - 1) sel++;
        } else if (ev.key == KEY_EXE) {
            return funcs[sel];
        } else if (ev.key == KEY_EXIT) {
            return NULL;
        }
    }
}

/* ---------- Popup menu for presets ---------- */
typedef struct {
    const char *name;
    const char *expr;
} Preset;

static const char *popup_presets(void)
{
    static const Preset presets[] = {
        { "Onda viajera",   "sin(x-t)" },
        { "Estacionaria",   "sin(x)*cos(t)" },
        { "Interferencia",  "sin(x-t)+sin(2x+t)" },
        { "Pulso Gauss",    "2*exp(-0.4*(x-t+3)^2)" },
        { "Sinc temporal",  "2*sin(x*t)/(x+0.001)" },
        { "Paquete onda",   "2*cos(2x-t)*exp(-0.1*x^2)" }
    };
    const int count = (int)(sizeof(presets) / sizeof(presets[0]));
    int sel = 0;
    int top = 0;

    while (1) {
        dclear(C_WHITE);
        drect(0, 0, 127, 8, C_BLACK);
        dprint(2, 1, C_WHITE, "Ejemplos f(x,t)");

        int visible = 4;
        if (sel < top) top = sel;
        if (sel >= top + visible) top = sel - visible + 1;

        for (int i = 0; i < visible && (top + i) < count; i++) {
            int idx = top + i;
            int y = 10 + i * 11;
            if (idx == sel) {
                drect(1, y, 126, y + 9, C_BLACK);
                dprint(3, y + 1, C_WHITE, "%s", presets[idx].name);
            } else {
                dprint(3, y + 1, C_BLACK, "%s", presets[idx].name);
            }
        }
        dprint(2, 56, C_BLACK, "%.20s", presets[sel].expr);
        dupdate();

        key_event_t ev = getkey();
        if (ev.key == KEY_UP) {
            if (sel > 0) sel--;
        } else if (ev.key == KEY_DOWN) {
            if (sel < count - 1) sel++;
        } else if (ev.key == KEY_EXE) {
            return presets[sel].expr;
        } else if (ev.key == KEY_EXIT) {
            return NULL;
        }
    }
}

/* ---------- Edit float value helper ---------- */
static bool edit_float_modal(const char *label, float *value)
{
    char buf[24];
    fmt_float(buf, (int)sizeof(buf), *value, 2);
    int cursor = (int)strlen(buf);
    bool first_key = true;

    while (1) {
        dclear(C_WHITE);
        drect(0, 0, 127, 8, C_BLACK);
        dprint(2, 1, C_WHITE, "Editar %s", label);

        drect_border(4, 20, 123, 35, C_WHITE, 1, C_BLACK);
        dprint(8, 24, C_BLACK, "%s_", buf);

        dprint(4, 42, C_BLACK, "[EXE] Guardar  [DEL] Borrar");
        dprint(4, 52, C_BLACK, "[EXIT] Cancelar");
        dupdate();

        key_event_t ev = getkey();
        if (ev.key == KEY_EXIT) return false;
        if (ev.key == KEY_EXE) {
            char *endp = NULL;
            float v = strtof(buf, &endp);
            if (endp != buf) {
                *value = v;
                return true;
            }
            return false;
        }
        if (ev.key == KEY_ACON) {
            buf[0] = '\0';
            cursor = 0;
            first_key = false;
            continue;
        }
        if (ev.key == KEY_DEL) {
            buf_backspace(buf, &cursor);
            first_key = false;
            continue;
        }

        const char *ins = NULL;
        switch (ev.key) {
            case KEY_0: ins = "0"; break;
            case KEY_1: ins = "1"; break;
            case KEY_2: ins = "2"; break;
            case KEY_3: ins = "3"; break;
            case KEY_4: ins = "4"; break;
            case KEY_5: ins = "5"; break;
            case KEY_6: ins = "6"; break;
            case KEY_7: ins = "7"; break;
            case KEY_8: ins = "8"; break;
            case KEY_9: ins = "9"; break;
            case KEY_DOT: ins = "."; break;
            case KEY_SUB:
            case KEY_NEG: ins = "-"; break;
            default: break;
        }
        if (ins) {
            if (first_key) {
                buf[0] = '\0';
                cursor = 0;
                first_key = false;
            }
            buf_insert(buf, &cursor, (int)sizeof(buf), ins);
        }
    }
}

/* ---------- Config Screen (V-Window + Time) ---------- */
static void run_config_screen(GraphConfig *cfg)
{
    int sel = 0;
    int top = 0;
    const int count = 8;

    while (1) {
        dclear(C_WHITE);
        drect(0, 0, 127, 8, C_BLACK);
        dprint(2, 1, C_WHITE, "Config Ventana y Tiempo");

        int visible = 5;
        if (sel < top) top = sel;
        if (sel >= top + visible) top = sel - visible + 1;

        for (int i = 0; i < visible && (top + i) < count; i++) {
            int idx = top + i;
            int y = 10 + i * 9;
            char line[32];
            char fbuf[16];

            switch (idx) {
                case 0: fmt_float(fbuf, sizeof(fbuf), cfg->xmin, 2); snprintf(line, sizeof(line), "Xmin: %s", fbuf); break;
                case 1: fmt_float(fbuf, sizeof(fbuf), cfg->xmax, 2); snprintf(line, sizeof(line), "Xmax: %s", fbuf); break;
                case 2: fmt_float(fbuf, sizeof(fbuf), cfg->ymin, 2); snprintf(line, sizeof(line), "Ymin: %s", fbuf); break;
                case 3: fmt_float(fbuf, sizeof(fbuf), cfg->ymax, 2); snprintf(line, sizeof(line), "Ymax: %s", fbuf); break;
                case 4: fmt_float(fbuf, sizeof(fbuf), cfg->tmin, 2); snprintf(line, sizeof(line), "Tmin: %s", fbuf); break;
                case 5: fmt_float(fbuf, sizeof(fbuf), cfg->tmax, 2); snprintf(line, sizeof(line), "Tmax: %s", fbuf); break;
                case 6: fmt_float(fbuf, sizeof(fbuf), cfg->dt,   2); snprintf(line, sizeof(line), "dt  : %s", fbuf); break;
                case 7: snprintf(line, sizeof(line), "Modo: %s", loop_mode_short(cfg->mode)); break;
            }

            if (idx == sel) {
                drect(1, y - 1, 126, y + 7, C_BLACK);
                dprint(3, y, C_WHITE, "%s", line);
            } else {
                dprint(3, y, C_BLACK, "%s", line);
            }
        }

        draw_fkeys("DEF", "", "", "", "", "OK");
        dupdate();

        key_event_t ev = getkey();
        if (ev.key == KEY_UP) {
            if (sel > 0) sel--;
        } else if (ev.key == KEY_DOWN) {
            if (sel < count - 1) sel++;
        } else if (ev.key == KEY_F1) {
            config_reset(cfg);
        } else if (ev.key == KEY_EXIT || ev.key == KEY_F6) {
            /* Ensure valid bounds */
            if (cfg->xmax <= cfg->xmin) cfg->xmax = cfg->xmin + 1.0f;
            if (cfg->ymax <= cfg->ymin) cfg->ymax = cfg->ymin + 1.0f;
            if (cfg->tmax <= cfg->tmin) cfg->tmax = cfg->tmin + 1.0f;
            if (cfg->dt <= 0.001f) cfg->dt = 0.05f;
            return;
        } else if (ev.key == KEY_LEFT || ev.key == KEY_RIGHT) {
            if (sel == 7) {
                int m = (int)cfg->mode + (ev.key == KEY_RIGHT ? 1 : 2);
                cfg->mode = (LoopMode)(m % 3);
            }
        } else if (ev.key == KEY_EXE) {
            switch (sel) {
                case 0: edit_float_modal("Xmin", &cfg->xmin); break;
                case 1: edit_float_modal("Xmax", &cfg->xmax); break;
                case 2: edit_float_modal("Ymin", &cfg->ymin); break;
                case 3: edit_float_modal("Ymax", &cfg->ymax); break;
                case 4: edit_float_modal("Tmin", &cfg->tmin); break;
                case 5: edit_float_modal("Tmax", &cfg->tmax); break;
                case 6: edit_float_modal("dt",   &cfg->dt);   break;
                case 7: cfg->mode = (LoopMode)(((int)cfg->mode + 1) % 3); break;
            }
        }
    }
}

/* ---------- Animated Graph Screen ---------- */
static void run_graph_screen(const ExprCompiled *prog, GraphConfig *cfg)
{
    float t = cfg->tmin;
    float dir = 1.0f;
    bool paused = false;
    bool show_hud = true;

    int held_key = 0;
    uint32_t hold_start_tick = 0;

    clearevents();
    uint32_t last_tick = rtc_ticks();

    while (1) {
        /* 1. Draw graph */
        dclear(C_WHITE);

        int plot_top = show_hud ? 9 : 0;
        int plot_bot = show_hud ? 55 : 63;
        int plot_h = plot_bot - plot_top;

        float x_span = cfg->xmax - cfg->xmin;
        float y_span = cfg->ymax - cfg->ymin;
        float dx = x_span / (float)(SCREEN_W - 1);

        /* Draw axes */
        if (cfg->xmin <= 0.0f && cfg->xmax >= 0.0f) {
            int ax = (int)((-cfg->xmin / x_span) * (SCREEN_W - 1) + 0.5f);
            if (ax >= 0 && ax < SCREEN_W) {
                dline(ax, plot_top, ax, plot_bot, C_BLACK);
            }
        }
        if (cfg->ymin <= 0.0f && cfg->ymax >= 0.0f) {
            int ay = plot_bot - (int)((-cfg->ymin / y_span) * plot_h + 0.5f);
            if (ay >= plot_top && ay <= plot_bot) {
                dline(0, ay, SCREEN_W - 1, ay, C_BLACK);
            }
        }

        /* Plot curve y = f(x, t) */
        bool has_prev = false;
        int prev_px = 0, prev_py = 0;

        for (int px = 0; px < SCREEN_W; px++) {
            float x = cfg->xmin + (float)px * dx;
            float y = expr_eval(prog, x, t);

            if (!isfinite(y)) {
                has_prev = false;
                continue;
            }

            float norm_y = (y - cfg->ymin) / y_span;
            int py = plot_bot - (int)(norm_y * (float)plot_h + 0.5f);

            /* Avoid drawing vertical asymptote lines across the entire screen */
            if (has_prev && abs(py - prev_py) <= plot_h) {
                /* Clip only if at least one endpoint is near screen */
                if ((py >= plot_top - 16 && py <= plot_bot + 16) ||
                    (prev_py >= plot_top - 16 && prev_py <= plot_bot + 16)) {
                    dline(prev_px, prev_py, px, py, C_BLACK);
                }
            } else if (py >= plot_top && py <= plot_bot) {
                dpixel(px, py, C_BLACK);
            }

            prev_px = px;
            prev_py = py;
            has_prev = true;
        }

        /* Draw HUD */
        if (show_hud) {
            char t_str[16];
            fmt_float(t_str, sizeof(t_str), t, 2);
            drect(0, 0, 127, 8, C_BLACK);
            dprint(2, 1, C_WHITE, "t=%s %s %s",
                   t_str,
                   paused ? "PAUSA" : loop_mode_short(cfg->mode),
                   prog->uses_t ? "" : "(est)");
            draw_fkeys(paused ? "PLAY" : "PAUS",
                       paused ? "<t" : "v-",
                       paused ? "t>" : "v+",
                       loop_mode_short(cfg->mode),
                       "t=0",
                       "CONF");
        }

        dupdate();

        /* 2. Handle non-blocking keyboard events */
        key_event_t ev;
        while ((ev = pollevent()).type != KEYEV_NONE) {
            if (ev.type == KEYEV_UP) {
                if (ev.key == held_key) held_key = 0;
                continue;
            }
            if (ev.type != KEYEV_DOWN) continue;

            if (ev.key == KEY_EXIT) {
                return;
            } else if (ev.key == KEY_EXE || ev.key == KEY_F1) {
                paused = !paused;
            } else if (ev.key == KEY_F2) {
                if (paused) {
                    t -= cfg->dt;
                    held_key = KEY_F2;
                    hold_start_tick = rtc_ticks();
                } else {
                    cfg->dt *= 0.75f;
                    if (cfg->dt < 0.01f) cfg->dt = 0.01f;
                }
            } else if (ev.key == KEY_F3) {
                if (paused) {
                    t += cfg->dt;
                    held_key = KEY_F3;
                    hold_start_tick = rtc_ticks();
                } else {
                    cfg->dt *= 1.33f;
                    if (cfg->dt > 5.0f) cfg->dt = 5.0f;
                }
            } else if (ev.key == KEY_F4) {
                cfg->mode = (LoopMode)(((int)cfg->mode + 1) % 3);
            } else if (ev.key == KEY_F5) {
                t = cfg->tmin;
                dir = 1.0f;
            } else if (ev.key == KEY_F6) {
                held_key = 0;
                run_config_screen(cfg);
                clearevents();
                last_tick = rtc_ticks();
            } else if (ev.key == KEY_OPTN || ev.key == KEY_VARS) {
                show_hud = !show_hud;
            } else if (ev.key == KEY_LEFT) {
                float shift = x_span * 0.1f;
                cfg->xmin -= shift; cfg->xmax -= shift;
            } else if (ev.key == KEY_RIGHT) {
                float shift = x_span * 0.1f;
                cfg->xmin += shift; cfg->xmax += shift;
            } else if (ev.key == KEY_UP) {
                float shift = y_span * 0.1f;
                cfg->ymin += shift; cfg->ymax += shift;
            } else if (ev.key == KEY_DOWN) {
                float shift = y_span * 0.1f;
                cfg->ymin -= shift; cfg->ymax -= shift;
            } else if (ev.key == KEY_ADD) {
                /* Zoom in */
                float cx = 0.5f * (cfg->xmin + cfg->xmax);
                float cy = 0.5f * (cfg->ymin + cfg->ymax);
                float hx = 0.4f * x_span;
                float hy = 0.4f * y_span;
                cfg->xmin = cx - hx; cfg->xmax = cx + hx;
                cfg->ymin = cy - hy; cfg->ymax = cy + hy;
                held_key = KEY_ADD;
                hold_start_tick = rtc_ticks();
            } else if (ev.key == KEY_SUB) {
                /* Zoom out */
                float cx = 0.5f * (cfg->xmin + cfg->xmax);
                float cy = 0.5f * (cfg->ymin + cfg->ymax);
                float hx = 0.625f * x_span;
                float hy = 0.625f * y_span;
                cfg->xmin = cx - hx; cfg->xmax = cx + hx;
                cfg->ymin = cy - hy; cfg->ymax = cy + hy;
                held_key = KEY_SUB;
                hold_start_tick = rtc_ticks();
            }
        }

        /* 2b. Continuous repeat after 1 second (128 RTC ticks) of holding F2/F3 (paused) or +/- */
        if (held_key != 0) {
            if (!keydown(held_key)) {
                held_key = 0;
            } else if ((rtc_ticks() - hold_start_tick) >= 128) {
                if (held_key == KEY_F2 && paused) {
                    t -= cfg->dt;
                } else if (held_key == KEY_F3 && paused) {
                    t += cfg->dt;
                } else if (held_key == KEY_ADD) {
                    float cur_xspan = cfg->xmax - cfg->xmin;
                    float cur_yspan = cfg->ymax - cfg->ymin;
                    float cx = 0.5f * (cfg->xmin + cfg->xmax);
                    float cy = 0.5f * (cfg->ymin + cfg->ymax);
                    float hx = 0.46f * cur_xspan;
                    float hy = 0.46f * cur_yspan;
                    cfg->xmin = cx - hx; cfg->xmax = cx + hx;
                    cfg->ymin = cy - hy; cfg->ymax = cy + hy;
                } else if (held_key == KEY_SUB) {
                    float cur_xspan = cfg->xmax - cfg->xmin;
                    float cur_yspan = cfg->ymax - cfg->ymin;
                    float cx = 0.5f * (cfg->xmin + cfg->xmax);
                    float cy = 0.5f * (cfg->ymin + cfg->ymax);
                    float hx = 0.54f * cur_xspan;
                    float hy = 0.54f * cur_yspan;
                    cfg->xmin = cx - hx; cfg->xmax = cx + hx;
                    cfg->ymin = cy - hy; cfg->ymax = cy + hy;
                }
            }
        }

        /* 3. Advance time t (frame-paced to ~20 FPS: 6 ticks at 128 Hz) */
        uint32_t now = rtc_ticks();
        if (!paused) {
            if (cfg->mode == LOOP_WRAP) {
                t += cfg->dt;
                if (t > cfg->tmax) {
                    float span = cfg->tmax - cfg->tmin;
                    t = (span > 0.0f) ? (cfg->tmin + fmodf(t - cfg->tmin, span)) : cfg->tmin;
                }
            } else if (cfg->mode == LOOP_BOUNCE) {
                t += dir * cfg->dt;
                if (t >= cfg->tmax) {
                    t = cfg->tmax;
                    dir = -1.0f;
                } else if (t <= cfg->tmin) {
                    t = cfg->tmin;
                    dir = 1.0f;
                }
            } else {
                t += cfg->dt;
            }
        }

        /* Cap max frame rate so fast expressions don't fly by */
        while ((rtc_ticks() - now) < 4) {
            /* Brief wait (~30ms max) */
        }
        (void)last_tick;
    }
}

/* ---------- Main Editor Screen ---------- */
int main(void)
{
    GraphConfig cfg;
    config_reset(&cfg);

    char expr_buf[EXPR_BUF_SIZE] = "sin(x-t)";
    int cursor = (int)strlen(expr_buf);
    char err_msg[EXPR_ERR_LEN] = "";
    ExprCompiled compiled;

    while (1) {
        dclear(C_WHITE);
        drect(0, 0, 127, 8, C_BLACK);
        dprint(2, 1, C_WHITE, "TimeGraph  y=f(x,t)");

        /* Formula input box */
        dprint(2, 11, C_BLACK, "y(x,t)=");
        drect_border(2, 20, 125, 36, C_WHITE, 1, C_BLACK);

        /* Horizontal scroll window (19 chars visible) */
        int vis = 19;
        int start = 0;
        if (cursor > vis - 2) start = cursor - (vis - 2);
        char view[24];
        strncpy(view, expr_buf + start, (size_t)vis);
        view[vis] = '\0';
        dprint(5, 25, C_BLACK, "%s", view);

        /* Draw cursor bar */
        int cx = 5 + (cursor - start) * 6;
        if (cx >= 5 && cx <= 122) {
            dline(cx, 23, cx, 33, C_BLACK);
        }

        /* Status / Error line */
        if (err_msg[0] != '\0') {
            dprint(2, 39, C_BLACK, "Err: %.19s", err_msg);
        } else {
            char s_tmin[12], s_tmax[12], s_dt[12];
            fmt_float(s_tmin, sizeof(s_tmin), cfg.tmin, 1);
            fmt_float(s_tmax, sizeof(s_tmax), cfg.tmax, 1);
            fmt_float(s_dt,   sizeof(s_dt),   cfg.dt,   2);
            dprint(2, 39, C_BLACK, "T:[%s..%s] dt=%s", s_tmin, s_tmax, s_dt);
        }
        dprint(2, 47, C_BLACK, "%s", loop_mode_name(cfg.mode));

        draw_fkeys("x", "t", "FUNC", "EJEM", "CONF", "GRAF");
        dupdate();

        key_event_t ev = getkey();
        err_msg[0] = '\0';

        if (ev.key == KEY_EXE || ev.key == KEY_F6) {
            if (expr_compile(expr_buf, &compiled, err_msg, (int)sizeof(err_msg))) {
                run_graph_screen(&compiled, &cfg);
            }
        } else if (ev.key == KEY_F1 || ev.key == KEY_XOT) {
            buf_insert(expr_buf, &cursor, EXPR_BUF_SIZE, "x");
        } else if (ev.key == KEY_F2) {
            buf_insert(expr_buf, &cursor, EXPR_BUF_SIZE, "t");
        } else if (ev.key == KEY_F3) {
            const char *f = popup_functions();
            if (f) buf_insert(expr_buf, &cursor, EXPR_BUF_SIZE, f);
        } else if (ev.key == KEY_F4) {
            const char *p = popup_presets();
            if (p) {
                snprintf(expr_buf, EXPR_BUF_SIZE, "%s", p);
                cursor = (int)strlen(expr_buf);
            }
        } else if (ev.key == KEY_F5) {
            run_config_screen(&cfg);
        } else if (ev.key == KEY_LEFT) {
            if (cursor > 0) cursor--;
        } else if (ev.key == KEY_RIGHT) {
            if (cursor < (int)strlen(expr_buf)) cursor++;
        } else if (ev.key == KEY_DEL) {
            buf_backspace(expr_buf, &cursor);
        } else if (ev.key == KEY_ACON) {
            expr_buf[0] = '\0';
            cursor = 0;
        } else {
            const char *ins = NULL;
            switch (ev.key) {
                case KEY_0: ins = "0"; break;
                case KEY_1: ins = "1"; break;
                case KEY_2: ins = "2"; break;
                case KEY_3: ins = "3"; break;
                case KEY_4: ins = "4"; break;
                case KEY_5: ins = "5"; break;
                case KEY_6: ins = "6"; break;
                case KEY_7: ins = "7"; break;
                case KEY_8: ins = "8"; break;
                case KEY_9: ins = "9"; break;
                case KEY_DOT: ins = "."; break;
                case KEY_ADD: ins = "+"; break;
                case KEY_SUB:
                case KEY_NEG: ins = "-"; break;
                case KEY_MUL: ins = "*"; break;
                case KEY_DIV: ins = "/"; break;
                case KEY_POWER: ins = "^"; break;
                case KEY_SQUARE: ins = "^2"; break;
                case KEY_LEFTP: ins = "("; break;
                case KEY_RIGHTP: ins = ")"; break;
                case KEY_SIN: ins = "sin("; break;
                case KEY_COS: ins = "cos("; break;
                case KEY_TAN: ins = "tan("; break;
                case KEY_LN:  ins = "ln("; break;
                case KEY_LOG: ins = "log("; break;
                default: break;
            }
            if (ins) {
                buf_insert(expr_buf, &cursor, EXPR_BUF_SIZE, ins);
            }
        }
    }

    return 1;
}
