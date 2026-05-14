#pragma once
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <stdexcept>
#include <sstream>
#include <charconv>
#include <algorithm>
#include <iterator>
#include <type_traits>
#include <windows.h>

// 前置声明
class MyJson;

// JSON值类型枚举
enum class JsonValueType {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

// 核心JSON类
class MyJson {
public:
    // 空构造（默认null）
    MyJson() : type(JsonValueType::Null) {}

    // 基础类型构造
    MyJson(std::nullptr_t) : type(JsonValueType::Null) {}
    MyJson(bool b) : type(JsonValueType::Bool), value(b) {}
    MyJson(int i) : type(JsonValueType::Number), value(static_cast<double>(i)) {}
    MyJson(float f) : type(JsonValueType::Number), value(static_cast<double>(f)) {}
    MyJson(double d) : type(JsonValueType::Number), value(d) {}
    MyJson(const std::string& s) : type(JsonValueType::String), value(s) {}
    MyJson(const char* s) : type(JsonValueType::String), value(std::string(s)) {}
    MyJson(const std::vector<MyJson>& arr) : type(JsonValueType::Array), value(arr) {}
    MyJson(const std::map<std::string, MyJson>& obj) : type(JsonValueType::Object), value(obj) {}

    // 列表初始化构造
    MyJson(std::initializer_list<std::pair<const std::string, MyJson>> init)
        : type(JsonValueType::Object), value(std::map<std::string, MyJson>(init)) {
    }
    MyJson(std::initializer_list<MyJson> init)
        : type(JsonValueType::Array), value(std::vector<MyJson>(init)) {
    }

    // 拷贝/移动构造
    MyJson(const MyJson&) = default;
    MyJson(MyJson&&) = default;

    // 拷贝/移动赋值
    MyJson& operator=(const MyJson&) = default;
    MyJson& operator=(MyJson&&) = default;

    // 基础类型赋值运算符
    MyJson& operator=(std::nullptr_t) {
        type = JsonValueType::Null;
        value = nullptr;
        return *this;
    }
    MyJson& operator=(bool b) {
        type = JsonValueType::Bool;
        value = b;
        return *this;
    }
    MyJson& operator=(int i) {
        type = JsonValueType::Number;
        value = static_cast<double>(i);
        return *this;
    }
    MyJson& operator=(float f) {
        type = JsonValueType::Number;
        value = static_cast<double>(f);
        return *this;
    }
    MyJson& operator=(double d) {
        type = JsonValueType::Number;
        value = d;
        return *this;
    }
    MyJson& operator=(const std::string& s) {
        type = JsonValueType::String;
        value = s;
        return *this;
    }
    MyJson& operator=(const char* s) {
        type = JsonValueType::String;
        value = std::string(s);
        return *this;
    }
    MyJson& operator=(const std::vector<MyJson>& arr) {
        type = JsonValueType::Array;
        value = arr;
        return *this;
    }
    MyJson& operator=(const std::map<std::string, MyJson>& obj) {
        type = JsonValueType::Object;
        value = obj;
        return *this;
    }

    // 非const下标访问
    MyJson& operator[](const std::string& key) {
        if (type != JsonValueType::Object) {
            type = JsonValueType::Object;
            value = std::map<std::string, MyJson>();
        }
        return std::get<std::map<std::string, MyJson>>(value)[key];
    }
    MyJson& operator[](size_t index) {
        if (type != JsonValueType::Array) {
            type = JsonValueType::Array;
            value = std::vector<MyJson>();
        }
        auto& arr = std::get<std::vector<MyJson>>(value);
        if (index >= arr.size()) {
            arr.resize(index + 1);
        }
        return arr[index];
    }

    // const下标访问
    const MyJson& operator[](const std::string& key) const {
        if (type != JsonValueType::Object) {
            throw std::out_of_range("Not an object");
        }
        const auto& obj = std::get<std::map<std::string, MyJson>>(value);
        auto it = obj.find(key);
        if (it == obj.end()) {
            throw std::out_of_range("Key not found: " + key);
        }
        return it->second;
    }
    const MyJson& operator[](size_t index) const {
        if (type != JsonValueType::Array) {
            throw std::out_of_range("Not an array");
        }
        const auto& arr = std::get<std::vector<MyJson>>(value);
        if (index >= arr.size()) {
            throw std::out_of_range("Index out of range");
        }
        return arr[index];
    }

