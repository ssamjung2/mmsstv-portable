// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 PocketSSTV contributors

#include "station/json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace station {
namespace json {

namespace {
/* A function-local static rather than a namespace-scope object: initialised on
 * first use, so nothing runs before main, and thread-safe since C++11. */
const Value &null_value() {
    static const Value v;
    return v;
}

void dump_string(const std::string &s, std::string *out) {
    out->push_back('"');
    for (unsigned char c : s) {
        switch (c) {
        case '"':
            *out += "\\\"";
            break;
        case '\\':
            *out += "\\\\";
            break;
        case '\n':
            *out += "\\n";
            break;
        case '\r':
            *out += "\\r";
            break;
        case '\t':
            *out += "\\t";
            break;
        case '\b':
            *out += "\\b";
            break;
        case '\f':
            *out += "\\f";
            break;
        default:
            if (c < 0x20) {
                /* Control characters must be escaped; anything else (including
                 * UTF-8 continuation bytes) passes through untouched. */
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", c);
                *out += buf;
            } else {
                out->push_back(static_cast<char>(c));
            }
        }
    }
    out->push_back('"');
}

void dump_number(double n, std::string *out) {
    char buf[32];
    /* Integers print without a decimal point: sequence numbers and line
     * indices read better as 42 than 42.0, and shell tools parse them. */
    if (std::isfinite(n) && n == static_cast<double>(static_cast<long long>(n))) {
        std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(n));
    } else if (std::isfinite(n)) {
        std::snprintf(buf, sizeof buf, "%.10g", n);
    } else {
        /* JSON has no NaN or Infinity. Emitting null is the least surprising
         * option and keeps the output parseable. */
        *out += "null";
        return;
    }
    *out += buf;
}

class Parser {
public:
    Parser(const std::string &text) : s_(text) {}

    bool parse(Value *out, std::string *error) {
        skip_ws();
        if (!parse_value(out, 0)) {
            *error = err_;
            return false;
        }
        skip_ws();
        if (pos_ != s_.size()) {
            *error = "trailing content after JSON value";
            return false;
        }
        return true;
    }

private:
    const std::string &s_;
    size_t pos_ = 0;
    std::string err_;

    bool fail(const char *what) {
        if (err_.empty()) err_ = what;
        return false;
    }
    void skip_ws() {
        while (pos_ < s_.size()) {
            char c = s_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
                pos_++;
            else
                break;
        }
    }
    bool literal(const char *lit) {
        size_t n = std::string(lit).size();
        if (s_.compare(pos_, n, lit) != 0) return fail("invalid literal");
        pos_ += n;
        return true;
    }

    bool parse_value(Value *out, int depth) {
        if (depth > kMaxDepth) return fail("nesting too deep");
        if (pos_ >= s_.size()) return fail("unexpected end of input");
        switch (s_[pos_]) {
        case 'n':
            if (!literal("null")) return false;
            *out = Value();
            return true;
        case 't':
            if (!literal("true")) return false;
            *out = Value(true);
            return true;
        case 'f':
            if (!literal("false")) return false;
            *out = Value(false);
            return true;
        case '"':
            return parse_string(out);
        case '[':
            return parse_array(out, depth);
        case '{':
            return parse_object(out, depth);
        default:
            return parse_number(out);
        }
    }

    bool parse_string(Value *out) {
        if (s_[pos_] != '"') return fail("expected string");
        pos_++;
        std::string result;
        while (true) {
            if (pos_ >= s_.size()) return fail("unterminated string");
            unsigned char c = static_cast<unsigned char>(s_[pos_++]);
            if (c == '"') break;
            if (c < 0x20) return fail("control character in string");
            if (c != '\\') {
                result.push_back(static_cast<char>(c));
                continue;
            }
            if (pos_ >= s_.size()) return fail("unterminated escape");
            char e = s_[pos_++];
            switch (e) {
            case '"':
                result.push_back('"');
                break;
            case '\\':
                result.push_back('\\');
                break;
            case '/':
                result.push_back('/');
                break;
            case 'n':
                result.push_back('\n');
                break;
            case 'r':
                result.push_back('\r');
                break;
            case 't':
                result.push_back('\t');
                break;
            case 'b':
                result.push_back('\b');
                break;
            case 'f':
                result.push_back('\f');
                break;
            case 'u': {
                /* \uXXXX, encoded as UTF-8. Surrogate pairs are joined; a lone
                 * surrogate becomes U+FFFD rather than invalid UTF-8, because
                 * callers downstream assume well-formed text. */
                unsigned cp = 0;
                if (!hex4(&cp)) return false;
                if (cp >= 0xD800u && cp <= 0xDBFFu) {
                    unsigned lo = 0;
                    if (pos_ + 1 < s_.size() && s_[pos_] == '\\' && s_[pos_ + 1] == 'u') {
                        pos_ += 2;
                        if (!hex4(&lo)) return false;
                        if (lo >= 0xDC00u && lo <= 0xDFFFu)
                            cp = 0x10000u + ((cp - 0xD800u) << 10u) + (lo - 0xDC00u);
                        else
                            cp = 0xFFFDu;
                    } else
                        cp = 0xFFFDu;
                } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
                    cp = 0xFFFDu;
                }
                append_utf8(cp, &result);
                break;
            }
            default:
                return fail("invalid escape");
            }
        }
        *out = Value(std::move(result));
        return true;
    }

