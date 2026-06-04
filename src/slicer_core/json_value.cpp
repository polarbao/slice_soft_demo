#include "slicer_core/json_value.h"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <istream>
#include <sstream>
#include <stdexcept>

namespace slicer_core {
namespace {

class Parser {
public:
    explicit Parser(std::string text) : text_(std::move(text)) {}

    Json parse() {
        skip_ws();
        Json value = parse_value();
        skip_ws();
        if (pos_ != text_.size()) {
            throw std::runtime_error("unexpected trailing content in JSON");
        }
        return value;
    }

private:
    Json parse_value() {
        skip_ws();
        if (pos_ >= text_.size()) {
            throw std::runtime_error("unexpected end of JSON");
        }
        const char c{text_.at(pos_)};
        if (c == '{') {
            return parse_object();
        }
        if (c == '[') {
            return parse_array();
        }
        if (c == '"') {
            return Json{parse_string()};
        }
        if (c == 't') {
            consume_literal("true");
            return Json{true};
        }
        if (c == 'f') {
            consume_literal("false");
            return Json{false};
        }
        if (c == 'n') {
            consume_literal("null");
            return Json{nullptr};
        }
        return Json{parse_number()};
    }

    Json parse_object() {
        expect('{');
        Json::Object object;
        skip_ws();
        if (peek('}')) {
            expect('}');
            return Json{object};
        }
        while (true) {
            skip_ws();
            const std::string key = parse_string();
            skip_ws();
            expect(':');
            object.emplace(key, parse_value());
            skip_ws();
            if (peek('}')) {
                expect('}');
                return Json{object};
            }
            expect(',');
        }
    }

    Json parse_array() {
        expect('[');
        Json::Array array;
        skip_ws();
        if (peek(']')) {
            expect(']');
            return Json{array};
        }
        while (true) {
            array.push_back(parse_value());
            skip_ws();
            if (peek(']')) {
                expect(']');
                return Json{array};
            }
            expect(',');
        }
    }

    std::string parse_string() {
        expect('"');
        std::string result;
        while (pos_ < text_.size()) {
            const char c{text_.at(pos_++)};
            if (c == '"') {
                return result;
            }
            if (c != '\\') {
                result.push_back(c);
                continue;
            }
            if (pos_ >= text_.size()) {
                throw std::runtime_error("unterminated JSON string escape");
            }
            const char escaped{text_.at(pos_++)};
            switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    result.push_back(escaped);
                    break;
                case 'b':
                    result.push_back('\b');
                    break;
                case 'f':
                    result.push_back('\f');
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
                default:
                    throw std::runtime_error("unsupported JSON string escape");
            }
        }
        throw std::runtime_error("unterminated JSON string");
    }

    double parse_number() {
        const std::size_t start{pos_};
        if (peek('-')) {
            ++pos_;
        }
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_.at(pos_)))) {
            ++pos_;
        }
        if (peek('.')) {
            ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_.at(pos_)))) {
                ++pos_;
            }
        }
        if (peek('e') || peek('E')) {
            ++pos_;
            if (peek('+') || peek('-')) {
                ++pos_;
            }
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_.at(pos_)))) {
                ++pos_;
            }
        }
        if (start == pos_) {
            throw std::runtime_error("expected JSON value");
        }
        return std::stod(text_.substr(start, pos_ - start));
    }

    void consume_literal(const char* literal) {
        const std::string expected{literal};
        if (text_.substr(pos_, expected.size()) != expected) {
            throw std::runtime_error("invalid JSON literal");
        }
        pos_ += expected.size();
    }

    bool peek(const char c) const {
        return pos_ < text_.size() && text_.at(pos_) == c;
    }

    void expect(const char c) {
        if (!peek(c)) {
            throw std::runtime_error(std::string{"expected JSON character: "} + c);
        }
        ++pos_;
    }

    void skip_ws() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_.at(pos_)))) {
            ++pos_;
        }
    }

    std::string text_;
    std::size_t pos_{0};
};

std::string escape_string(const std::string& value) {
    std::string result;
    result.reserve(value.size() + 2);
    for (const char c : value) {
        switch (c) {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\b':
                result += "\\b";
                break;
            case '\f':
                result += "\\f";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                result.push_back(c);
                break;
        }
    }
    return result;
}

