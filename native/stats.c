#include "color.h"
#include "sink.h"

enum {
    ROW_LEN   = 10,
    BAR_WIDTH = 5,
    SPACING   = 1,
};

enum {
    STATE_INIT,
    STATE_TITLE,
    STATE_ROW,
    STATE_VALUE,
    STATE_TABLE,
};

static void putsUpper(ChiSink* sink, const char* s) {
    for (const char* p = s; *p; ++p)
        chiSinkPutc(sink, chiToUpper(*p));
}

static void jsonLabel(ChiSink* sink, const char* s) {
    bool escaped = false;
    chiSinkPutc(sink, '"');
    for (; *s; ++s) {
        if (chiAlnum(*s)) {
            escaped = false;
            chiSinkPutc(sink, *s);
        } else if (!escaped) {
            chiSinkPutc(sink, '_');
            escaped = true;
        }
    }
    chiSinkPuts(sink, "\": ");
}

static void formatBar(ChiSink* cell, double val) {
    char buf[3 * BAR_WIDTH], *p = buf;
    val *= BAR_WIDTH;
    for (uint32_t i = 0; i < BAR_WIDTH; ++i) {
        int len = (int)(8 * (val - i) + 0.5);
        if (len > 0) {
            *p++ = '\xE2';
            *p++ = '\x96';
            *p++ = (char)(0x8F - CHI_MIN(7, len - 1));
        } else {
            *p++ = ' ';
        }
    }
    chiSinkWrite(cell, buf, (size_t)(p - buf));
}

static size_t formatPercent(ChiSink* cell, double x, double max, bool cumulative) {
    chiSinkPuts(cell, cumulative
                 ? (x >= 0.99 ? FgRed : x >= 0.9 ? FgYellow : x >= 0.5 ? FgGreen : FgWhite)
                 : (x >= 0.1 ? FgRed : x >= 0.05 ? FgYellow : x >= 0.005 ? FgGreen : FgWhite));
    formatBar(cell, max != 0 ? x / max : 0);
    size_t len = chiSinkFmt(cell, "%7.2f%%", 100*x);
    chiSinkPuts(cell, FgDefault);
    return BAR_WIDTH + len;
}

static size_t formatCell(ChiSink* cell, const ChiStatsColumn* col, uint32_t y, double max, size_t cellSize) {
    switch (col->type) {
    case CHI_STATS_INT:
        if (col->percent)
            return formatPercent(cell, col->sum ? (double)col->ints[y] / (double)col->sum : 0, max, col->cumulative);
        if (!col->ints[y]) {
            chiSinkPuts(cell, FgWhite"0"FgDefault);
            return 1;
        }
        return chiSinkPutu(cell, col->ints[y]);
    case CHI_STATS_FLOAT:
        if (col->percent)
            return formatPercent(cell, col->floats[y], max, col->cumulative);
        if (col->floats[y] == 0) {
            chiSinkPuts(cell, FgWhite"0"FgDefault);
            return 1;
        }
        return chiSinkFmt(cell, "%.2f", col->floats[y]);
    case CHI_STATS_PATH:
        if (col->strings[y].size > cellSize - 1U) {
            const uint8_t* start = col->strings[y].bytes, *end = start + col->strings[y].size, *p = start, *q;
            size_t maxlen = cellSize - 1U, written = 0, slashes = 0;
            while (p < end && (q = (const uint8_t*)memchr(p, '/', (size_t)(end - p)))) {
                p = q + 1;
                ++slashes;
            }
            size_t w = maxlen > (size_t)(end - p) ? (maxlen - (size_t)(end - p)) / (slashes + 1) + 1 : 1;
            p = start;
            while (written < maxlen && p < end && (q = (const uint8_t*)memchr(p, '/', (size_t)(end - p)))) {
                size_t m = CHI_MIN((size_t)(q - p), w);
                chiSinkWrite(cell, p, m);
                chiSinkPutc(cell, '/');
                p = q + 1;
                written += m + 1;
            }
            chiSinkWrite(cell, p, (size_t)(end - p));
            return written + (size_t)(end - p);
        }
        // fall through
    case CHI_STATS_STRING:
        {
            size_t n = CHI_MIN(cellSize - 1U, col->strings[y].size);
            chiSinkWrite(cell, col->strings[y].bytes, n);
            return n;
        }
    default:
        CHI_BUG("Invalid stats type");
    }
}

