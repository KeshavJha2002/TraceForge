const express = require('express');
const { createJob, getJob } = require('./controllers/job-controller');
const { getTestTrace } = require('./controllers/test-trace-controller');

function createServer() {
  const app = express();

  app.use(express.json({ limit: '2mb' }));
  app.use((req, res, next) => {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
    res.setHeader('Access-Control-Allow-Methods', 'GET,POST,OPTIONS');

    if (req.method === 'OPTIONS') {
      res.sendStatus(204);
      return;
    }

    next();
  });

  app.get('/api/health', (_req, res) => {
    res.json({ ok: true });
  });

  app.get('/test', getTestTrace);
  app.post('/api/jobs', createJob);
  app.get('/api/jobs/:jobId', getJob);

  return app;
}

module.exports = { createServer };
