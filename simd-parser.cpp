#include <iostream>
#include <vector>
#include <string>
using namespace std;

// Function to find structural character positions
vector<size_t> find_structurals(const string &json) {
    vector<size_t> structurals;
    const string chars = "{}[]:,\"";

    // Process in blocks of 16 chars (SIMD would do parallel compares)
    size_t block_size = 16;
    for (size_t i = 0; i < json.size(); i += block_size) {
        size_t end = min(i + block_size, json.size());
        
        for (size_t j = i; j < end; j++) {
            if (chars.find(json[j]) != string::npos) {
                structurals.push_back(j);
            }
        }
    }
    return structurals;
}

struct Token {
    string type;
    string value;
};

struct Node {
    string type;  // Object, Array, String, Number, True, False, Null
    string value;
    vector<pair<string, Node>> obj; // object fields
    vector<Node> arr;               // array elements
};

// string extractString(const string &json, size_t &pos) {
//     string result;
//     pos++; // skip opening "

//     while (pos < json.size()) {
//         char c = json[pos];
//         if (c == '\\') {          // escape sequence
//             if (pos + 1 < json.size()) {
//                 result += c;       // keep backslash
//                 result += json[pos + 1];
//                 pos += 2;
//                 continue;
//             }
//         } else if (c == '"') {     // end of string   // it's better to get next pos value from structurals and go till there right?
//             pos++;                  // move past closing "
//             break;
//         }
//         result += c;
//         pos++;
//     }

//     return result;
// }

// Extract string using structurals and advance i
string extractString(const string &json, size_t &pos,
                     const vector<size_t> &structurals, size_t &i) {
    size_t start = pos + 1; // after opening "
    size_t end = start;

    // Look for the next quote in structurals
    while (++i < structurals.size()) {
        size_t cand = structurals[i];
        if (json[cand] == '"' && json[cand - 1] != '\\') {
            end = cand;
            break;
        }
    }

    pos = end + 1; // move past closing "
    return json.substr(start, end - start);
}

// Extract number starting at pos
string extractNumber(const string &json, size_t &pos) {
    size_t start = pos;
    while (pos < json.size() && (isdigit(json[pos]) || json[pos]=='-' || 
           json[pos]=='+' || json[pos]=='.' || json[pos]=='e' || json[pos]=='E')) {
        pos++;
    }
    return json.substr(start, pos - start);
}

// Extract literal (true, false, null)
string extractLiteral(const string &json, size_t &pos) {
    size_t start = pos;
    while (pos < json.size() && isalpha(json[pos])) pos++;
    return json.substr(start, pos - start);
}

// vector<Token> parseJsonWithIndex(const string &json, const vector<size_t> &structurals) {
//     vector<Token> tokens;
//     bool opened = false;

//     for (size_t pos : structurals) {
//         char c = json[pos];

//         switch(c) {
//             case '{': tokens.push_back({"LeftBrace", "{"}); break;
//             case '}': tokens.push_back({"RightBrace", "}"}); break;
//             case '[': tokens.push_back({"LeftBracket", "["}); break;
//             case ']': tokens.push_back({"RightBracket", "]"}); break;
//             case ':': tokens.push_back({"Colon", ":"}); break;
//             case ',': tokens.push_back({"Comma", ","}); break;
//             case '"': {
//                 if (opened) {opened = false; break;}
//                 opened = true;
//                 string s = extractString(json, pos);
//                 tokens.push_back({"String", s});
//                 break;
//             }
//             default: {
//                 if (isdigit(c) || c=='-') {
//                     string num = extractNumber(json, pos);
//                     tokens.push_back({"Number", num});
//                 } else if (isalpha(c)) {
//                     string lit = extractLiteral(json, pos);
//                     if (lit == "true") tokens.push_back({"True", "true"});
//                     else if (lit == "false") tokens.push_back({"False", "false"});
//                     else if (lit == "null") tokens.push_back({"Null", "null"});
//                 }
//             }
//         }
//     }
//     return tokens;
// }

