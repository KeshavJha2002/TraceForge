#include <cmath>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

class JsonValue {
public:
  using Object = std::map<std::string, JsonValue>;
  using Array = std::vector<JsonValue>;
  using Storage = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

  JsonValue() : storage_(nullptr) {}
  JsonValue(std::nullptr_t) : storage_(nullptr) {}
  JsonValue(bool value) : storage_(value) {}
  JsonValue(double value) : storage_(value) {}
  JsonValue(int value) : storage_(static_cast<double>(value)) {}
  JsonValue(const char *value) : storage_(std::string(value)) {}
  JsonValue(std::string value) : storage_(std::move(value)) {}
  JsonValue(Array value) : storage_(std::move(value)) {}
  JsonValue(Object value) : storage_(std::move(value)) {}

  bool isNull() const { return std::holds_alternative<std::nullptr_t>(storage_); }
  bool isBool() const { return std::holds_alternative<bool>(storage_); }
  bool isNumber() const { return std::holds_alternative<double>(storage_); }
  bool isString() const { return std::holds_alternative<std::string>(storage_); }
  bool isArray() const { return std::holds_alternative<Array>(storage_); }
  bool isObject() const { return std::holds_alternative<Object>(storage_); }

  bool asBool() const { return std::get<bool>(storage_); }
  double asNumber() const { return std::get<double>(storage_); }
  const std::string &asString() const { return std::get<std::string>(storage_); }
  const Array &asArray() const { return std::get<Array>(storage_); }
  const Object &asObject() const { return std::get<Object>(storage_); }
  Array &asArray() { return std::get<Array>(storage_); }
  Object &asObject() { return std::get<Object>(storage_); }

  const JsonValue *find(const std::string &key) const {
    if (!isObject()) {
      return nullptr;
    }
    const auto &object = asObject();
    auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
  }

  const JsonValue *atIndex(std::size_t index) const {
    if (!isArray()) {
      return nullptr;
    }
    const auto &array = asArray();
    return index < array.size() ? &array[index] : nullptr;
  }

private:
  Storage storage_;
};

class JsonParser {
public:
  explicit JsonParser(std::string input) : input_(std::move(input)) {}

  JsonValue parse() {
    skipWhitespace();
    JsonValue value = parseValue();
    skipWhitespace();
    if (position_ != input_.size()) {
      throw std::runtime_error("Unexpected trailing characters in JSON input");
    }
    return value;
  }

private:
  JsonValue parseValue() {
    skipWhitespace();
    ensure(position_ < input_.size(), "Unexpected end of input while parsing JSON value");
    char ch = input_[position_];
    if (ch == '{') {
      return parseObject();
    }
    if (ch == '[') {
      return parseArray();
    }
    if (ch == '"') {
      return JsonValue(parseString());
    }
    if (ch == 't') {
      consumeLiteral("true");
      return JsonValue(true);
    }
    if (ch == 'f') {
      consumeLiteral("false");
      return JsonValue(false);
    }
    if (ch == 'n') {
      consumeLiteral("null");
      return JsonValue(nullptr);
    }
    if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
      return JsonValue(parseNumber());
    }
    throw std::runtime_error("Unsupported JSON token encountered");
  }

  JsonValue parseObject() {
    expect('{');
    JsonValue::Object object;
    skipWhitespace();
    if (tryConsume('}')) {
      return JsonValue(object);
    }

    while (true) {
      skipWhitespace();
      ensure(position_ < input_.size() && input_[position_] == '"', "Expected object key");
      std::string key = parseString();
      skipWhitespace();
      expect(':');
      skipWhitespace();
      object.emplace(std::move(key), parseValue());
      skipWhitespace();
      if (tryConsume('}')) {
        break;
      }
      expect(',');
    }
    return JsonValue(object);
  }

  JsonValue parseArray() {
    expect('[');
    JsonValue::Array array;
    skipWhitespace();
    if (tryConsume(']')) {
      return JsonValue(array);
    }

    while (true) {
      skipWhitespace();
      array.push_back(parseValue());
      skipWhitespace();
      if (tryConsume(']')) {
        break;
      }
      expect(',');
    }
    return JsonValue(array);
  }

  std::string parseString() {
    expect('"');
    std::string result;
    while (position_ < input_.size()) {
      char ch = input_[position_++];
      if (ch == '"') {
        return result;
      }
      if (ch == '\\') {
        ensure(position_ < input_.size(), "Invalid escape sequence in JSON string");
        char escape = input_[position_++];
        switch (escape) {
        case '"': result.push_back('"'); break;
        case '\\': result.push_back('\\'); break;
        case '/': result.push_back('/'); break;
        case 'b': result.push_back('\b'); break;
        case 'f': result.push_back('\f'); break;
        case 'n': result.push_back('\n'); break;
        case 'r': result.push_back('\r'); break;
        case 't': result.push_back('\t'); break;
        case 'u': {
          ensure(position_ + 4 <= input_.size(), "Invalid unicode escape in JSON string");
          std::string hex = input_.substr(position_, 4);
          position_ += 4;
          unsigned int code = static_cast<unsigned int>(std::stoul(hex, nullptr, 16));
          if (code <= 0x7F) {
            result.push_back(static_cast<char>(code));
          } else {
            result.push_back('?');
          }
          break;
        }
        default:
          throw std::runtime_error("Unsupported JSON escape sequence");
        }
      } else {
        result.push_back(ch);
      }
    }
    throw std::runtime_error("Unterminated JSON string");
  }

  double parseNumber() {
    std::size_t start = position_;
    if (input_[position_] == '-') {
      ++position_;
    }
    while (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) {
      ++position_;
    }
    if (position_ < input_.size() && input_[position_] == '.') {
      ++position_;
      while (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) {
        ++position_;
      }
    }
    if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
      ++position_;
      if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) {
        ++position_;
      }
      while (position_ < input_.size() && std::isdigit(static_cast<unsigned char>(input_[position_]))) {
        ++position_;
      }
    }
    return std::stod(input_.substr(start, position_ - start));
  }

  void consumeLiteral(const std::string &literal) {
    ensure(input_.compare(position_, literal.size(), literal) == 0, "Unexpected JSON literal");
    position_ += literal.size();
  }

  bool tryConsume(char ch) {
    if (position_ < input_.size() && input_[position_] == ch) {
      ++position_;
      return true;
    }
    return false;
  }

  void expect(char ch) {
    ensure(position_ < input_.size() && input_[position_] == ch, "Unexpected JSON structure");
    ++position_;
  }

  void skipWhitespace() {
    while (position_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[position_]))) {
      ++position_;
    }
  }

  static void ensure(bool condition, const std::string &message) {
    if (!condition) {
      throw std::runtime_error(message);
    }
  }

  std::string input_;
  std::size_t position_ = 0;
};

