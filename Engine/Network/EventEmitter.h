#pragma once
#include <cctype>
#include <cstdio>
#include <string>
#include <map>
#include <functional>
#include <sstream>
#include <iostream>

// Simple JSON value class
class JsonValue {
public:
    enum Type { Null, String, Number, Object };
    Type type = Null;
    std::string strVal;
    double numVal = 0.0;
    std::map<std::string, JsonValue> objVal;

    JsonValue() = default;
    JsonValue(const std::string &s) : type(String), strVal(s) {}
    JsonValue(double n) : type(Number), numVal(n) {}
    JsonValue(int n) : type(Number), numVal((double)n) {}

    static JsonValue object() {
        JsonValue v;
        v.type = Object;
        return v;
    }

    JsonValue &operator[](const std::string &key) {
        if (type != Object) type = Object;
        return objVal[key];
    }

    const JsonValue &operator[](const std::string &key) const {
        static JsonValue null;
        auto it = objVal.find(key);
        return it != objVal.end() ? it->second : null;
    }

    std::string toString() const {
        if (type == String) {
            std::string s = strVal;
            return "\"" + s + "\"";
        } else if (type == Number) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%g", numVal);
            return std::string(buf);
        } else if (type == Object) {
            std::string res = "{";
            bool first = true;
            for (const auto &kv : objVal) {
                if (!first) res += ",";
                res += "\"" + kv.first + "\":" + kv.second.toString();
                first = false;
            }
            res += "}";
            return res;
        }
        return "null";
    }

    static JsonValue parse(const std::string &json);
};

class EventEmitter {
public:
    using Callback = std::function<void(const JsonValue &)>;

    void on(const std::string &eventName, Callback cb) {
        listeners[eventName] = cb;
    }

    void emit(const std::string &eventName, const JsonValue &data = JsonValue()) {
        auto it = listeners.find(eventName);
        if (it != listeners.end()) {
            it->second(data);
        }
    }

    virtual void sendEvent(const std::string &eventName, const JsonValue &data) = 0;

    void handleEvent(const std::string &eventName, const JsonValue &data) {
        auto it = listeners.find(eventName);
        if (it != listeners.end()) {
            it->second(data);
        }
    }

    virtual ~EventEmitter() = default;

protected:
    std::map<std::string, Callback> listeners;
};

// Simple JSON parser
inline JsonValue JsonValue::parse(const std::string &json) {
    std::string s = json;
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s[0]))) s.erase(0, 1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();

    if (s.empty() || s == "null") return JsonValue();
    if (s[0] == '"') {
        size_t end = s.rfind('"');
        if (end > 0) return JsonValue(s.substr(1, end - 1));
    }
    if (s[0] == '{') {
        // object
        JsonValue obj;
        obj.type = Object;
        size_t pos = 1;
        while (pos < s.length() && s[pos] != '}') {
            // find "key"
            while (pos < s.length() && s[pos] != '"') ++pos;
            if (pos >= s.length()) break;
            size_t keyStart = pos + 1;
            pos = s.find('"', keyStart);
            if (pos == std::string::npos) break;
            std::string key = s.substr(keyStart, pos - keyStart);
            // find :
            pos = s.find(':', pos);
            if (pos == std::string::npos) break;
            ++pos;
            // find value (until , or })
            while (pos < s.length() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
            size_t valStart = pos;
            int depth = 0;
            while (pos < s.length()) {
                if (s[pos] == '{' || s[pos] == '[') depth++;
                else if (s[pos] == '}' || s[pos] == ']') depth--;
                else if ((s[pos] == ',' || s[pos] == '}') && depth == 0) break;
                ++pos;
            }
            std::string valStr = s.substr(valStart, pos - valStart);
            // trim
            while (!valStr.empty() && std::isspace(static_cast<unsigned char>(valStr[0]))) valStr.erase(0, 1);
            while (!valStr.empty() && std::isspace(static_cast<unsigned char>(valStr.back()))) valStr.pop_back();
            obj[key] = parse(valStr);
            if (pos < s.length() && s[pos] == ',') ++pos;
        }
        return obj;
    }
    // try number
    try {
        double d = std::stod(s);
        return JsonValue(d);
    } catch (...) {}
    // fallback: string
    return JsonValue(s);
}
