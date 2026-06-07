#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
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
      throw std::runtime_error("Unexpected trailing JSON characters");
    }
    return value;
  }

private:
  JsonValue parseValue() {
    skipWhitespace();
    ensure(position_ < input_.size(), "Unexpected end of JSON input");
    char ch = input_[position_];
    if (ch == '{') return parseObject();
    if (ch == '[') return parseArray();
    if (ch == '"') return JsonValue(parseString());
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
    throw std::runtime_error("Unsupported JSON token");
  }

  JsonValue parseObject() {
    expect('{');
    JsonValue::Object object;
    skipWhitespace();
    if (tryConsume('}')) {
      return JsonValue(object);
    }
    while (true) {
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
      skipWhitespace();
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
      array.push_back(parseValue());
      skipWhitespace();
      if (tryConsume(']')) {
        break;
      }
      expect(',');
      skipWhitespace();
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
        ensure(position_ < input_.size(), "Invalid JSON escape");
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
        case 'u':
          ensure(position_ + 4 <= input_.size(), "Invalid unicode escape");
          position_ += 4;
          result.push_back('?');
          break;
        default:
          throw std::runtime_error("Unsupported JSON escape");
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

struct Insertion {
  std::size_t offset = 0;
  std::string text;
};

std::string readFile(const std::string &path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Failed to open file: " + path);
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

std::string stringField(const JsonValue &value, const std::string &key) {
  const JsonValue *field = value.find(key);
  return field != nullptr && field->isString() ? field->asString() : "";
}

const JsonValue *objectField(const JsonValue &value, const std::string &key) {
  const JsonValue *field = value.find(key);
  return field != nullptr && field->isObject() ? field : nullptr;
}

const JsonValue *arrayField(const JsonValue &value, const std::string &key) {
  const JsonValue *field = value.find(key);
  return field != nullptr && field->isArray() ? field : nullptr;
}

std::size_t offsetFromNode(const JsonValue &node, const std::string &rangeKey, const std::string &boundKey) {
  const JsonValue *range = objectField(node, rangeKey);
  if (range == nullptr) {
    return 0;
  }
  const JsonValue *bound = objectField(*range, boundKey);
  if (bound == nullptr) {
    return 0;
  }
  const JsonValue *offset = bound->find("offset");
  return offset != nullptr && offset->isNumber() ? static_cast<std::size_t>(offset->asNumber()) : 0;
}

std::string indentationAt(const std::string &source, std::size_t offset) {
  std::size_t lineStart = source.rfind('\n', offset);
  lineStart = lineStart == std::string::npos ? 0 : lineStart + 1;
  std::size_t index = lineStart;
  while (index < source.size() && (source[index] == ' ' || source[index] == '\t')) {
    ++index;
  }
  return source.substr(lineStart, index - lineStart);
}

std::size_t statementEnd(const std::string &source, std::size_t startOffset) {
  std::size_t index = startOffset;
  while (index < source.size() && source[index] != ';') {
    ++index;
  }
  if (index >= source.size()) {
    throw std::runtime_error("Failed to find statement terminator");
  }
  return index;
}

std::size_t lineStartOffset(const std::string &source, std::size_t offset) {
  std::size_t lineStart = source.rfind('\n', offset);
  return lineStart == std::string::npos ? 0 : lineStart + 1;
}

std::string traceLine(const std::string &name, const std::string &indent) {
  return "\n" + indent + "__trace_state(\"" + name + "\", " + name + ");";
}

std::string traceBlock(const std::vector<std::string> &names, const std::string &indent) {
  std::string block;
  for (const std::string &name : names) {
    block += traceLine(name, indent);
  }
  return block;
}

std::string blockLabel(const JsonValue &node) {
  const std::string kind = stringField(node, "kind");
  if (kind == "FunctionDecl") {
    return "function:" + stringField(node, "name");
  }
  if (kind == "CompoundStmt") {
    return "compound";
  }
  if (kind == "ForStmt") {
    return "loop:for";
  }
  if (kind == "WhileStmt") {
    return "loop:while";
  }
  if (kind == "DoStmt") {
    return "loop:do";
  }
  return kind;
}

std::string eventLine(const std::string &eventName, const std::string &label, const std::string &indent) {
  return "\n" + indent + "__trace_event(\"" + eventName + "\", \"" + label + "\");";
}

std::string nearestFunctionLabel(const JsonValue &node, const std::map<std::string, JsonValue::Object> &nodeObjects) {
  std::string parentId = stringField(node, "parentId");
  while (!parentId.empty()) {
    auto parentIt = nodeObjects.find(parentId);
    if (parentIt == nodeObjects.end()) {
      break;
    }
    if (stringField(JsonValue(parentIt->second), "kind") == "FunctionDecl") {
      return blockLabel(JsonValue(parentIt->second));
    }
    parentId = stringField(JsonValue(parentIt->second), "parentId");
  }
  return "function";
}

const JsonValue::Object *nearestFunctionNode(const JsonValue &node, const std::map<std::string, JsonValue::Object> &nodeObjects) {
  std::string parentId = stringField(node, "parentId");
  while (!parentId.empty()) {
    auto parentIt = nodeObjects.find(parentId);
    if (parentIt == nodeObjects.end()) {
      break;
    }
    if (stringField(JsonValue(parentIt->second), "kind") == "FunctionDecl") {
      return &parentIt->second;
    }
    parentId = stringField(JsonValue(parentIt->second), "parentId");
  }
  return nullptr;
}

std::string functionEnterLine(const JsonValue &functionNode,
                              const std::vector<std::string> &paramNames,
                              const std::string &indent) {
  std::ostringstream output;
  output << "\n" << indent << "__trace_file << \"[enter] " << stringField(functionNode, "name");
  for (const std::string &param : paramNames) {
    output << " " << param << "=\" << " << param << " << \"";
  }
  output << "\\n\";";
  return output.str();
}

std::string returnWrapPrefix(const std::string &functionName) {
  return "__trace_return(\"" + functionName + "\", ";
}

std::string identifierName(const JsonValue &node) {
  return stringField(node, "name");
}

int main(int argc, char **argv) {
  try {
    const std::string astPath = argc > 1 ? argv[1] : "normalized_ast.json";
    const std::string sourcePath = argc > 2 ? argv[2] : "source.cpp";
    const std::string outputPath = argc > 3 ? argv[3] : "source.injected.cpp";

    JsonParser parser(readFile(astPath));
    JsonValue root = parser.parse();
    const JsonValue *nodesValue = objectField(root, "nodes");
    if (nodesValue == nullptr) {
      throw std::runtime_error("Normalized AST has no nodes object");
    }

    const auto &nodes = nodesValue->asObject();
    std::map<std::string, JsonValue::Object> nodeObjects;
    for (const auto &entry : nodes) {
      if (entry.second.isObject()) {
        nodeObjects.emplace(entry.first, entry.second.asObject());
      }
    }
    std::string source = readFile(sourcePath);
    std::vector<Insertion> insertions;
    std::set<std::pair<std::size_t, std::string>> dedupe;

    for (const auto &entry : nodes) {
      const JsonValue &node = entry.second;
      const std::string kind = stringField(node, "kind");

      if (kind == "FunctionDecl") {
        const JsonValue *paramIds = arrayField(node, "paramIds");
        std::string bodyId = stringField(node, "bodyId");
        auto bodyIt = nodes.find(bodyId);
        if (paramIds != nullptr && bodyIt != nodes.end()) {
          std::vector<std::string> params;
          for (const JsonValue &paramIdValue : paramIds->asArray()) {
            if (!paramIdValue.isString()) {
              continue;
            }
            auto paramIt = nodes.find(paramIdValue.asString());
            if (paramIt == nodes.end()) {
              continue;
            }
            std::string name = stringField(paramIt->second, "name");
            if (!name.empty()) {
              params.push_back(name);
            }
          }
          if (!params.empty()) {
            std::size_t braceOffset = offsetFromNode(bodyIt->second, "range", "start");
            std::string indent = indentationAt(source, braceOffset) + "  ";
            std::string text = functionEnterLine(node, params, indent);
            if (dedupe.insert({braceOffset + 1, text}).second) {
              insertions.push_back({braceOffset + 1, text});
            }
          }
        }
      }

      if (kind == "CompoundStmt" || kind == "ForStmt" || kind == "WhileStmt" || kind == "DoStmt") {
        std::size_t startOffset = offsetFromNode(node, "range", "start");
        std::size_t endOffset = offsetFromNode(node, "range", "end");
        std::string indent = indentationAt(source, startOffset) + "  ";
        std::string enterText = eventLine("enter", blockLabel(node), indent);
        if (dedupe.insert({startOffset + 1, enterText}).second) {
          insertions.push_back({startOffset + 1, enterText});
        }
        std::string exitIndent = indentationAt(source, endOffset);
        std::string exitText = "\n" + exitIndent + "__trace_event(\"exit\", \"" + blockLabel(node) + "\");";
        if (dedupe.insert({endOffset, exitText}).second) {
          insertions.push_back({endOffset, exitText});
        }
      }

      if ((kind == "IfStmt" || kind == "ForStmt" || kind == "WhileStmt" || kind == "DoStmt")) {
        const JsonValue *childIds = arrayField(node, "childIds");
        if (childIds != nullptr) {
          for (const JsonValue &childIdValue : childIds->asArray()) {
            if (!childIdValue.isString()) {
              continue;
            }
            auto childIt = nodes.find(childIdValue.asString());
            if (childIt == nodes.end()) {
              continue;
            }
            const JsonValue &childNode = childIt->second;
            const std::string childKind = stringField(childNode, "kind");
            if (childKind == "CompoundStmt" || childKind == "BinaryExpr" || childKind == "Identifier" || childKind == "Literal") {
              continue;
            }
            std::size_t childStart = offsetFromNode(childNode, "range", "start");
            std::size_t childEnd = statementEnd(source, offsetFromNode(childNode, "range", "end"));
            std::string parentIndent = indentationAt(source, childStart);
            std::string innerIndent = parentIndent + "  ";
            std::string openBrace = "{\n" + innerIndent;
            std::string closeBrace = "\n" + parentIndent + "}";
            if (dedupe.insert({childStart, openBrace}).second) {
              insertions.push_back({childStart, openBrace});
            }
            if (dedupe.insert({childEnd + 1, closeBrace}).second) {
              insertions.push_back({childEnd + 1, closeBrace});
            }
            break;
          }
        }
      }

      if (kind == "VarDecl") {
        std::string name = stringField(node, "name");
        std::string initId = stringField(node, "initId");
        if (!name.empty() && !initId.empty()) {
          std::size_t end = statementEnd(source, offsetFromNode(node, "range", "end"));
          std::string indent = indentationAt(source, offsetFromNode(node, "range", "start"));
          std::string text = traceLine(name, indent);
          if (dedupe.insert({end + 1, text}).second) {
            insertions.push_back({end + 1, text});
          }
        }
      }

      if (kind == "BinaryExpr" && stringField(node, "category") == "assignment") {
        std::string lhsId = stringField(node, "lhsId");
        auto lhsIt = nodes.find(lhsId);
        if (lhsIt != nodes.end()) {
          std::string name = identifierName(lhsIt->second);
          if (!name.empty()) {
            std::size_t end = statementEnd(source, offsetFromNode(node, "range", "end"));
            std::string indent = indentationAt(source, offsetFromNode(node, "range", "start"));
            std::string text = traceLine(name, indent);
            if (dedupe.insert({end + 1, text}).second) {
              insertions.push_back({end + 1, text});
            }
          }
        }
      }

      if (kind == "UnaryExpr") {
        const std::string category = stringField(node, "category");
        if (category == "increment" || category == "decrement") {
          std::string operandId = stringField(node, "operandId");
          auto operandIt = nodes.find(operandId);
          if (operandIt != nodes.end()) {
            std::string name = identifierName(operandIt->second);
            if (!name.empty()) {
              std::size_t end = statementEnd(source, offsetFromNode(node, "range", "end"));
              std::string indent = indentationAt(source, offsetFromNode(node, "range", "start"));
              std::string text = traceLine(name, indent);
              if (dedupe.insert({end + 1, text}).second) {
                insertions.push_back({end + 1, text});
              }
            }
          }
        }
      }

      if (kind == "ReturnStmt") {
        std::size_t startOffset = offsetFromNode(node, "range", "start");
        const JsonValue::Object *functionNode = nearestFunctionNode(node, nodeObjects);
        if (functionNode == nullptr) {
          continue;
        }
        std::string functionName = stringField(JsonValue(*functionNode), "name");
        std::size_t returnKeywordEnd = startOffset + std::string("return").size();
        while (returnKeywordEnd < source.size() && std::isspace(static_cast<unsigned char>(source[returnKeywordEnd]))) {
          ++returnKeywordEnd;
        }
        std::size_t endOffset = statementEnd(source, offsetFromNode(node, "range", "end"));
        std::string prefix = returnWrapPrefix(functionName);
        if (dedupe.insert({returnKeywordEnd, prefix}).second) {
          insertions.push_back({returnKeywordEnd, prefix});
        }
        if (dedupe.insert({endOffset, ")"}).second) {
          insertions.push_back({endOffset, ")"});
        }
      }
    }

    std::sort(insertions.begin(), insertions.end(), [](const Insertion &left, const Insertion &right) {
      return left.offset > right.offset;
    });

    for (const Insertion &insertion : insertions) {
      source.insert(insertion.offset, insertion.text);
    }

    if (source.find("#include <fstream>") == std::string::npos) {
      source = "#include <fstream>\n\n" + source;
    }
    source =
        "#include <fstream>\n\n"
        "static std::ofstream __trace_file(\"test/state_trace.log\");\n\n"
        "inline void __trace_event(const char *event_name, const char *label) {\n"
        "  __trace_file << \"[block] \" << event_name << \":\" << label << \"\\n\";\n"
        "}\n\n"
        "template <typename T>\n"
        "T __trace_return(const char *function_name, const T &value) {\n"
        "  __trace_file << \"[return] \" << function_name << \" value=\" << value << \"\\n\";\n"
        "  return value;\n"
        "}\n\n"
        "template <typename T>\n"
        "void __trace_state(const char *name, const T &value) {\n"
        "  __trace_file << \"[state] \" << name << \"=\" << value << \"\\n\";\n"
        "}\n\n" +
        source;

    writeFile(outputPath, source);
    std::cout << "Wrote injected source to " << outputPath << "\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "injecter failed: " << error.what() << "\n";
    return 1;
  }
}