class JsonWriter {
public:
  static std::string write(const JsonValue &value) {
    std::ostringstream output;
    writeValue(output, value, 0);
    return output.str();
  }

private:
  static void writeIndent(std::ostringstream &output, int depth) {
    for (int index = 0; index < depth; ++index) {
      output << "  ";
    }
  }

  static void writeString(std::ostringstream &output, const std::string &value) {
    output << '"';
    for (char ch : value) {
      switch (ch) {
      case '"': output << "\\\""; break;
      case '\\': output << "\\\\"; break;
      case '\b': output << "\\b"; break;
      case '\f': output << "\\f"; break;
      case '\n': output << "\\n"; break;
      case '\r': output << "\\r"; break;
      case '\t': output << "\\t"; break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          output << "?";
        } else {
          output << ch;
        }
        break;
      }
    }
    output << '"';
  }

  static void writeValue(std::ostringstream &output, const JsonValue &value, int depth) {
    if (value.isNull()) {
      output << "null";
      return;
    }
    if (value.isBool()) {
      output << (value.asBool() ? "true" : "false");
      return;
    }
    if (value.isNumber()) {
      double number = value.asNumber();
      if (std::abs(number - static_cast<long long>(number)) < 1e-9) {
        output << static_cast<long long>(number);
      } else {
        output << std::setprecision(15) << number;
      }
      return;
    }
    if (value.isString()) {
      writeString(output, value.asString());
      return;
    }
    if (value.isArray()) {
      output << "[";
      const auto &array = value.asArray();
      if (!array.empty()) {
        output << "\n";
        for (std::size_t index = 0; index < array.size(); ++index) {
          writeIndent(output, depth + 1);
          writeValue(output, array[index], depth + 1);
          if (index + 1 != array.size()) {
            output << ",";
          }
          output << "\n";
        }
        writeIndent(output, depth);
      }
      output << "]";
      return;
    }
    output << "{";
    const auto &object = value.asObject();
    if (!object.empty()) {
      output << "\n";
      std::size_t index = 0;
      for (const auto &entry : object) {
        writeIndent(output, depth + 1);
        writeString(output, entry.first);
        output << ": ";
        writeValue(output, entry.second, depth + 1);
        if (++index != object.size()) {
          output << ",";
        }
        output << "\n";
      }
      writeIndent(output, depth);
    }
    output << "}";
  }
};

struct Location {
  int line = 0;
  int col = 0;
  int offset = 0;
  bool valid = false;
};

