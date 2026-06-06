const fs = require('fs');
const path = require('path');

const sourcePath = path.join(__dirname, 'source.cpp');
const astPath = path.join(__dirname, 'ast_dump.json');
const outputPath = path.join(__dirname, 'source.instrumented.cpp');

const source = fs.readFileSync(sourcePath, 'utf8');
const ast = JSON.parse(fs.readFileSync(astPath, 'utf8'));

function walk(node, visit) {
  if (!node || typeof node !== 'object') {
    return;
  }

  visit(node);

  if (Array.isArray(node.inner)) {
    for (const child of node.inner) {
      walk(child, visit);
    }
  }
}

function findReferencedName(node) {
  if (!node || typeof node !== 'object') {
    return null;
  }

  if (node.kind === 'DeclRefExpr' && node.referencedDecl?.name) {
    return node.referencedDecl.name;
  }

  if (Array.isArray(node.inner)) {
    for (const child of node.inner) {
      const name = findReferencedName(child);
      if (name) {
        return name;
      }
    }
  }

  return null;
}

function findStatementEnd(sourceText, startOffset) {
  let idx = startOffset;
  while (idx < sourceText.length && sourceText[idx] !== ';') {
    idx += 1;
  }

  if (idx >= sourceText.length) {
    throw new Error(`Could not find ';' after offset ${startOffset}`);
  }

  return idx;
}

function indentationAt(sourceText, offset) {
  const lineStart = sourceText.lastIndexOf('\n', offset) + 1;
  const linePrefix = sourceText.slice(lineStart, offset);
  const match = linePrefix.match(/^\s*/);
  return match ? match[0] : '';
}

const insertions = [];

walk(ast, (node) => {
  if (node.kind === 'VarDecl' && (node.name === 'a' || node.name === 'b') && node.init) {
    const statementEnd = findStatementEnd(source, node.range.end.offset);
    const indent = indentationAt(source, node.range.begin.offset);
    insertions.push({
      offset: statementEnd + 1,
      text: `\n${indent}std::cout << "[state] ${node.name}=" << ${node.name} << "\\n";`,
    });
  }

  if (node.kind === 'BinaryOperator' && node.opcode === '=' && Array.isArray(node.inner) && node.inner[0]) {
    const lhsName = findReferencedName(node.inner[0]);
    if (lhsName === 'a' || lhsName === 'b') {
      const statementEnd = findStatementEnd(source, node.range.end.offset);
      const indent = indentationAt(source, node.range.begin.offset);
      insertions.push({
        offset: statementEnd + 1,
        text: `\n${indent}std::cout << "[state] ${lhsName}=" << ${lhsName} << "\\n";`,
      });
    }
  }
});

insertions.sort((left, right) => right.offset - left.offset);

let instrumented = source;
for (const insertion of insertions) {
  instrumented =
    instrumented.slice(0, insertion.offset) +
    insertion.text +
    instrumented.slice(insertion.offset);
}

if (!instrumented.includes('#include <iostream>')) {
  instrumented = `#include <iostream>\n\n${instrumented}`;
}

fs.writeFileSync(outputPath, instrumented);
console.log(`Wrote ${outputPath}`);