    size_t size() const {
        if (type == JsonValueType::Array)
            return std::get<std::vector<MyJson>>(value).size();
        else if (type == JsonValueType::Object)
            return std::get<std::map<std::string, MyJson>>(value).size();
        else
            throw std::runtime_error("size() is only valid for arrays and objects");
    }

    // at 方法
    MyJson& at(const std::string& key) {
        if (type != JsonValueType::Object) throw std::out_of_range("Not an object");
        auto& obj = std::get<std::map<std::string, MyJson>>(value);
        auto it = obj.find(key);
        if (it == obj.end()) throw std::out_of_range("Key not found: " + key);
        return it->second;
    }
    const MyJson& at(const std::string& key) const {
        if (type != JsonValueType::Object) throw std::out_of_range("Not an object");
        const auto& obj = std::get<std::map<std::string, MyJson>>(value);
        auto it = obj.find(key);
        if (it == obj.end()) throw std::out_of_range("Key not found: " + key);
        return it->second;
    }
    MyJson& at(size_t index) {
        if (type != JsonValueType::Array) throw std::out_of_range("Not an array");
        auto& arr = std::get<std::vector<MyJson>>(value);
        if (index >= arr.size()) throw std::out_of_range("Index out of range");
        return arr[index];
    }
    const MyJson& at(size_t index) const {
        if (type != JsonValueType::Array) throw std::out_of_range("Not an array");
        const auto& arr = std::get<std::vector<MyJson>>(value);
        if (index >= arr.size()) throw std::out_of_range("Index out of range");
        return arr[index];
    }

    bool contains(const std::string& key) const {
        if (type != JsonValueType::Object) return false;
        const auto& obj = std::get<std::map<std::string, MyJson>>(value);
        return obj.find(key) != obj.end();
    }

    bool is_null() const { return type == JsonValueType::Null; }
    bool is_boolean() const { return type == JsonValueType::Bool; }
    bool is_number() const { return type == JsonValueType::Number; }
    bool is_string() const { return type == JsonValueType::String; }
    bool is_array() const { return type == JsonValueType::Array; }
    bool is_object() const { return type == JsonValueType::Object; }

    template <typename T>
    void get_to(T& out) const { out = get<T>(); }

    // 统一的 get<T> 实现
    template <typename T>
    T get() const {
        if constexpr (std::is_same_v<T, bool>) {
            if (type != JsonValueType::Bool) throw std::bad_variant_access();
            return std::get<bool>(value);
        }
        else if constexpr (std::is_same_v<T, int>) {
            if (type != JsonValueType::Number) throw std::bad_variant_access();
            return static_cast<int>(std::get<double>(value));
        }
        else if constexpr (std::is_same_v<T, float>) {
            if (type != JsonValueType::Number) throw std::bad_variant_access();
            return static_cast<float>(std::get<double>(value));
        }
        else if constexpr (std::is_same_v<T, double>) {
            if (type != JsonValueType::Number) throw std::bad_variant_access();
            return std::get<double>(value);
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            if (type != JsonValueType::String) throw std::bad_variant_access();
            return std::get<std::string>(value);
        }
        else if constexpr (std::is_same_v<T, std::vector<MyJson>>) {
            if (type != JsonValueType::Array) throw std::bad_variant_access();
            return std::get<std::vector<MyJson>>(value);
        }
        else if constexpr (std::is_same_v<T, std::map<std::string, MyJson>>) {
            if (type != JsonValueType::Object) throw std::bad_variant_access();
            return std::get<std::map<std::string, MyJson>>(value);
        }
        else {
            T obj;
            from_json(*this, obj);
            return obj;
        }
    }

    // 迭代器
    struct iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const std::string, MyJson>;
        using pointer = value_type*;
        using reference = value_type&;
        using difference_type = std::ptrdiff_t;

        iterator(std::map<std::string, MyJson>::iterator it) : it_(it) {}

        reference operator*() const { return *it_; }
        pointer operator->() const { return &(*it_); }
        iterator& operator++() { ++it_; return *this; }
        iterator operator++(int) { auto tmp = *this; ++it_; return tmp; }
        bool operator==(const iterator& other) const { return it_ == other.it_; }
        bool operator!=(const iterator& other) const { return it_ != other.it_; }