class Normalizer {
public:
  explicit Normalizer(const JsonValue &rawRoot) : rawRoot_(rawRoot) {}

  JsonValue normalize() {
    ensure(rawRoot_.isObject(), "Raw AST root must be a JSON object");

    JsonValue::Object result;
    result["version"] = JsonValue("1.0");
    result["language"] = JsonValue("cpp");
    result["entrypoints"] = JsonValue(JsonValue::Array{});
    result["files"] = JsonValue(JsonValue::Array{});
    result["rootNodeIds"] = JsonValue(JsonValue::Array{});
    result["scopes"] = JsonValue(JsonValue::Object{});
    result["symbols"] = JsonValue(JsonValue::Object{});
    result["nodes"] = JsonValue(JsonValue::Object{});

    std::string fileId = ensureFile(filePathForNode(rawRoot_));
    std::string rootScopeId = makeScopeId("translation", rawId(rawRoot_));
    writeScope(rootScopeId, "translationUnit", "", nodeId(rawRoot_), {});

    auto rootNode = JsonValue::Object{};
    rootNode["id"] = nodeId(rawRoot_);
    rootNode["kind"] = JsonValue("TranslationUnit");
    rootNode["parentId"] = JsonValue(nullptr);
    rootNode["childIds"] = JsonValue(JsonValue::Array{});
    rootNode["fileId"] = JsonValue(fileId);
    rootNode["scopeId"] = JsonValue(rootScopeId);
    appendLocation(rootNode, rawRoot_);
    appendTraceHints(rootNode, false, false, false, false, false);
    nodes_[nodeId(rawRoot_)] = JsonValue(rootNode);
    rootNodeIds_.push_back(nodeId(rawRoot_));

    for (const auto &child : rawChildren(rawRoot_)) {
      std::optional<std::string> childId = normalizeNode(child, nodeId(rawRoot_), rootScopeId);
      if (childId.has_value()) {
        childIds_[nodeId(rawRoot_)].push_back(*childId);
      }
    }

    finalizeChildIds();
    flush(result);
    return JsonValue(result);
  }

private:
  std::optional<std::string> normalizeNode(const JsonValue &rawNode, const std::string &parentId, const std::string &scopeId) {
    if (!rawNode.isObject()) {
      return std::nullopt;
    }

    if (shouldSkipNode(rawNode)) {
      return std::nullopt;
    }

    std::string rawKindValue = rawKind(rawNode);
    if (rawKindValue == "ImplicitCastExpr" || rawKindValue == "ParenExpr") {
      for (const auto &child : rawChildren(rawNode)) {
        std::optional<std::string> collapsed = normalizeNode(child, parentId, scopeId);
        if (collapsed.has_value()) {
          return collapsed;
        }
      }
      return std::nullopt;
    }

    std::string normalizedKind = mapKind(rawKindValue);
    std::string normalizedNodeId = nodeId(rawNode);
    std::string fileId = ensureFile(filePathForNode(rawNode));
    std::string nodeScopeId = scopeId;

    if (normalizedKind == "FunctionDecl") {
      nodeScopeId = makeScopeId("function", rawId(rawNode));
      writeScope(nodeScopeId, "function", scopeId, normalizedNodeId, {});
    } else if (normalizedKind == "CompoundStmt") {
      nodeScopeId = makeScopeId("compound", rawId(rawNode));
      writeScope(nodeScopeId, "compound", scopeId, normalizedNodeId, {});
    } else if (normalizedKind == "IfStmt") {
      nodeScopeId = makeScopeId("branch", rawId(rawNode));
      writeScope(nodeScopeId, "branch", scopeId, normalizedNodeId, {});
    } else if (normalizedKind == "ForStmt" || normalizedKind == "WhileStmt" || normalizedKind == "DoStmt") {
      nodeScopeId = makeScopeId("loop", rawId(rawNode));
      writeScope(nodeScopeId, "loop", scopeId, normalizedNodeId, {});
    }

    JsonValue::Object node;
    node["id"] = JsonValue(normalizedNodeId);
    node["kind"] = JsonValue(normalizedKind);
    node["parentId"] = JsonValue(parentId);
    node["childIds"] = JsonValue(JsonValue::Array{});
    node["fileId"] = JsonValue(fileId);
    node["scopeId"] = JsonValue(nodeScopeId);
    appendLocation(node, rawNode);
    appendNodeFields(node, rawNode, normalizedKind, scopeId, nodeScopeId);
    appendTraceHints(
        node,
        normalizedKind == "VarDecl" || normalizedKind == "ParamDecl",
        isMutationNode(node),
        normalizedKind == "IfStmt" || normalizedKind == "ForStmt" || normalizedKind == "WhileStmt" || normalizedKind == "DoStmt",
        normalizedKind == "ForStmt" || normalizedKind == "WhileStmt" || normalizedKind == "DoStmt",
        normalizedKind == "ReturnStmt");

    nodes_[normalizedNodeId] = JsonValue(node);

    std::string childrenScopeId = nodeScopeId;
    if (normalizedKind == "VarDecl" || normalizedKind == "ParamDecl" || normalizedKind == "BinaryExpr" || normalizedKind == "UnaryExpr" || normalizedKind == "CallExpr" || normalizedKind == "ReturnStmt" || normalizedKind == "DeclStmt") {
      childrenScopeId = scopeId;
    }

    for (const auto &child : rawChildren(rawNode)) {
      std::optional<std::string> childId = normalizeNode(child, normalizedNodeId, childrenScopeId);
      if (childId.has_value()) {
        childIds_[normalizedNodeId].push_back(*childId);
      }
    }

    appendDerivedChildFields(rawNode, normalizedKind, normalizedNodeId);
    return normalizedNodeId;
  }

