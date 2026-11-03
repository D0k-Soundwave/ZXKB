// Minimal JSON reader. Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "json.h"
#include <cstdlib>
#include <cctype>
#include <cstring>

namespace {
const Json g_null;

struct Parser {
    const char *p, *end;
    bool        loose;
    int         depth = 0;

    void ws() {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    }
    static void utf8(std::string &out, unsigned c) {
        if (c < 0x80) out += (char)c;
        else if (c < 0x800) { out += (char)(0xC0 | c >> 6); out += (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) { out += (char)(0xE0 | c >> 12); out += (char)(0x80 | (c >> 6 & 63)); out += (char)(0x80 | (c & 63)); }
        else { out += (char)(0xF0 | c >> 18); out += (char)(0x80 | (c >> 12 & 63)); out += (char)(0x80 | (c >> 6 & 63)); out += (char)(0x80 | (c & 63)); }
    }
    unsigned hex4() {
        unsigned v = 0;
        for (int i = 0; i < 4 && p < end; i++, p++) {
            char c = *p;
            v = v * 16 + (c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 0);
        }
        return v;
    }
    bool string(std::string &out) {
        if (p >= end || *p != '"') return false;
        p++;
        while (p < end && *p != '"') {
            if (*p == '\\' && p + 1 < end) {
                p++;
                char c = *p++;
                switch (c) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u': {
                        unsigned u = hex4();
                        if (u >= 0xD800 && u < 0xDC00 && p + 1 < end && p[0] == '\\' && p[1] == 'u') {
                            p += 2;
                            unsigned lo = hex4();
                            u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        utf8(out, u);
                        break;
                    }
                    default: out += c;
                }
            } else {
                out += *p++;
            }
        }
        if (p >= end) return false;
        p++;
        return true;
    }
    bool key(std::string &out) { // KLE writes {x:1,w:2}: unquoted keys
        if (p < end && *p == '"') return string(out);
        if (!loose) return false;
        while (p < end && (isalnum((unsigned char)*p) || *p == '_')) out += *p++;
        return !out.empty();
    }
    bool value(Json &v) {
        if (++depth > 64) return false; // deeper than any keyboard file
        ws();
        bool ok = false;
        if (p >= end) ok = false;
        else if (*p == '{') {
            v.type = Json::OBJ;
            p++;
            ws();
            if (p < end && *p == '}') { p++; ok = true; }
            else
                for (;;) {
                    ws();
                    std::string k;
                    if (!key(k)) break;
                    ws();
                    if (p >= end || *p != ':') break;
                    p++;
                    Json m;
                    if (!value(m)) break;
                    v.members.emplace_back(std::move(k), std::move(m));
                    ws();
                    if (p < end && *p == ',') { p++; continue; }
                    if (p < end && *p == '}') { p++; ok = true; }
                    break;
                }
        } else if (*p == '[') {
            v.type = Json::ARR;
            p++;
            ws();
            if (p < end && *p == ']') { p++; ok = true; }
            else
                for (;;) {
                    Json m;
                    if (!value(m)) break;
                    v.items.push_back(std::move(m));
                    ws();
                    if (p < end && *p == ',') { p++; continue; }
                    if (p < end && *p == ']') { p++; ok = true; }
                    break;
                }
        } else if (*p == '"') {
            v.type = Json::STR;
            ok = string(v.str);
        } else if (end - p >= 4 && !strncmp(p, "true", 4)) { v.type = Json::BOOL; v.num = 1; p += 4; ok = true; }
        else if (end - p >= 5 && !strncmp(p, "false", 5)) { v.type = Json::BOOL; p += 5; ok = true; }
        else if (end - p >= 4 && !strncmp(p, "null", 4)) { p += 4; ok = true; }
        else {
            char *e = nullptr;
            std::string num;
            while (p < end && (isdigit((unsigned char)*p) || strchr("+-.eE", *p))) num += *p++;
            v.num  = strtod(num.c_str(), &e);
            v.type = Json::NUM;
            ok     = !num.empty();
        }
        depth--;
        return ok;
    }
};
} // namespace

const Json &Json::operator[](const char *key) const {
    if (type == OBJ)
        for (const auto &m : members)
            if (m.first == key) return m.second;
    return g_null;
}

const Json &Json::operator[](size_t i) const { return type == ARR && i < items.size() ? items[i] : g_null; }

int Json::to_int(int def) const {
    if (type == NUM) return (int)num;
    if (type == STR && !str.empty()) return (int)strtol(str.c_str(), nullptr, 0);
    return def;
}

Json Json::parse(const std::string &text, bool loose) {
    Parser ps{text.data(), text.data() + text.size(), loose};
    Json v;
    if (!ps.value(v)) return Json();
    return v;
}
