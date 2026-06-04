#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iosfwd>
#include <map>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace slicer_core {

class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json>;

    Json();
    Json(std::nullptr_t);
    Json(bool value);
    Json(int value);
    Json(std::uint64_t value);
    Json(double value);
    Json(const char* value);
    Json(std::string value);
    Json(Array value);
    Json(Object value);

    static Json array(std::initializer_list<Json> values);
    static Json object(std::initializer_list<std::pair<std::string, Json>> values);
    static Json parse(std::istream& input);

    bool is_array() const;
    bool is_object() const;
    bool is_string() const;
    bool is_bool() const;
    bool is_number() const;
    bool contains(const std::string& key) const;
    std::size_t size() const;

    const Json& at(const std::string& key) const;
    const Json& at(std::size_t index) const;

    const Array& as_array() const;
    const Object& as_object() const;
    std::string as_string() const;
    int as_int() const;
    double as_double() const;
    bool as_bool() const;

    template <typename T>
    T value(const std::string& key, const T& fallback) const {
        if (!contains(key)) {
            return fallback;
        }
        const Json& item = at(key);
        if constexpr (std::is_same_v<T, std::string>) {
            return item.as_string();
        } else if constexpr (std::is_same_v<T, bool>) {
            return item.as_bool();
        } else if constexpr (std::is_same_v<T, int>) {
            return item.as_int();
        } else if constexpr (std::is_same_v<T, double>) {
            return item.as_double();
        } else {
            static_assert(!sizeof(T), "unsupported Json::value type");
        }
    }

    std::string dump(int indent = 2) const;

private:
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;
    Storage storage_;
};

}  // namespace slicer_core