  void appendNodeFields(JsonValue::Object &node, const JsonValue &rawNode, const std::string &normalizedKind,
                        const std::string &lexicalScopeId, const std::string &nodeScopeId) {
    if (normalizedKind == "FunctionDecl") {
      std::string name = stringField(rawNode, "name");
      node["name"] = JsonValue(name);
      node["qualifiedName"] = JsonValue(name);
      node["type"] = typeInfo(typeText(rawNode));
      node["returnType"] = typeInfo(extractReturnType(typeText(rawNode)));
      node["paramIds"] = JsonValue(JsonValue::Array{});
      node["bodyId"] = JsonValue(nullptr);
      std::string symbolId = makeSymbolId("function", name, rawId(rawNode));
      node["symbolId"] = JsonValue(symbolId);
      writeSymbol(symbolId, name, "function", name, typeText(rawNode), nodeId(rawNode), lexicalScopeId, "global");
      entrypoints_.push_back(name);
      addSymbolToScope(nodeScopeId, symbolId);
      return;
    }

    if (normalizedKind == "ParamDecl" || normalizedKind == "VarDecl") {
      std::string name = stringField(rawNode, "name");
      std::string storage = normalizedKind == "ParamDecl" ? "param" : (lexicalScopeId == translationScopeId() ? "global" : "local");
      node["name"] = JsonValue(name);
      node["type"] = typeInfo(typeText(rawNode));
      node["storage"] = JsonValue(storage);
      node["initId"] = JsonValue(nullptr);
      std::string symbolId = makeSymbolId(normalizedKind == "ParamDecl" ? "param" : "variable", name, rawId(rawNode));
      node["symbolId"] = JsonValue(symbolId);
      writeSymbol(symbolId, name, normalizedKind == "ParamDecl" ? "param" : "variable", name, typeText(rawNode), nodeId(rawNode), lexicalScopeId, storage);
      addSymbolToScope(lexicalScopeId, symbolId);
      return;
    }

    if (normalizedKind == "BinaryExpr") {
      std::string op = stringField(rawNode, "opcode");
      node["op"] = JsonValue(op);
      node["category"] = JsonValue(classifyBinaryOp(op));
      node["lhsId"] = JsonValue(nullptr);
      node["rhsId"] = JsonValue(nullptr);
      return;
    }

    if (normalizedKind == "UnaryExpr") {
      std::string op = stringField(rawNode, "opcode");
      node["op"] = JsonValue(op);
      node["category"] = JsonValue(classifyUnaryOp(op));
      node["operandId"] = JsonValue(nullptr);
      node["isPostfix"] = JsonValue(boolField(rawNode, "isPostfix"));
      return;
    }

    if (normalizedKind == "CallExpr") {
      node["calleeId"] = JsonValue(nullptr);
      node["calleeName"] = JsonValue(lookupCalleeName(rawNode));
      node["argIds"] = JsonValue(JsonValue::Array{});
      return;
    }

    if (normalizedKind == "Identifier") {
      std::string name = referencedName(rawNode);
      node["name"] = JsonValue(name);
      std::string referencedId = referencedSymbolId(rawNode);
      if (!referencedId.empty()) {
        node["referencedSymbolId"] = JsonValue(referencedId);
      }
      return;
    }

    if (normalizedKind == "Literal") {
      std::string literalKind = classifyLiteralKind(rawNode);
      node["literalKind"] = JsonValue(literalKind);
      if (rawNode.find("value") != nullptr) {
        const JsonValue *value = rawNode.find("value");
        if (value->isString() && literalKind == "int") {
          try {
            node["value"] = JsonValue(static_cast<double>(std::stoll(value->asString())));
          } catch (...) {
            node["value"] = JsonValue(value->asString());
          }
        } else if (value->isString() && literalKind == "bool") {
          node["value"] = JsonValue(value->asString() == "true" || value->asString() == "1");
        } else if (value->isString()) {
          node["value"] = JsonValue(value->asString());
        } else {
          node["value"] = *value;
        }
      }
      return;
    }
  }