static void alignCell(ChiSink* sink, size_t x, size_t n, ChiStringRef cell, const ChiStatsColumn* col, size_t width, size_t ncols) {
    chiSinkFmt(sink, "%*w%S%*w%s",
               (uint32_t)(width - n) * (!col[x].left) + (uint32_t)SPACING * (x > 0),
               cell,
               (x < ncols - 1) * ((uint32_t)(width - n) * col[x].left + (uint32_t)SPACING * col[x].sep),
               x < ncols - 1 && col[x].sep ? FgWhite"│"FgDefault : "");
}

static void prettyTitle(ChiStats* s, const char* title) {
    ChiSink* sink = s->sink;
    chiSinkPuts(sink, s->state == STATE_INIT ? TitleBegin : "\n\n"TitleBegin);
    putsUpper(sink, title);
    chiSinkPuts(sink, TitleEnd);
}

static void jsonTitle(ChiStats* s, const char* title) {
    ChiSink* sink = s->sink;
    if (s->state == STATE_INIT)
        chiSinkPuts(sink, "{\n  ");
    else if (s->state == STATE_TABLE)
        chiSinkPuts(sink, ",\n  ");
    else if (s->state == STATE_VALUE)
        chiSinkPuts(sink, "\n    }\n  },\n  ");
    jsonLabel(sink, title);
    chiSinkPuts(sink, "{\n");
}

static void prettyTable(ChiStats* s, const ChiStatsTable* table) {
    ChiSink* sink = s->sink;

    uint32_t ncols = table->columns, nrows = table->rows;
    const ChiStatsColumn* col = table->data;

    CHI_AUTO_ZALLOC(double, max, ncols);
    CHI_AUTO_ZALLOC(size_t, width, ncols);
    for (uint32_t x = 0; x < ncols; ++x) {
        for (uint32_t y = 0; y < nrows; ++y) {
            if (col[x].type == CHI_STATS_FLOAT && col[x].percent)
                max[x] = CHI_MAX(max[x], col[x].floats[y]);
            else if (col[x].type == CHI_STATS_INT && col[x].percent)
                max[x] = CHI_MAX(max[x], col[x].sum ? (double)col[x].ints[y] / (double)col[x].sum : 0);
        }
    }

    CHI_STRING_SINK(cell);
    for (uint32_t x = 0; x < ncols; ++x) {
        width[x] = strlen(col[x].header);
        for (uint32_t y = 0; y < nrows; ++y) {
            width[x] = CHI_MAX(width[x], formatCell(cell, col + x, y, max[x], s->cell));
            chiSinkString(cell); // reset
        }
    }

    chiSinkPutc(sink, '\n');
    for (uint32_t x = 0; x < ncols; ++x) {
        chiSinkFmt(cell, StyUnder"%s"StyNounder, col[x].header);
        alignCell(sink, x, strlen(col[x].header), chiSinkString(cell), col, width[x], ncols);
    }
    for (uint32_t y = 0; y < nrows; ++y) {
        chiSinkPutc(sink, '\n');
        for (uint32_t x = 0; x < ncols; ++x) {
            size_t w = formatCell(cell, col + x, y, max[x], s->cell);
            alignCell(sink, x, w, chiSinkString(cell), col, width[x], ncols);
        }
    }
}

static void jsonCell(ChiSink* sink, uint32_t y, const ChiStatsColumn* col) {
    switch (col->type) {
    case CHI_STATS_INT:
        if (col->percent)
            chiSinkFmt(sink, "%f", col->sum ? 100 * (double)col->ints[y] / (double)col->sum : 0);
        else
            chiSinkPutu(sink, col->ints[y]);
        break;
    case CHI_STATS_FLOAT:
        chiSinkFmt(sink, "%f", col->percent ?
                    100 * col->floats[y] : col->floats[y]);
        break;
    case CHI_STATS_STRING:
    case CHI_STATS_PATH:
        chiSinkPutq(sink, col->strings[y]);
        break;
    }
}

