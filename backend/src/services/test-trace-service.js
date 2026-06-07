const fs = require('fs');
const path = require('path');

const TRACE_FILE_PATH = path.resolve(__dirname, '../../../test/state_trace.log');

function readTestTrace() {
  if (!fs.existsSync(TRACE_FILE_PATH)) {
    return {
      tracePath: TRACE_FILE_PATH,
      raw: '',
      events: [],
    };
  }

  const raw = fs.readFileSync(TRACE_FILE_PATH, 'utf8');
  return {
    tracePath: TRACE_FILE_PATH,
    raw,
    events: parseTrace(raw),
  };
}

function parseTrace(raw) {
  return raw
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter(Boolean)
    .map((line, index) => parseTraceLine(line, index))
    .filter(Boolean);
}

function parseTraceLine(line, index) {
  const blockMatch = line.match(/^\[block\]\s+(\w+):(.*)$/);
  if (blockMatch) {
    return {
      index,
      type: 'block',
      event: blockMatch[1],
      label: blockMatch[2],
      raw: line,
    };
  }

  const enterMatch = line.match(/^\[enter\]\s+(\w+)(?:\s+(.*))?$/);
  if (enterMatch) {
    return {
      index,
      type: 'enter',
      fn: enterMatch[1],
      args: parseAssignments(enterMatch[2] || ''),
      raw: line,
    };
  }

  const returnMatch = line.match(/^\[return\]\s+(\w+)\s+value=(.*)$/);
  if (returnMatch) {
    return {
      index,
      type: 'return',
      fn: returnMatch[1],
      value: returnMatch[2],
      raw: line,
    };
  }

  const stateMatch = line.match(/^\[state\]\s+([A-Za-z_]\w*)=(.*)$/);
  if (stateMatch) {
    return {
      index,
      type: 'state',
      name: stateMatch[1],
      value: stateMatch[2],
      raw: line,
    };
  }

  return {
    index,
    type: 'unknown',
    raw: line,
  };
}

function parseAssignments(text) {
  if (!text.trim()) {
    return {};
  }

  const tokens = text.trim().split(/\s+/);
  const args = {};
  for (const token of tokens) {
    const separatorIndex = token.indexOf('=');
    if (separatorIndex <= 0) {
      continue;
    }
    const key = token.slice(0, separatorIndex);
    const value = token.slice(separatorIndex + 1);
    args[key] = value;
  }
  return args;
}

module.exports = {
  readTestTrace,
};