  void appendDerivedChildFields(const JsonValue &rawNode, const std::string &normalizedKind, const std::string &normalizedNodeId) {
    auto it = childIds_.find(normalizedNodeId);
    if (it == childIds_.end()) {
      return;
    }
    auto &children = it->second;
    JsonValue::Object &node = nodes_.at(normalizedNodeId).asObject();

    if (normalizedKind == "FunctionDecl") {
      JsonValue::Array paramIds;
      for (const auto &childId : children) {
        const auto &childNode = nodes_.at(childId).asObject();
        std::string childKind = childNode.at("kind").asString();
        if (childKind == "ParamDecl") {
          paramIds.push_back(JsonValue(childId));
        } else if (childKind == "CompoundStmt") {
          node["bodyId"] = JsonValue(childId);
        }
      }
      node["paramIds"] = JsonValue(paramIds);
      return;
    }

    if (normalizedKind == "VarDecl" || normalizedKind == "ParamDecl") {
      if (!children.empty()) {
        node["initId"] = JsonValue(children.front());
      }
      return;
    }

    if (normalizedKind == "BinaryExpr") {
      if (!children.empty()) {
        node["lhsId"] = JsonValue(children[0]);
      }
      if (children.size() > 1) {
        node["rhsId"] = JsonValue(children[1]);
      }
      return;
    }

    if (normalizedKind == "UnaryExpr") {
      if (!children.empty()) {
        node["operandId"] = JsonValue(children[0]);
      }
      return;
    }

    if (normalizedKind == "CallExpr") {
      if (!children.empty()) {
        node["calleeId"] = JsonValue(children[0]);
      }
      JsonValue::Array args;
      for (std::size_t index = 1; index < children.size(); ++index) {
        args.push_back(JsonValue(children[index]));
      }
      node["argIds"] = JsonValue(args);
      return;
    }
  }

  bool shouldSkipNode(const JsonValue &rawNode) const {
    std::string kind = rawKind(rawNode);
    if (kind.empty()) {
      return true;
    }

    if ((kind == "TypedefDecl" || kind == "BuiltinType" || kind == "PointerType" || kind == "RecordType" || kind == "ConstantArrayType") && boolField(rawNode, "isImplicit")) {
      return true;
    }

    if (kind == "TypedefDecl" || kind == "BuiltinType" || kind == "PointerType" || kind == "RecordType" || kind == "ConstantArrayType") {
      return true;
    }

    return false;
  }

  static std::string mapKind(const std::string &rawKindValue) {
    if (rawKindValue == "TranslationUnitDecl") return "TranslationUnit";
    if (rawKindValue == "FunctionDecl") return "FunctionDecl";
    if (rawKindValue == "ParmVarDecl") return "ParamDecl";
    if (rawKindValue == "VarDecl") return "VarDecl";
    if (rawKindValue == "CompoundStmt") return "CompoundStmt";
    if (rawKindValue == "IfStmt") return "IfStmt";
    if (rawKindValue == "ForStmt") return "ForStmt";
    if (rawKindValue == "WhileStmt") return "WhileStmt";
    if (rawKindValue == "DoStmt") return "DoStmt";
    if (rawKindValue == "ReturnStmt") return "ReturnStmt";
    if (rawKindValue == "DeclStmt") return "DeclStmt";
    if (rawKindValue == "BinaryOperator") return "BinaryExpr";
    if (rawKindValue == "UnaryOperator") return "UnaryExpr";
    if (rawKindValue == "CallExpr") return "CallExpr";
    if (rawKindValue == "DeclRefExpr") return "Identifier";
    if (rawKindValue == "IntegerLiteral" || rawKindValue == "FloatingLiteral" || rawKindValue == "StringLiteral" || rawKindValue == "CharacterLiteral" || rawKindValue == "CXXBoolLiteralExpr") return "Literal";
    return "Unknown";
  }

  static std::string classifyBinaryOp(const std::string &op) {
    if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=") return "assignment";
    if (op == "<" || op == ">" || op == "<=" || op == ">=" || op == "==" || op == "!=") return "comparison";
    if (op == "&&" || op == "||") return "logical";
    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") return "arithmetic";
    return "other";
  }

