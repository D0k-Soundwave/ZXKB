// KLE layouts. Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "layout.h"
#include "json.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace kle {

static int h100(double v) { return (int)std::lround(v * 100); }

int colour(const std::string &hex) {
    if (hex.size() < 4 || hex[0] != '#') return -1;
    for (size_t i = 1; i < hex.size(); i++)
        if (!isxdigit((unsigned char)hex[i])) return -1;
    long v = strtol(hex.c_str() + 1, nullptr, 16);
    if (hex.size() == 4) return (int)(((v >> 8 & 15) * 17) << 16 | ((v >> 4 & 15) * 17) << 8 | (v & 15) * 17);
    return hex.size() == 7 ? (int)v : -1;
}

std::vector<KeyRect> from_kle(const Json &rows) {
    std::vector<KeyRect> keys;
    double y = 0;
    int cap = -1, side = -1;        // KLE colours carry on to the following keys until changed
    std::string text;
    for (const Json &row : rows.items) {
        if (!row.is(Json::ARR)) continue; // the optional first object holds board metadata
        double x = 0, w = 1, h = 1, x2 = 0, y2 = 0, w2 = 0, h2 = 0;
        bool   decal = false;
        std::string matrix; // "zm": this key's matrix position "row,col" (ZXKB keycap sets)
        for (const Json &item : row.items) {
            if (item.is(Json::OBJ)) { // properties for the next key
                const Json &v = item["x"]; if (v.is(Json::NUM)) x += v.num;
                if (item["y"].is(Json::NUM)) y += item["y"].num;
                if (item["w"].is(Json::NUM)) w = item["w"].num;
                if (item["h"].is(Json::NUM)) h = item["h"].num;
                if (item["x2"].is(Json::NUM)) x2 = item["x2"].num;
                if (item["y2"].is(Json::NUM)) y2 = item["y2"].num;
                if (item["w2"].is(Json::NUM)) w2 = item["w2"].num;
                if (item["h2"].is(Json::NUM)) h2 = item["h2"].num;
                if (item["d"].is(Json::BOOL)) decal = item["d"].num != 0;
                if (item["c"].is(Json::STR)) { cap = colour(item["c"].str); side = -1; }
                if (item["zs"].is(Json::STR)) side = colour(item["zs"].str);
                if (item["t"].is(Json::STR)) text = item["t"].str;
                if (item["zm"].is(Json::STR)) matrix = item["zm"].str;
                continue;
            }
            if (!item.is(Json::STR)) continue;
            KeyRect k;
            k.x = h100(x); k.y = h100(y); k.w = h100(w); k.h = h100(h);
            if (w2 > 0) { k.x2 = h100(x + x2); k.y2 = h100(y + y2); k.w2 = h100(w2); k.h2 = h100(h2 > 0 ? h2 : h); }
            // labels separated by newlines; VIA puts "row,col" in the first, "group,choice" in the 4th, "e0" in the 10th
            std::string s = item.str;
            for (size_t p = 0;;) {
                size_t n = s.find('\n', p);
                k.labels.push_back(s.substr(p, n == std::string::npos ? std::string::npos : n - p));
                if (n == std::string::npos) break;
                p = n + 1;
            }
            k.labels.resize(12);
            const std::string &rc = k.labels[0];
            size_t comma = rc.find(',');
            if (comma != std::string::npos && comma > 0 && isdigit((unsigned char)rc[0])) {
                k.row = atoi(rc.c_str());
                k.col = atoi(rc.c_str() + comma + 1);
            }
            k.knob = !k.labels[9].empty() && k.labels[9][0] == 'e';
            if (k.knob) k.encoder = atoi(k.labels[9].c_str() + 1);
            if (!matrix.empty()) {
                size_t mc = matrix.find(',');
                if (mc != std::string::npos) { k.row = atoi(matrix.c_str()); k.col = atoi(matrix.c_str() + mc + 1); }
            }
            k.cap_rgb = cap;
            k.side_rgb = side;
            { // legend colours: one per label, or one for all
                std::vector<std::string> tc;
                for (size_t p = 0;;) {
                    size_t n = text.find('\n', p);
                    tc.push_back(text.substr(p, n == std::string::npos ? std::string::npos : n - p));
                    if (n == std::string::npos) break;
                    p = n + 1;
                }
                for (int i = 0; i < 12; i++) k.text_rgb[i] = colour(tc.size() == 1 ? tc[0] : i < (int)tc.size() ? tc[i] : std::string());
            }
            bool option_ok = true;
            const std::string &opt = k.labels[3];
            size_t oc = opt.find(',');
            if (oc != std::string::npos && isdigit((unsigned char)opt[0])) option_ok = atoi(opt.c_str() + oc + 1) == 0;
            if (!decal && option_ok) keys.push_back(k);
            x += w;
            w = h = 1;
            x2 = y2 = w2 = h2 = 0;
            decal = false;
            matrix.clear();
        }
        y += 1;
    }
    // VIA layout options move alternatives elsewhere; the kept (choice 0) keys can start away from 0,0
    if (!keys.empty()) {
        int mx = keys[0].x, my = keys[0].y;
        for (const KeyRect &k : keys) { mx = std::min(mx, k.x); my = std::min(my, k.y); }
        for (KeyRect &k : keys) { k.x -= mx; k.y -= my; if (k.w2) { k.x2 -= mx; k.y2 -= my; } }
    }
    return keys;
}

static KeyRect compiled_key(const Json &k) {
    KeyRect r;
    double x = k["x"].num, y = k["y"].num, w = k["w"].is(Json::NUM) ? k["w"].num : 1, h = k["h"].is(Json::NUM) ? k["h"].num : 1;
    r.x = h100(x); r.y = h100(y); r.w = h100(w); r.h = h100(h);
    if (k["w2"].is(Json::NUM) && k["w2"].num > 0) {
        r.x2 = h100(x + k["x2"].num);
        r.y2 = h100(y + k["y2"].num);
        r.w2 = h100(k["w2"].num);
        r.h2 = h100(k["h2"].is(Json::NUM) ? k["h2"].num : h);
    }
    r.row  = k["row"].to_int(-1);
    r.col  = k["col"].to_int(-1);
    r.knob = k["ei"].is(Json::NUM);
    r.encoder = k["ei"].to_int(-1);
    r.labels.resize(12);
    return r;
}

std::vector<KeyRect> from_via(const Json &layouts) {
    const Json &keys = layouts["keys"];
    if (!keys.is(Json::ARR)) return from_kle(layouts["keymap"]);
    std::vector<KeyRect> out;
    for (const Json &k : keys.items)
        if (!(k["d"].is(Json::BOOL) && k["d"].num)) out.push_back(compiled_key(k)); // "d": decal, not a key
    for (const auto &group : layouts["optionKeys"].members) // {"0": {"0": [keys], "1": [keys]}}: first choice
        for (const Json &k : group.second["0"].items) out.push_back(compiled_key(k));
    return out;
}

void bounds(const std::vector<KeyRect> &keys, int *w, int *h) {
    *w = *h = 0;
    for (const KeyRect &k : keys) {
        *w = std::max(*w, k.x + k.w);
        *h = std::max(*h, k.y + k.h);
        if (k.w2) { *w = std::max(*w, k.x2 + k.w2); *h = std::max(*h, k.y2 + k.h2); }
    }
}

} // namespace kle