    private:
        std::map<std::string, MyJson>::iterator it_;
    };

    struct const_iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const std::string, MyJson>;
        using pointer = const value_type*;
        using reference = const value_type&;
        using difference_type = std::ptrdiff_t;

        const_iterator(std::map<std::string, MyJson>::const_iterator it) : it_(it) {}

        reference operator*() const { return *it_; }
        pointer operator->() const { return &(*it_); }
        const_iterator& operator++() { ++it_; return *this; }
        const_iterator operator++(int) { auto tmp = *this; ++it_; return tmp; }
        bool operator==(const const_iterator& other) const { return it_ == other.it_; }
        bool operator!=(const const_iterator& other) const { return it_ != other.it_; }

    private:
        std::map<std::string, MyJson>::const_iterator it_;
    };

    iterator begin() {
        if (type != JsonValueType::Object) throw std::runtime_error("Not an object");
        return iterator(std::get<std::map<std::string, MyJson>>(value).begin());
    }
    iterator end() {
        if (type != JsonValueType::Object) throw std::runtime_error("Not an object");
        return iterator(std::get<std::map<std::string, MyJson>>(value).end());
    }
    const_iterator begin() const {
        if (type != JsonValueType::Object) throw std::runtime_error("Not an object");
        return const_iterator(std::get<std::map<std::string, MyJson>>(value).begin());
    }
    const_iterator end() const {
        if (type != JsonValueType::Object) throw std::runtime_error("Not an object");
        return const_iterator(std::get<std::map<std::string, MyJson>>(value).end());
    }

    // items() 代理
    struct ItemsProxy {
        MyJson& j;
        iterator begin() { return j.begin(); }
        iterator end() { return j.end(); }
        const_iterator begin() const { return static_cast<const MyJson&>(j).begin(); }
        const_iterator end()   const { return static_cast<const MyJson&>(j).end(); }
    };
    ItemsProxy items() { return { *this }; }
    const ItemsProxy items() const { return { const_cast<MyJson&>(*this) }; }

    // 序列化
    std::string dump(int indent = -1) const {
        std::ostringstream oss;
        serialize(oss, *this, 0, indent);
        return oss.str();
    }

    friend std::istream& operator>>(std::istream& is, MyJson& j) {
        std::string content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
        j = parse(content);
        return is;
    }

    friend std::ostream& operator<<(std::ostream& os, const MyJson& j) {
        os << j.dump();
        return os;
    }