  static std::string classifyUnaryOp(const std::string &op) {
    if (op == "++") return "increment";
    if (op == "--") return "decrement";
    if (op == "&") return "addressOf";
    if (op == "*") return "deref";
    if (op == "-") return "negation";
    return "other";
  }

  static std::string classifyLiteralKind(const JsonValue &rawNode) {
    std::string kind = rawKind(rawNode);
    if (kind == "IntegerLiteral") return "int";
    if (kind == "FloatingLiteral") return "float";
    if (kind == "CXXBoolLiteralExpr") return "bool";
    if (kind == "CharacterLiteral") return "char";
    if (kind == "StringLiteral") return "string";
    return "unknown";
  }

  static bool isMutationNode(const JsonValue::Object &node) {
    const auto kindIt = node.find("kind");
    if (kindIt == node.end()) {
      return false;
    }
    if (kindIt->second.asString() == "VarDecl") {
      return true;
    }
    const auto categoryIt = node.find("category");
    if (categoryIt != node.end()) {
      std::string category = categoryIt->second.asString();
      return category == "assignment" || category == "increment" || category == "decrement";
    }
    return false;
  }

  static std::string rawId(const JsonValue &node) {
    return stringField(node, "id");
  }

  static std::string rawKind(const JsonValue &node) {
    return stringField(node, "kind");
  }

  static std::string nodeId(const JsonValue &node) {
    return "node:" + rawId(node);
  }

  static std::string makeScopeId(const std::string &prefix, const std::string &rawIdValue) {
    return "scope:" + prefix + ":" + rawIdValue;
  }

  static std::string makeSymbolId(const std::string &kind, const std::string &name, const std::string &rawIdValue) {
    return "symbol:" + kind + ":" + name + ":" + rawIdValue;
  }

  static std::string stringField(const JsonValue &node, const std::string &key) {
    const JsonValue *value = node.find(key);
    return value != nullptr && value->isString() ? value->asString() : "";
  }

  static bool boolField(const JsonValue &node, const std::string &key) {
    const JsonValue *value = node.find(key);
    return value != nullptr && value->isBool() ? value->asBool() : false;
  }

  static std::string typeText(const JsonValue &node) {
    const JsonValue *type = node.find("type");
    if (type == nullptr || !type->isObject()) {
      return "";
    }
    return stringField(*type, "qualType");
  }

  static std::string extractReturnType(const std::string &functionType) {
    std::size_t paren = functionType.find('(');
    if (paren == std::string::npos) {
      return functionType;
    }
    std::string prefix = functionType.substr(0, paren);
    while (!prefix.empty() && prefix.back() == ' ') {
      prefix.pop_back();
    }
    return prefix;
  }

  static JsonValue typeInfo(const std::string &text) {
    JsonValue::Object type;
    type["text"] = JsonValue(text);
    return JsonValue(type);
  }

  std::vector<JsonValue> rawChildren(const JsonValue &node) const {
    std::vector<JsonValue> children;
    const JsonValue *inner = node.find("inner");
    if (inner == nullptr || !inner->isArray()) {
      return children;
    }
    for (const auto &child : inner->asArray()) {
      if (child.isObject()) {
        children.push_back(child);
      }
    }
    return children;
  }

  static Location resolveLocation(const JsonValue *locationNode) {
    if (locationNode == nullptr || !locationNode->isObject()) {
      return {};
    }
    if (const JsonValue *expansion = locationNode->find("expansionLoc")) {
      Location resolved = resolveLocation(expansion);
      if (resolved.valid) {
        return resolved;
      }
    }
    if (const JsonValue *spelling = locationNode->find("spellingLoc")) {
      Location resolved = resolveLocation(spelling);
      if (resolved.valid) {
        return resolved;
      }
    }

    Location location;
    if (const JsonValue *offset = locationNode->find("offset"); offset != nullptr && offset->isNumber()) {
      location.offset = static_cast<int>(offset->asNumber());
      location.valid = true;
    }
    if (const JsonValue *line = locationNode->find("line"); line != nullptr && line->isNumber()) {
      location.line = static_cast<int>(line->asNumber());
      location.valid = true;
    }
    if (const JsonValue *col = locationNode->find("col"); col != nullptr && col->isNumber()) {
      location.col = static_cast<int>(col->asNumber());
      location.valid = true;
    }
    return location;
  }

  static JsonValue locationToJson(const Location &location) {
    JsonValue::Object object;
    object["line"] = JsonValue(location.line);
    object["col"] = JsonValue(location.col);
    object["offset"] = JsonValue(location.offset);
    return JsonValue(object);
  }