static void jsonTable(ChiStats* s, const ChiStatsTable* table) {
    ChiSink* sink = s->sink;
    chiSinkPuts(sink, "    \"header\": [");

    uint32_t ncols = table->columns, nrows = table->rows;
    const ChiStatsColumn* col = table->data;
    for (uint32_t x = 0; x < ncols; ++x) {
        if (x)
            chiSinkPuts(sink, ", ");
        chiSinkPutq(sink, col[x].header);
    }
    chiSinkPuts(sink, "],\n    \"rows\": [\n");
    for (uint32_t y = 0; y < nrows; ++y) {
        chiSinkPuts(sink, "      [");
        for (uint32_t x = 0; x < ncols; ++x) {
            if (x)
                chiSinkPuts(sink, ", ");
            jsonCell(sink, y, col + x);
        }
        chiSinkPuts(sink, y == nrows - 1 ? "]\n" : "],\n");
    }
    chiSinkPuts(sink, "    ]\n  }");
}

static void tableFree(const ChiStatsTable* table) {
    for (uint32_t x = 0; x < table->columns; ++x) {
        uint32_t y = 0;
        for (y = 0; y < x && table->data[x].ints != table->data[y].ints; ++y) {}
        if (y == x)
            chiFree(table->data[x].ints);
    }
}

void chiStatsSetup(ChiStats* s, uint32_t cell, bool json) {
    s->sink = chiSinkStringNew();
    s->state = STATE_INIT;
    s->json = json;
    s->cell = cell;
}

void chiStatsDestroy(ChiStats* s, const char* file, ChiSinkColor color) {
    ChiStringRef str = chiSinkString(s->sink);
    if (str.size) {
        CHI_AUTO_SINK(sink, chiSinkFileTryNew(file[0] ? file : "stderr", CHI_KiB(8), false, color));
        if (sink) {
            if (s->json) {
                chiSinkPuts(sink, str);
                if (s->state == STATE_VALUE)
                    chiSinkPuts(sink, "\n    }\n  }\n}\n");
                else if (s->state == STATE_TABLE)
                    chiSinkPuts(sink, "\n}\n");

            } else {
                chiSinkPuts(sink, TitleBegin "CHILI RUNTIME STATISTICS\n\n"TitleBegin);
                chiSinkPuts(sink, str);
                chiSinkPutc(sink, '\n');
            }
        }
    }
    chiSinkClose(s->sink);
    CHI_POISON_STRUCT(s, CHI_POISON_DESTROYED);
}

void chiStatsTitle(ChiStats* s, const char* n) {
    if (s->json)
        jsonTitle(s, n);
    else
        prettyTitle(s, n);
    s->state = STATE_TITLE;
}

static void jsonRow(ChiStats* s, const char* n) {
    ChiSink* sink = s->sink;
    if (s->state == STATE_VALUE)
        chiSinkPuts(sink, "\n    },\n");
    chiSinkPuts(sink, "    ");
    jsonLabel(sink, n);
    chiSinkPuts(sink, "{\n");
}

static void prettyRow(ChiStats* s, const char* n) {
    ChiSink* sink = s->sink;
    chiSinkFmt(sink, "\n%*s · ", ROW_LEN, n);
}

void chiStatsRow(ChiStats* s, const char* n) {
    if (s->json)
        jsonRow(s, n);
    else
        prettyRow(s, n);
    s->state = STATE_ROW;
}

static void valueBegin(ChiStats* s, const char* name) {
    ChiSink* sink = s->sink;
    if (s->json) {
        if (s->state == STATE_VALUE)
            chiSinkPuts(sink, ",\n");
        chiSinkPuts(sink, "      ");
        jsonLabel(sink, name);
        chiSinkPuts(sink, "{ ");
    } else {
        if (s->state == STATE_VALUE)
            chiSinkPutc(sink, ' ');
        if (name)
            chiSinkFmt(sink, "%s:", name);
    }
}

static void jsonUnit(ChiSink* sink, const char* unit) {
    if (unit)
        chiSinkFmt(sink, ", \"unit\": \"%s\"", unit);
}

