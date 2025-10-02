#include <iostream>
#include <stdlib.h>
#include <variant>
#include <vector>
#include <string>
#include <unordered_map>
using namespace std;

enum class TokenType {
    LeftBrace,   // {
    RightBrace,  // }
    LeftBracket, // [
    RightBracket,// ]
    Colon,       // :
    Comma,       // ,
    String,      // "..."
    Number,      // 123, -4.56, 1e10
    True,        // true
    False,       // false
    Null,        // null
    EndOfFile,   // end of input
    Invalid      // error case
};

struct Token {
    TokenType type;
    string value; // only meaningful for String, Number, literals
};

class Tokenizer {
private:
    const string& input;
    size_t pos = 0;

    char peek() const { return (pos < input.size()) ? input[pos] : '\0'; }

    char get() { return (pos < input.size()) ? input[pos++] : '\0'; }

    void skipWhitespace() { while (isspace(peek())) get(); }

public:
    Tokenizer(const string& text) : input(text) {}

    Token nextToken() {
        skipWhitespace();

        char c = peek();
        if (c == '\0') return {TokenType::EndOfFile, ""};
        
        // Single-character tokens
        switch (c) {
            case '{': get(); return {TokenType::LeftBrace, "{"};
            case '}': get(); return {TokenType::RightBrace, "}"};
            case '[': get(); return {TokenType::LeftBracket, "["};
            case ']': get(); return {TokenType::RightBracket, "]"};
            case ':': get(); return {TokenType::Colon, ":"};
            case ',': get(); return {TokenType::Comma, ","};
        }

        // Keywords: true, false, null
        if (isalpha(c)) {
            string word;
            while (isalpha(peek())) {
                word += get();
            }
            if (word == "true")  return {TokenType::True, word};
            if (word == "false") return {TokenType::False, word};
            if (word == "null")  return {TokenType::Null, word};
            return {TokenType::Invalid, word};
        }

        // Numbers
        if (c == '-' || isdigit(c)) {
            return parseNumber();
        }

        // Strings
        if (c == '"') {
            Token rawTok = parseStringRaw();
            if (rawTok.type == TokenType::Invalid) return rawTok;

            bool ok;
            string unescaped = unescapeString(rawTok.value, ok);
            if (!ok) return {TokenType::Invalid, rawTok.value};
            return {TokenType::String, unescaped};
        }

        return {TokenType::Invalid, string(1, get())};
    }

    Token parseNumber() {
        string num;

        // Optional minus
        if (peek() == '-') {
            num += get();
        }

        // Digits
        if (isdigit(peek())) {
            if (peek() == '0') {
                num += get(); // leading zero allowed only if single
            } else {
                while (isdigit(peek())) {
                    num += get();
                }
            }
        } else {
            return {TokenType::Invalid, num};
        }

        // Fraction
        if (peek() == '.') {
            num += get();
            if (!isdigit(peek())) {
                return {TokenType::Invalid, num}; // must have digit after '.'
            }
            while (isdigit(peek())) {
                num += get();
            }
        }

        // Exponent
        if (peek() == 'e' || peek() == 'E') {
            num += get();
            if (peek() == '+' || peek() == '-') {
                num += get();
            }
            if (!isdigit(peek())) {
                return {TokenType::Invalid, num}; // must have digit after 'e'
            }
            while (isdigit(peek())) {
                num += get();
            }
        }

        return {TokenType::Number, num};
    }

    Token parseStringRaw() {
        // precondition: peek() == '"'
        get(); // consume opening quote
    
        string raw;
        while (true) {
            char c = get();
            if (c == '\0') {
                // unterminated string
                return {TokenType::Invalid, raw};
            }
            if (c == '"') {
                // closing quote — done
                break;
            }
            if (c == '\\') {
                // preserve escape sequence for now: store backslash + next char (if any)
                char next = get();
                if (next == '\0') {
                    // backslash at end -> invalid
                    return {TokenType::Invalid, raw};
                }
                raw.push_back('\\');
                raw.push_back(next);
                continue;
            }
            // normal character
            raw.push_back(c);
        }
    
        return {TokenType::String, raw}; // raw contains escapes as two-character sequences
    }

    string unescapeString(const string& raw, bool& success) {
        string result;
        success = true;

        for (size_t i = 0; i < raw.size(); ++i) {
            char c = raw[i];
            if (c == '\\') {
                if (i + 1 >= raw.size()) {
                    success = false; // backslash at end
                    break;
                }
                char next = raw[++i];
                switch (next) {
                    case '\\': result.push_back('\\'); break;
                    case '"':  result.push_back('"'); break;
                    case 'n':  result.push_back('\n'); break;
                    case 't':  result.push_back('\t'); break;
                    case 'r':  result.push_back('\r'); break;
                    case 'b':  result.push_back('\b'); break;
                    case 'f':  result.push_back('\f'); break;
                    default:
                        success = false; // invalid escape
                        break;
                }
                if (!success) break;
            } else {
                result.push_back(c);
            }
        }

        return result;
    }
};