  static void appendLocation(JsonValue::Object &node, const JsonValue &rawNode) {
    if (const JsonValue *loc = rawNode.find("loc")) {
      Location resolved = resolveLocation(loc);
      if (resolved.valid) {
        node["loc"] = locationToJson(resolved);
      }
    }

    const JsonValue *range = rawNode.find("range");
    if (range == nullptr || !range->isObject()) {
      return;
    }
    const JsonValue *begin = range->find("begin");
    const JsonValue *end = range->find("end");
    Location start = resolveLocation(begin);
    Location finish = resolveLocation(end);
    if (!start.valid && !finish.valid) {
      return;
    }
    JsonValue::Object rangeObject;
    if (start.valid) {
      rangeObject["start"] = locationToJson(start);
    }
    if (finish.valid) {
      rangeObject["end"] = locationToJson(finish);
    }
    node["range"] = JsonValue(rangeObject);
  }

  static void appendTraceHints(JsonValue::Object &node, bool isStateful, bool isMutationPoint,
                               bool isControlFlow, bool isLoopLike, bool isReturnBoundary) {
    JsonValue::Object trace;
    trace["isStateful"] = JsonValue(isStateful);
    trace["isMutationPoint"] = JsonValue(isMutationPoint);
    trace["isControlFlow"] = JsonValue(isControlFlow);
    trace["isLoopLike"] = JsonValue(isLoopLike);
    trace["isReturnBoundary"] = JsonValue(isReturnBoundary);
    node["trace"] = JsonValue(trace);
  }

  std::string filePathForNode(const JsonValue &node) const {
    const JsonValue *loc = node.find("loc");
    if (loc != nullptr && loc->isObject()) {
      std::string file = stringField(*loc, "file");
      if (!file.empty()) {
        return file;
      }
    }
    const JsonValue *range = node.find("range");
    if (range != nullptr && range->isObject()) {
      if (const JsonValue *begin = range->find("begin"); begin != nullptr && begin->isObject()) {
        std::string file = stringField(*begin, "file");
        if (!file.empty()) {
          return file;
        }
      }
      if (const JsonValue *end = range->find("end"); end != nullptr && end->isObject()) {
        std::string file = stringField(*end, "file");
        if (!file.empty()) {
          return file;
        }
      }
    }
    return defaultFilePath_.empty() ? "source.cpp" : defaultFilePath_;
  }

  std::string ensureFile(const std::string &path) {
    std::string effectivePath = path.empty() ? "source.cpp" : path;
    defaultFilePath_ = effectivePath;
    std::string id = "file:" + effectivePath;
    if (knownFiles_.insert(id).second) {
      JsonValue::Object file;
      file["fileId"] = JsonValue(id);
      file["path"] = JsonValue(effectivePath);
      files_.push_back(JsonValue(file));
    }
    return id;
  }

  void writeScope(const std::string &id, const std::string &kind, const std::string &parentScopeId,
                  const std::string &ownerNodeId, const std::vector<std::string> &symbolIds) {
    JsonValue::Object scope;
    scope["id"] = JsonValue(id);
    scope["kind"] = JsonValue(kind);
    scope["ownerNodeId"] = JsonValue(ownerNodeId);
    scope["parentScopeId"] = parentScopeId.empty() ? JsonValue(nullptr) : JsonValue(parentScopeId);
    JsonValue::Array array;
    for (const auto &symbolId : symbolIds) {
      array.push_back(JsonValue(symbolId));
    }
    scope["symbolIds"] = JsonValue(array);
    scopes_[id] = JsonValue(scope);
  }

  void addSymbolToScope(const std::string &scopeId, const std::string &symbolId) {
    auto scopeIt = scopes_.find(scopeId);
    if (scopeIt == scopes_.end()) {
      return;
    }
    JsonValue::Array &array = scopeIt->second.asObject().at("symbolIds").asArray();
    array.push_back(JsonValue(symbolId));
  }

  void writeSymbol(const std::string &id, const std::string &name, const std::string &kind,
                   const std::string &qualifiedName, const std::string &type,
                   const std::string &declaredNodeId, const std::string &scopeId,
                   const std::string &storage) {
    JsonValue::Object symbol;
    symbol["id"] = JsonValue(id);
    symbol["name"] = JsonValue(name);
    symbol["kind"] = JsonValue(kind);
    symbol["qualifiedName"] = JsonValue(qualifiedName);
    symbol["type"] = typeInfo(type);
    symbol["declaredNodeId"] = JsonValue(declaredNodeId);
    symbol["scopeId"] = JsonValue(scopeId);
    symbol["storage"] = JsonValue(storage);
    symbols_[id] = JsonValue(symbol);
  }