private:
    JsonValueType type;
    std::variant<std::nullptr_t, bool, double, std::string, std::vector<MyJson>, std::map<std::string, MyJson>> value;

    // 序列化辅助函数（保持不变）
    static void serialize(std::ostream& os, const MyJson& j, int depth, int indent) {
        switch (j.type) {
        case JsonValueType::Null:
            os << "null";
            break;
        case JsonValueType::Bool:
            os << (std::get<bool>(j.value) ? "true" : "false");
            break;
        case JsonValueType::Number:
            os << std::get<double>(j.value);
            break;
        case JsonValueType::String: {
            const auto& s = std::get<std::string>(j.value);
            os << "\"";
            for (char c : s) {
                switch (c) {
                case '"': os << "\\\""; break;
                case '\\': os << "\\\\"; break;
                case '\b': os << "\\b"; break;
                case '\f': os << "\\f"; break;
                case '\n': os << "\\n"; break;
                case '\r': os << "\\r"; break;
                case '\t': os << "\\t"; break;
                default: os << c; break;
                }
            }
            os << "\"";
            break;
        }
        case JsonValueType::Array: {
            const auto& arr = std::get<std::vector<MyJson>>(j.value);
            os << "[";
            if (indent >= 0 && !arr.empty()) {
                os << "\n";
                for (size_t i = 0; i < arr.size(); ++i) {
                    os << std::string((depth + 1) * indent, ' ');
                    serialize(os, arr[i], depth + 1, indent);
                    if (i != arr.size() - 1) os << ",";
                    os << "\n";
                }
                os << std::string(depth * indent, ' ');
            }
            else {
                for (size_t i = 0; i < arr.size(); ++i) {
                    serialize(os, arr[i], depth, indent);
                    if (i != arr.size() - 1) os << ",";
                }
            }
            os << "]";
            break;
        }
        case JsonValueType::Object: {
            const auto& obj = std::get<std::map<std::string, MyJson>>(j.value);
            os << "{";
            if (indent >= 0 && !obj.empty()) {
                os << "\n";
                size_t i = 0;
                for (const auto& [key, val] : obj) {
                    os << std::string((depth + 1) * indent, ' ');
                    os << "\"" << key << "\": ";
                    serialize(os, val, depth + 1, indent);
                    if (i != obj.size() - 1) os << ",";
                    os << "\n";
                    ++i;
                }
                os << std::string(depth * indent, ' ');
            }
            else {
                size_t i = 0;
                for (const auto& [key, val] : obj) {
                    os << "\"" << key << "\":";
                    serialize(os, val, depth, indent);
                    if (i != obj.size() - 1) os << ",";
                    ++i;
                }
            }
            os << "}";
            break;
        }
        }
    }

    // 解析辅助函数（保持不变）
    static MyJson parse(const std::string& s) {
        size_t pos = 0;
        skip_whitespace(s, pos);
        return parse_value(s, pos);
    }
    static void skip_whitespace(const std::string& s, size_t& pos) {
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
    }
    static MyJson parse_value(const std::string& s, size_t& pos) {
        skip_whitespace(s, pos);
        if (pos >= s.size()) throw std::runtime_error("Unexpected end of input");
        char c = s[pos];
        if (c == 'n') return parse_null(s, pos);
        if (c == 't' || c == 'f') return parse_bool(s, pos);
        if (c == '"') return parse_string(s, pos);
        if (c == '[') return parse_array(s, pos);
        if (c == '{') return parse_object(s, pos);
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '-') return parse_number(s, pos);
        throw std::runtime_error("Unexpected character: " + std::string(1, c));
    }
    static MyJson parse_null(const std::string& s, size_t& pos) {
        if (pos + 3 >= s.size() || s.substr(pos, 4) != "null") throw std::runtime_error("Invalid null value");
        pos += 4;
        return MyJson(nullptr);
    }
    static MyJson parse_bool(const std::string& s, size_t& pos) {
        if (s[pos] == 't') {
            if (pos + 3 >= s.size() || s.substr(pos, 4) != "true") throw std::runtime_error("Invalid true value");
            pos += 4;
            return MyJson(true);
        }
        else {
            if (pos + 4 >= s.size() || s.substr(pos, 5) != "false") throw std::runtime_error("Invalid false value");
            pos += 5;
            return MyJson(false);
        }
    }
    static MyJson parse_string(const std::string& s, size_t& pos) {
        ++pos;
        std::string str;
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\') {
                ++pos;
                if (pos >= s.size()) throw std::runtime_error("Unterminated escape sequence");
                switch (s[pos]) {
                case '"': str += '"'; break;
                case '\\': str += '\\'; break;
                case '/': str += '/'; break;
                case 'b': str += '\b'; break;
                case 'f': str += '\f'; break;
                case 'n': str += '\n'; break;
                case 'r': str += '\r'; break;
                case 't': str += '\t'; break;
                default: throw std::runtime_error("Invalid escape character");
                }
            }
            else {
                str += s[pos];
            }
            ++pos;
        }
        if (pos >= s.size()) throw std::runtime_error("Unterminated string");
        ++pos;
        return MyJson(str);
    }
    static MyJson parse_number(const std::string& s, size_t& pos) {
        size_t start = pos;
        if (s[pos] == '-') ++pos;
        if (s[pos] == '0') {
            ++pos;
            if (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) throw std::runtime_error("Leading zero");
        }
        else if (std::isdigit(static_cast<unsigned char>(s[pos]))) {
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        }
        else throw std::runtime_error("Invalid number");
        if (pos < s.size() && s[pos] == '.') {
            ++pos;
            if (!std::isdigit(static_cast<unsigned char>(s[pos]))) throw std::runtime_error("Invalid decimal point");
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        }
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
            ++pos;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) ++pos;
            if (!std::isdigit(static_cast<unsigned char>(s[pos]))) throw std::runtime_error("Invalid exponent");
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        }
        double num;
        auto [ptr, ec] = std::from_chars(s.data() + start, s.data() + pos, num);
        if (ec != std::errc{}) throw std::runtime_error("Failed to parse number");
        return MyJson(num);
    }
    static MyJson parse_array(const std::string& s, size_t& pos) {
        ++pos;
        std::vector<MyJson> arr;
        skip_whitespace(s, pos);
        while (pos < s.size() && s[pos] != ']') {
            arr.push_back(parse_value(s, pos));
            skip_whitespace(s, pos);
            if (pos < s.size() && s[pos] == ',') { ++pos; skip_whitespace(s, pos); }
            else if (pos < s.size() && s[pos] != ']') throw std::runtime_error("Expected comma or ]");
        }
        if (pos >= s.size()) throw std::runtime_error("Unterminated array");
        ++pos;
        return MyJson(arr);
    }
    static MyJson parse_object(const std::string& s, size_t& pos) {
        ++pos;
        std::map<std::string, MyJson> obj;
        skip_whitespace(s, pos);
        while (pos < s.size() && s[pos] != '}') {
            if (s[pos] != '"') throw std::runtime_error("Expected string key");
            MyJson keyJson = parse_string(s, pos);
            std::string key = keyJson.get<std::string>();
            skip_whitespace(s, pos);
            if (pos >= s.size() || s[pos] != ':') throw std::runtime_error("Expected colon");
            ++pos;
            skip_whitespace(s, pos);
            obj[key] = parse_value(s, pos);
            skip_whitespace(s, pos);
            if (pos < s.size() && s[pos] == ',') { ++pos; skip_whitespace(s, pos); }
            else if (pos < s.size() && s[pos] != '}') throw std::runtime_error("Expected comma or }");
        }
        if (pos >= s.size()) throw std::runtime_error("Unterminated object");
        ++pos;
        return MyJson(obj);
    }
};