static void jsonUInt(ChiSink* sink, uint64_t x, const char* unit) {
    chiSinkFmt(sink, "\"value\": %ju", x);
    jsonUnit(sink, unit);
}

static void valueEnd(ChiStats* s) {
    if (s->json)
        chiSinkPuts(s->sink, " }");
    s->state = STATE_VALUE;
}

void chiStatsBytesPerSec(ChiStats* s, const char* name, double v) {
    ChiSink* sink = s->sink;
    valueBegin(s, name);
    if (s->json) {
        chiSinkFmt(sink, "\"value\": %f", v);
        jsonUnit(sink, "B/s");
    } else {
        if (v >= CHI_TiB(1))
            chiSinkFmt(sink, "%.2fT/s", v / CHI_TiB(1));
        else if (v >= CHI_GiB(1))
            chiSinkFmt(sink, "%.2fG/s", v / CHI_GiB(1));
        else if (v >= CHI_MiB(1))
            chiSinkFmt(sink, "%.2fM/s", v / CHI_MiB(1));
        else if (v >= CHI_KiB(1))
            chiSinkFmt(sink, "%.2fK/s", v / CHI_KiB(1));
        else
            chiSinkFmt(sink, "%.2fB/s", v);
    }
    valueEnd(s);
}

void chiStatsString(ChiStats* s, const char* name, ChiStringRef v) {
    ChiSink* sink = s->sink;
    valueBegin(s, name);
    if (s->json)
        chiSinkFmt(sink, "\"value\": %qS", v);
    else
        chiSinkPuts(sink, v);
    valueEnd(s);
}

void chiStatsBytes(ChiStats* s, const char* name, uint64_t v) {
    ChiSink* sink = s->sink;
    valueBegin(s, name);
    if (s->json) {
        jsonUInt(sink, v, "B");
    } else {
        if (v >= CHI_TiB(1))
            chiSinkFmt(sink, "%.2fT", (double)v / CHI_TiB(1));
        else if (v >= CHI_GiB(1))
            chiSinkFmt(sink, "%.2fG", (double)v / CHI_GiB(1));
        else if (v >= CHI_MiB(1))
            chiSinkFmt(sink, "%.2fM", (double)v / CHI_MiB(1));
        else if (v >= CHI_KiB(1))
            chiSinkFmt(sink, "%.2fK", (double)v / CHI_KiB(1));
        else
            chiSinkPutu(sink, v);
    }
    valueEnd(s);
}

void chiStatsTime(ChiStats* s, const char* name, ChiNanos t) {
    ChiSink* sink = s->sink;
    valueBegin(s, name);
    if (s->json)
        jsonUInt(sink, CHI_UN(Nanos, t), "ns");
    else
        chiSinkFmt(sink, "%t", t);
    valueEnd(s);
}

void chiStatsIntUnit(ChiStats* s, const char* name, uint64_t v, bool exp, const char* unit) {
    ChiSink* sink = s->sink;
    valueBegin(s, name);
    if (s->json) {
        jsonUInt(sink, v, unit);
    } else {
        if (exp)
            chiSinkFmt(sink, "%.2e", (double)v);
        else
            chiSinkPutu(sink, v);
        if (unit)
            chiSinkPuts(sink, unit);
    }
    valueEnd(s);
}

void chiStatsFloatUnit(ChiStats* s, const char* name, double v, bool exp, const char* unit) {
    ChiSink* sink = s->sink;
    valueBegin(s, name);
    if (s->json) {
        chiSinkFmt(sink, "\"value\": %f", v);
        jsonUnit(sink, unit);
    } else {
        if (exp)
            chiSinkFmt(sink, "%.2e", v);
        else
            chiSinkFmt(sink, "%.2f", v);
        if (unit)
            chiSinkPuts(sink, unit);
    }
    valueEnd(s);
}

void chiStatsAddTable(ChiStats* s, const ChiStatsTable* table) {
    if (s->json || table->rows) {
        chiStatsTitle(s, table->title);
        if (s->json)
            jsonTable(s, table);
        else
            prettyTable(s, table);
        s->state = STATE_TABLE;
    }
    tableFree(table);
}