vector<Token> parseJsonWithIndex(const string &json, const vector<size_t> &structurals) {
    vector<Token> tokens;

    for (size_t i = 0; i < structurals.size(); i++) {
        size_t pos = structurals[i];
        char c = json[pos];

        // Handle the structural itself (as before)
        switch(c) {
            case '{': tokens.push_back({"LeftBrace","{"}); break;
            case '}': tokens.push_back({"RightBrace","}"}); break;
            case '[': tokens.push_back({"LeftBracket","["}); break;
            case ']': tokens.push_back({"RightBracket","]"}); break;
            case ':': tokens.push_back({"Colon",":"}); break;
            case ',': tokens.push_back({"Comma",","}); break;
            case '"': {
                // string s = extractString(json, pos);
                string s = extractString(json, pos, structurals, i);
                tokens.push_back({"String", s});
                break;
            }
        }

        // Now check the gap *after* this structural, until the next structural
        size_t next = (i + 1 < structurals.size()) ? structurals[i+1] : json.size();
        size_t j = pos + 1;

        while (j < next) {
            if (isspace(json[j])) { j++; continue; }

            if (isdigit(json[j]) || json[j]=='-') {
                string num = extractNumber(json, j);
                tokens.push_back({"Number", num});
            } else if (isalpha(json[j])) {
                string lit = extractLiteral(json, j);
                if (lit=="true") tokens.push_back({"True","true"});
                else if (lit=="false") tokens.push_back({"False","false"});
                else if (lit=="null") tokens.push_back({"Null","null"});
            } else {
                j++; // unexpected, but skip safely
            }
        }
    }
    return tokens;
}

struct Parser {
    vector<Token> tokens;
    size_t idx = 0;

    Token peek() { return tokens[idx]; }
    Token get() { return tokens[idx++]; }
    bool hasNext() { return idx < tokens.size(); }

    Node parseValue() {
        Token t = peek();
        if (t.type == "String") { get(); return {"String", t.value}; }
        if (t.type == "Number") { get(); return {"Number", t.value}; }
        if (t.type == "True")   { get(); return {"True", "true"}; }
        if (t.type == "False")  { get(); return {"False", "false"}; }
        if (t.type == "Null")   { get(); return {"Null", "null"}; }
        if (t.type == "LeftBrace") return parseObject();
        if (t.type == "LeftBracket") return parseArray();
        throw runtime_error("Unexpected token: " + t.type);
    }

    Node parseObject() {
        get(); // consume '{'
        Node n; n.type = "Object";

        while (peek().type != "RightBrace") {
            Token key = get(); // must be String
            get(); // consume ':'
            Node value = parseValue();
            n.obj.push_back({key.value, value});
            if (peek().type == "Comma") get();
        }
        get(); // consume '}'
        return n;
    }

    Node parseArray() {
        get(); // consume '['
        Node n; n.type = "Array";

        while (peek().type != "RightBracket") {
            Node elem = parseValue();
            n.arr.push_back(elem);
            if (peek().type == "Comma") get();
        }
        get(); // consume ']'
        return n;
    }
};

void printNode(const Node &n, int indent=0) {
    string pad(indent, ' ');
    if (n.type == "Object") {
        cout << "{\n";
        for (size_t i = 0; i < n.obj.size(); i++) {
            auto &kv = n.obj[i];
            cout << pad << "  \"" << kv.first << "\": ";
            printNode(kv.second, indent + 2);
            if (i + 1 < n.obj.size()) cout << ",";
            cout << "\n";
        }
        cout << pad << "}";
    } 
    else if (n.type == "Array") {
        cout << "[\n";
        for (size_t i = 0; i < n.arr.size(); i++) {
            cout << pad << "  ";
            printNode(n.arr[i], indent + 2);
            if (i + 1 < n.arr.size()) cout << ",";
            cout << "\n";
        }
        cout << pad << "]";
    } 
    else {
        // primitives
        if (n.type == "String") cout << "\"" << n.value << "\"";
        else cout << n.value; // Number, True, False, Null
    }
}

int main() {
    string json = R"({
        "name": "Alice",
        "age": 30,
        "married": false,
        "score": 99.5,
        "children": null,
        "hobbies": ["reading", "coding", "music"]
    })";

    auto structurals = find_structurals(json);
    auto tokens = parseJsonWithIndex(json, structurals);

    Parser p{tokens};
    Node root = p.parseValue();

    cout << "Parsed JSON Tree:\n";
    printNode(root);

    return 0;
}