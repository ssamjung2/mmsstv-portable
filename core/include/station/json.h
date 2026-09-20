// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 PocketSSTV contributors
//
// A deliberately small JSON implementation, sized for the station API and
// nothing else.
//
// Why not a library: the daemon speaks a protocol we define, on a socket that
// may be exposed to a network (R-CLI-6), so the parser is attack surface. A
// few hundred readable lines we fuzz ourselves is easier to reason about than
// a general-purpose dependency on a Raspberry Pi image. If this ever needs
// full JSON (unicode escapes beyond BMP, big-number semantics), replace it
// with a vetted library rather than growing it.
//
// Limits are explicit and enforced: nesting depth, and input length checked by
// the caller. Parsing never throws; errors come back as a message.

#ifndef STATION_JSON_H
#define STATION_JSON_H

#include <map>
#include <string>
#include <vector>

namespace station {
namespace json {

/* Maximum nesting depth. Hostile input should not be able to exhaust the
 * stack, and no legitimate API message comes close to this. */
constexpr int kMaxDepth = 32;

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Value() : type_(Type::Null) {}
    Value(bool b) : type_(Type::Bool), bool_(b) {}
    Value(double n) : type_(Type::Number), num_(n) {}
    Value(int n) : type_(Type::Number), num_(static_cast<double>(n)) {}
    Value(long long n) : type_(Type::Number), num_(static_cast<double>(n)) {}
    Value(const char *s) : type_(Type::String), str_(s ? s : "") {}
    Value(std::string s) : type_(Type::String), str_(std::move(s)) {}

    static Value array() { Value v; v.type_ = Type::Array; return v; }
    static Value object() { Value v; v.type_ = Type::Object; return v; }

    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }
    bool is_object() const { return type_ == Type::Object; }
    bool is_string() const { return type_ == Type::String; }
    bool is_number() const { return type_ == Type::Number; }

    bool as_bool(bool def = false) const { return type_ == Type::Bool ? bool_ : def; }
    double as_number(double def = 0.0) const { return type_ == Type::Number ? num_ : def; }
    const std::string &as_string() const { return str_; }

    /* Object access. Missing keys read as null, which keeps callers free of
     * existence checks for optional fields. */
    const Value &operator[](const std::string &key) const;
    Value &operator[](const std::string &key);
    bool has(const std::string &key) const;

    /* Array access. */
    void push_back(Value v) { arr_.push_back(std::move(v)); }
    const std::vector<Value> &items() const { return arr_; }

    const std::map<std::string, Value> &members() const { return obj_; }

    /* Compact serialisation. Object keys are emitted in sorted order, so
     * output is byte-stable and testable. */
    std::string dump() const;

private:
    Type type_;
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    std::vector<Value> arr_;
    std::map<std::string, Value> obj_;
};

/* Parse one JSON document. Returns false and fills `error` on failure; `out`
 * is then unspecified. Trailing whitespace is allowed, trailing content is
 * not. */
bool parse(const std::string &text, Value *out, std::string *error);

} // namespace json
} // namespace station

#endif