  void finalizeChildIds() {
    for (auto &entry : childIds_) {
      JsonValue::Array array;
      for (const auto &childId : entry.second) {
        array.push_back(JsonValue(childId));
      }
      nodes_.at(entry.first).asObject()["childIds"] = JsonValue(array);
    }
  }

  void flush(JsonValue::Object &result) {
    JsonValue::Array entrypoints;
    for (const auto &entrypoint : entrypoints_) {
      entrypoints.push_back(JsonValue(entrypoint));
    }
    result["entrypoints"] = JsonValue(entrypoints);
    result["files"] = JsonValue(files_);

    JsonValue::Array roots;
    for (const auto &rootId : rootNodeIds_) {
      roots.push_back(JsonValue(rootId));
    }
    result["rootNodeIds"] = JsonValue(roots);
    result["scopes"] = JsonValue(scopes_);
    result["symbols"] = JsonValue(symbols_);
    result["nodes"] = JsonValue(nodes_);
  }

  std::string referencedName(const JsonValue &rawNode) const {
    const JsonValue *referencedDecl = rawNode.find("referencedDecl");
    if (referencedDecl != nullptr && referencedDecl->isObject()) {
      std::string name = stringField(*referencedDecl, "name");
      if (!name.empty()) {
        return name;
      }
    }
    return stringField(rawNode, "name");
  }

  std::string referencedSymbolId(const JsonValue &rawNode) const {
    const JsonValue *referencedDecl = rawNode.find("referencedDecl");
    if (referencedDecl == nullptr || !referencedDecl->isObject()) {
      return "";
    }
    std::string declKind = stringField(*referencedDecl, "kind");
    std::string name = stringField(*referencedDecl, "name");
    std::string id = stringField(*referencedDecl, "id");
    if (declKind == "ParmVarDecl") {
      return makeSymbolId("param", name, id);
    }
    if (declKind == "VarDecl") {
      return makeSymbolId("variable", name, id);
    }
    if (declKind == "FunctionDecl") {
      return makeSymbolId("function", name, id);
    }
    return "";
  }

  std::string lookupCalleeName(const JsonValue &rawNode) const {
    for (const auto &child : rawChildren(rawNode)) {
      std::string name = extractReferencedFunctionName(child);
      if (!name.empty()) {
        return name;
      }
    }
    return "";
  }

  std::string extractReferencedFunctionName(const JsonValue &rawNode) const {
    if (!rawNode.isObject()) {
      return "";
    }
    if (rawKind(rawNode) == "DeclRefExpr") {
      const JsonValue *referencedDecl = rawNode.find("referencedDecl");
      if (referencedDecl != nullptr && referencedDecl->isObject() && stringField(*referencedDecl, "kind") == "FunctionDecl") {
        return stringField(*referencedDecl, "name");
      }
    }
    for (const auto &child : rawChildren(rawNode)) {
      std::string name = extractReferencedFunctionName(child);
      if (!name.empty()) {
        return name;
      }
    }
    return "";
  }

  std::string translationScopeId() const {
    return makeScopeId("translation", rawId(rawRoot_));
  }

  static void ensure(bool condition, const std::string &message) {
    if (!condition) {
      throw std::runtime_error(message);
    }
  }

  const JsonValue &rawRoot_;
  std::string defaultFilePath_;
  std::set<std::string> knownFiles_;
  std::vector<JsonValue> files_;
  std::vector<std::string> entrypoints_;
  std::vector<std::string> rootNodeIds_;
  std::map<std::string, JsonValue> scopes_;
  std::map<std::string, JsonValue> symbols_;
  std::map<std::string, JsonValue> nodes_;
  std::map<std::string, std::vector<std::string>> childIds_;
};

std::string readFile(const std::string &path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Failed to open input file: " + path);
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

void writeFile(const std::string &path, const std::string &contents) {
  std::ofstream output(path);
  if (!output) {
    throw std::runtime_error("Failed to open output file: " + path);
  }
  output << contents;
}

int main(int argc, char **argv) {
  try {
    std::string inputPath = argc > 1 ? argv[1] : "./ast_dump.json";
    std::string outputPath = argc > 2 ? argv[2] : "normalized_ast.json";

    JsonParser parser(readFile(inputPath));
    JsonValue rawRoot = parser.parse();
    Normalizer normalizer(rawRoot);
    JsonValue normalized = normalizer.normalize();
    writeFile(outputPath, JsonWriter::write(normalized) + "\n");

    std::cout << "Wrote normalized AST to " << outputPath << "\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "normalize_ast failed: " << error.what() << "\n";
    return 1;
  }
}