// ===== 辅助序列化重载（修复版） =====

// 基础类型识别辅助（用于 if constexpr）
template <typename T>
constexpr bool is_basic_json_type_v =
std::is_same_v<T, bool> ||
std::is_same_v<T, int> ||
std::is_same_v<T, float> ||
std::is_same_v<T, double> ||
std::is_same_v<T, std::string> ||
std::is_same_v<T, const char*>;

// std::optional
template <typename T>
void to_json(MyJson& j, const std::optional<T>& opt) {
    if (opt) to_json(j, *opt);
    else j = nullptr;
}
template <typename T>
void from_json(const MyJson& j, std::optional<T>& opt) {
    if (j.is_null()) opt = std::nullopt;
    else opt = j.get<T>();
}

// std::vector
template <typename T>
void to_json(MyJson& j, const std::vector<T>& vec) {
    std::vector<MyJson> tmpVec;
    for (const auto& elem : vec) {
        MyJson elemJson;
        if constexpr (is_basic_json_type_v<T>) {
            elemJson = elem;                 // 使用已有的赋值运算符
        }
        else {
            to_json(elemJson, elem);         // 自定义类型
        }
        tmpVec.push_back(std::move(elemJson));
    }
    j = std::move(tmpVec);
}
template <typename T>
void from_json(const MyJson& j, std::vector<T>& vec) {
    vec.clear();
    const auto& arr = j.get<std::vector<MyJson>>();
    for (const auto& elemJson : arr) {
        T val;
        if constexpr (is_basic_json_type_v<T>) {
            val = elemJson.get<T>();         // 使用 get<T> 基础类型提取
        }
        else {
            from_json(elemJson, val);        // 自定义类型
        }
        vec.push_back(std::move(val));
    }
}

// std::map
template <typename K, typename V>
void to_json(MyJson& j, const std::map<K, V>& map) {
    std::map<std::string, MyJson> tmpMap;
    for (const auto& [k, v] : map) {
        MyJson valJson;
        if constexpr (is_basic_json_type_v<V>) {
            valJson = v;
        }
        else {
            to_json(valJson, v);
        }
        std::ostringstream oss;
        oss << k;
        tmpMap[oss.str()] = std::move(valJson);
    }
    j = std::move(tmpMap);
}
template <typename K, typename V>
void from_json(const MyJson& j, std::map<K, V>& map) {
    map.clear();
    const auto& obj = j.get<std::map<std::string, MyJson>>();
    for (const auto& [keyStr, valJson] : obj) {
        K key;
        std::istringstream iss(keyStr);
        iss >> key;
        V val;
        if constexpr (is_basic_json_type_v<V>) {
            val = valJson.get<V>();
        }
        else {
            from_json(valJson, val);
        }
        map.emplace(key, std::move(val));
    }
}