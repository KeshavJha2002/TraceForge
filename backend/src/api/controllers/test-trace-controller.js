const { readTestTrace } = require('../../services/test-trace-service');

function getTestTrace(_req, res) {
  const trace = readTestTrace();
  res.json(trace);
}

module.exports = {
  getTestTrace,
};