    bool hex4(unsigned *out) {
        if (pos_ + 4 > s_.size()) return fail("truncated \\u escape");
        unsigned v = 0;
        for (int i = 0; i < 4; i++) {
            char c = s_[pos_++];
            v <<= 4u;
            if (c >= '0' && c <= '9')
                v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f')
                v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                v |= static_cast<unsigned>(c - 'A' + 10);
            else
                return fail("invalid hex in \\u escape");
        }
        *out = v;
        return true;
    }

    static void append_utf8(unsigned cp, std::string *out) {
        /* Unsigned literals throughout: mixing signed constants into bit
         * manipulation is how encoders acquire sign-extension bugs. */
        if (cp < 0x80u) {
            out->push_back(static_cast<char>(cp));
        } else if (cp < 0x800u) {
            out->push_back(static_cast<char>(0xC0u | (cp >> 6u)));
            out->push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else if (cp < 0x10000u) {
            out->push_back(static_cast<char>(0xE0u | (cp >> 12u)));
            out->push_back(static_cast<char>(0x80u | ((cp >> 6u) & 0x3Fu)));
            out->push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else {
            out->push_back(static_cast<char>(0xF0u | (cp >> 18u)));
            out->push_back(static_cast<char>(0x80u | ((cp >> 12u) & 0x3Fu)));
            out->push_back(static_cast<char>(0x80u | ((cp >> 6u) & 0x3Fu)));
            out->push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        }
    }

    bool parse_number(Value *out) {
        size_t start = pos_;
        if (pos_ < s_.size() && (s_[pos_] == '-' || s_[pos_] == '+')) pos_++;
        bool digits = false;
        while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
            pos_++;
            digits = true;
        }
        if (pos_ < s_.size() && s_[pos_] == '.') {
            pos_++;
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
                pos_++;
                digits = true;
            }
        }
        if (digits && pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
            pos_++;
            if (pos_ < s_.size() && (s_[pos_] == '-' || s_[pos_] == '+')) pos_++;
            bool edigits = false;
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
                pos_++;
                edigits = true;
            }
            if (!edigits) return fail("malformed exponent");
        }
        if (!digits) return fail("expected value");
        *out = Value(std::strtod(s_.substr(start, pos_ - start).c_str(), nullptr));
        return true;
    }

    bool parse_array(Value *out, int depth) {
        pos_++; /* '[' */
        Value arr = Value::array();
        skip_ws();
        if (pos_ < s_.size() && s_[pos_] == ']') {
            pos_++;
            *out = std::move(arr);
            return true;
        }
        while (true) {
            skip_ws();
            Value item;
            if (!parse_value(&item, depth + 1)) return false;
            arr.push_back(std::move(item));
            skip_ws();
            if (pos_ >= s_.size()) return fail("unterminated array");
            if (s_[pos_] == ',') {
                pos_++;
                continue;
            }
            if (s_[pos_] == ']') {
                pos_++;
                break;
            }
            return fail("expected ',' or ']'");
        }
        *out = std::move(arr);
        return true;
    }

    bool parse_object(Value *out, int depth) {
        pos_++; /* '{' */
        Value obj = Value::object();
        skip_ws();
        if (pos_ < s_.size() && s_[pos_] == '}') {
            pos_++;
            *out = std::move(obj);
            return true;
        }
        while (true) {
            skip_ws();
            Value key;
            if (pos_ >= s_.size() || s_[pos_] != '"') return fail("expected object key");
            if (!parse_string(&key)) return false;
            skip_ws();
            if (pos_ >= s_.size() || s_[pos_] != ':') return fail("expected ':'");
            pos_++;
            skip_ws();
            Value val;
            if (!parse_value(&val, depth + 1)) return false;
            obj[key.as_string()] = std::move(val);
            skip_ws();
            if (pos_ >= s_.size()) return fail("unterminated object");
            if (s_[pos_] == ',') {
                pos_++;
                continue;
            }
            if (s_[pos_] == '}') {
                pos_++;
                break;
            }
            return fail("expected ',' or '}'");
        }
        *out = std::move(obj);
        return true;
    }
};

} // namespace

const Value &Value::operator[](const std::string &key) const {
    auto it = obj_.find(key);
    return it == obj_.end() ? null_value() : it->second;
}

Value &Value::operator[](const std::string &key) {
    type_ = Type::Object;
    return obj_[key];
}

bool Value::has(const std::string &key) const {
    return type_ == Type::Object && obj_.find(key) != obj_.end();
}

std::string Value::dump() const {
    std::string out;
    switch (type_) {
    case Type::Null:
        out += "null";
        break;
    case Type::Bool:
        out += bool_ ? "true" : "false";
        break;
    case Type::Number:
        dump_number(num_, &out);
        break;
    case Type::String:
        dump_string(str_, &out);
        break;
    case Type::Array: {
        out.push_back('[');
        for (size_t i = 0; i < arr_.size(); i++) {
            if (i) out.push_back(',');
            out += arr_[i].dump();
        }
        out.push_back(']');
        break;
    }
    case Type::Object: {
        out.push_back('{');
        bool first = true;
        for (const auto &kv : obj_) {
            if (!first) out.push_back(',');
            first = false;
            dump_string(kv.first, &out);
            out.push_back(':');
            out += kv.second.dump();
        }
        out.push_back('}');
        break;
    }
    }
    return out;
}

bool parse(const std::string &text, Value *out, std::string *error) {
    std::string ignored;
    if (!error) error = &ignored;
    Parser p(text);
    return p.parse(out, error);
}

} // namespace json
} // namespace station
