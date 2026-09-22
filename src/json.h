#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace poly {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() : type_(Type::Null) {}
    Json(std::nullptr_t) : type_(Type::Null) {}
    Json(bool b) : type_(Type::Bool), bool_(b) {}
    Json(int v) : type_(Type::Number), num_(v) {}
    Json(long long v) : type_(Type::Number), num_(static_cast<double>(v)) {}
    Json(double v) : type_(Type::Number), num_(v) {}
    Json(const char* s) : type_(Type::String), str_(s) {}
    Json(std::string s) : type_(Type::String), str_(std::move(s)) {}

    static Json array();
    static Json object();

    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }
    bool is_bool() const { return type_ == Type::Bool; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

    bool as_bool(bool def = false) const { return type_ == Type::Bool ? bool_ : def; }
    long long as_int(long long def = 0) const { return type_ == Type::Number ? static_cast<long long>(num_) : def; }
    double as_double(double def = 0.0) const { return type_ == Type::Number ? num_ : def; }
    const std::string& as_string() const;
    std::string as_string(const std::string& def) const { return type_ == Type::String ? str_ : def; }

    size_t size() const;
    const std::vector<Json>& items() const { return arr_; }

    Json& operator[](const std::string& key);
    const Json& operator[](const std::string& key) const;
    bool has(const std::string& key) const;
    void set(const std::string& key, Json value);
    void remove(const std::string& key);
    std::vector<std::string> keys() const;

    Json& push_back(Json value);
    const Json& at(size_t i) const;

    std::string dump(int indent = 2) const;
    static bool parse(const std::string& text, Json& out, std::string* error = nullptr);
    static Json parse_or(const std::string& text, Json fallback);

private:
    void dump_to(std::string& out, int indent, int depth) const;

    Type type_ = Type::Null;
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    std::vector<Json> arr_;
    std::vector<std::pair<std::string, Json>> obj_;
};

}  // namespace poly