struct JsonValue;

using JsonObject = unordered_map<string, JsonValue>;
using JsonArray  = vector<JsonValue>;
using JsonLiteral = variant<nullptr_t, bool, double, string, JsonObject, JsonArray>;

struct JsonValue {
    JsonLiteral value;
};

class Parser {
private:
    Tokenizer& tokenizer;
    Token current;

    void advance() { current = tokenizer.nextToken(); }

public:
    Parser(Tokenizer& t) : tokenizer(t) { advance(); } // load first token

    JsonValue parseValue() {
        switch (current.type) {
            case TokenType::String: {
                JsonValue val{current.value};
                advance();
                return val;
            }
            case TokenType::Number: {
                double num = stod(current.value);
                JsonValue val{num};
                advance();
                return val;
            }
            case TokenType::True: {
                JsonValue val{true};
                advance();
                return val;
            }
            case TokenType::False: {
                JsonValue val{false};
                advance();
                return val;
            }
            case TokenType::Null: {
                JsonValue val{nullptr};
                advance();
                return val;
            }
            case TokenType::LeftBrace:
                return JsonValue{parseObject()};
            case TokenType::LeftBracket:
                return JsonValue{parseArray()};
            default:
                throw runtime_error("Unexpected token in parseValue");
        }
    }

    JsonObject parseObject() {
        JsonObject obj;
        advance(); // skip '{'

        if (current.type == TokenType::RightBrace) {
            advance(); // empty object
            return obj;
        }

        while (true) {
            if (current.type != TokenType::String)
                throw runtime_error("Expected string key in object");

            string key = current.value;
            advance();

            if (current.type != TokenType::Colon)
                throw runtime_error("Expected ':' after key");
            advance();

            obj[key] = parseValue();

            if (current.type == TokenType::Comma) {
                advance();
                continue;
            } else if (current.type == TokenType::RightBrace) {
                advance();
                break;
            } else {
                throw runtime_error("Expected ',' or '}' in object");
            }
        }
        return obj;
    }

    JsonArray parseArray() {
        JsonArray arr;
        advance(); // skip '['
    
        if (current.type == TokenType::RightBracket) {
            advance(); // empty array
            return arr;
        }
    
        while (true) {
            arr.push_back(parseValue());
        
            if (current.type == TokenType::Comma) {
                advance();
                continue;
            } else if (current.type == TokenType::RightBracket) {
                advance();
                break;
            } else {
                throw runtime_error("Expected ',' or ']' in array");
            }
        }
        return arr;
    }
};

void printJson(const JsonValue& value, int indent = 0) {
    string ind(indent, ' '); // indentation for readability

    if (holds_alternative<nullptr_t>(value.value)) {
        cout << "null";
    } 
    else if (holds_alternative<bool>(value.value)) {
        cout << (get<bool>(value.value) ? "true" : "false");
    } 
    else if (holds_alternative<double>(value.value)) {
        cout << get<double>(value.value);
    } 
    else if (holds_alternative<string>(value.value)) {
        cout << '"' << get<string>(value.value) << '"';
    } 
    else if (holds_alternative<JsonObject>(value.value)) {
        const auto& obj = get<JsonObject>(value.value);
        cout << "{\n";
        bool first = true;
        for (const auto& [key, val] : obj) {
            if (!first) cout << ",\n";
            first = false;
            cout << ind << "  \"" << key << "\": ";
            printJson(val, indent + 2);
        }
        cout << "\n" << ind << "}";
    } 
    else if (holds_alternative<JsonArray>(value.value)) {
        const auto& arr = get<JsonArray>(value.value);
        cout << "[\n";
        for (size_t i = 0; i < arr.size(); ++i) {
            cout << ind << "  ";
            printJson(arr[i], indent + 2);
            if (i != arr.size() - 1) cout << ",";
            cout << "\n";
        }
        cout << ind << "]";
    }
}

int main() {
    string text = R"({
        "person": {
            "name": "Alice",
            "details": {
                "age": 30,
                "skills": ["C++", "Python"],
                "active": true
            }
        }
    })";
    Tokenizer tokenizer(text);
    Parser parser(tokenizer);

    JsonValue root = parser.parseValue();
    cout << "Parsed JSON successfully!\n\n";
    printJson(root);
    cout << "\n";

    return 0;
}