void dump_impl(const Json& value, std::ostream& output, const int indent, const int depth) {
    const auto write_indent = [&] {
        for (int i{0}; i < depth * indent; ++i) {
            output << ' ';
        }
    };

    if (value.is_array()) {
        const auto& array = value.as_array();
        output << '[';
        if (!array.empty()) {
            output << '\n';
            for (std::size_t i{0}; i < array.size(); ++i) {
                for (int j{0}; j < (depth + 1) * indent; ++j) {
                    output << ' ';
                }
                dump_impl(array.at(i), output, indent, depth + 1);
                if (i + 1 < array.size()) {
                    output << ',';
                }
                output << '\n';
            }
            write_indent();
        }
        output << ']';
        return;
    }

    if (value.is_object()) {
        const auto& object = value.as_object();
        output << '{';
        if (!object.empty()) {
            output << '\n';
            std::size_t i{0};
            for (const auto& [key, item] : object) {
                for (int j{0}; j < (depth + 1) * indent; ++j) {
                    output << ' ';
                }
                output << '"' << escape_string(key) << "\": ";
                dump_impl(item, output, indent, depth + 1);
                if (++i < object.size()) {
                    output << ',';
                }
                output << '\n';
            }
            write_indent();
        }
        output << '}';
        return;
    }

    if (value.is_string()) {
        output << '"' << escape_string(value.as_string()) << '"';
        return;
    }
    if (value.is_bool()) {
        output << (value.as_bool() ? "true" : "false");
        return;
    }
    if (value.is_number()) {
        const double number{value.as_double()};
        if (std::floor(number) == number) {
            output << std::setprecision(0) << std::fixed << number;
        } else {
            output << std::defaultfloat << std::setprecision(15) << number;
        }
        return;
    }
    output << "null";
}

}  // namespace

Json::Json() : storage_(nullptr) {}
Json::Json(std::nullptr_t) : storage_(nullptr) {}
Json::Json(const bool value) : storage_(value) {}
Json::Json(const int value) : storage_(static_cast<double>(value)) {}
Json::Json(const std::uint64_t value) : storage_(static_cast<double>(value)) {}
Json::Json(const double value) : storage_(value) {}
Json::Json(const char* value) : storage_(std::string{value}) {}
Json::Json(std::string value) : storage_(std::move(value)) {}
Json::Json(Array value) : storage_(std::move(value)) {}
Json::Json(Object value) : storage_(std::move(value)) {}

Json Json::array(std::initializer_list<Json> values) {
    return Json{Array{values}};
}

Json Json::object(std::initializer_list<std::pair<std::string, Json>> values) {
    Object object;
    for (const auto& [key, value] : values) {
        object.emplace(key, value);
    }
    return Json{std::move(object)};
}

Json Json::parse(std::istream& input) {
    std::ostringstream stream;
    stream << input.rdbuf();
    return Parser{stream.str()}.parse();
}

bool Json::is_array() const {
    return std::holds_alternative<Array>(storage_);
}

bool Json::is_object() const {
    return std::holds_alternative<Object>(storage_);
}

bool Json::is_string() const {
    return std::holds_alternative<std::string>(storage_);
}

bool Json::is_bool() const {
    return std::holds_alternative<bool>(storage_);
}

bool Json::is_number() const {
    return std::holds_alternative<double>(storage_);
}

bool Json::contains(const std::string& key) const {
    if (!is_object()) {
        return false;
    }
    return as_object().contains(key);
}

std::size_t Json::size() const {
    if (is_array()) {
        return as_array().size();
    }
    if (is_object()) {
        return as_object().size();
    }
    throw std::runtime_error("JSON value has no size");
}

const Json& Json::at(const std::string& key) const {
    const auto& object = as_object();
    const auto found = object.find(key);
    if (found == object.end()) {
        throw std::runtime_error("missing JSON object key: " + key);
    }
    return found->second;
}

const Json& Json::at(const std::size_t index) const {
    const auto& array = as_array();
    if (index >= array.size()) {
        throw std::runtime_error("JSON array index out of range");
    }
    return array.at(index);
}

const Json::Array& Json::as_array() const {
    const auto* value = std::get_if<Array>(&storage_);
    if (value == nullptr) {
        throw std::runtime_error("JSON value is not an array");
    }
    return *value;
}

const Json::Object& Json::as_object() const {
    const auto* value = std::get_if<Object>(&storage_);
    if (value == nullptr) {
        throw std::runtime_error("JSON value is not an object");
    }
    return *value;
}

std::string Json::as_string() const {
    const auto* value = std::get_if<std::string>(&storage_);
    if (value == nullptr) {
        throw std::runtime_error("JSON value is not a string");
    }
    return *value;
}

int Json::as_int() const {
    const auto* value = std::get_if<double>(&storage_);
    if (value == nullptr) {
        throw std::runtime_error("JSON value is not a number");
    }
    return static_cast<int>(*value);
}

double Json::as_double() const {
    const auto* value = std::get_if<double>(&storage_);
    if (value == nullptr) {
        throw std::runtime_error("JSON value is not a number");
    }
    return *value;
}

bool Json::as_bool() const {
    const auto* value = std::get_if<bool>(&storage_);
    if (value == nullptr) {
        throw std::runtime_error("JSON value is not a bool");
    }
    return *value;
}

std::string Json::dump(const int indent) const {
    std::ostringstream output;
    dump_impl(*this, output, indent, 0);
    return output.str();
}

}  // namespace slicer_core
