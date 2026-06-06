const { enqueueJob, readJob } = require('../../services/job-status-service');

function createJob(req, res) {
  const validation = validateJobPayload(req.body);
  if (!validation.ok) {
    res.status(400).json({
      error: 'invalid_request',
      message: validation.message,
    });
    return;
  }

  const job = enqueueJob(validation.payload);
  res.status(202).json(job);
}

function getJob(req, res) {
  const job = readJob(req.params.jobId);
  if (!job) {
    res.status(404).json({
      error: 'not_found',
      message: 'Job not found.',
    });
    return;
  }

  res.json(job);
}

function validateJobPayload(body) {
  if (!body || typeof body !== 'object') {
    return { ok: false, message: 'Request body must be a JSON object.' };
  }

  const code = typeof body.code === 'string' ? body.code.trim() : '';
  const entryMode = body.entryMode === 'targeted' ? 'targeted' : 'standalone';
  const targetFunction = typeof body.targetFunction === 'string' ? body.targetFunction.trim() : '';
  const parameters = Array.isArray(body.parameters) ? body.parameters : [];
  const globals = Array.isArray(body.globals) ? body.globals : [];

  if (!code) {
    return { ok: false, message: 'Code is required.' };
  }

  if (entryMode === 'targeted' && !targetFunction) {
    return { ok: false, message: 'Target function is required for targeted mode.' };
  }

  const normalizedParameters = [];
  for (const param of parameters) {
    if (!param || typeof param !== 'object') {
      return { ok: false, message: 'Each parameter entry must be an object.' };
    }

    const name = typeof param.name === 'string' ? param.name.trim() : '';
    const value = typeof param.value === 'string' ? param.value : '';
    if (!name) {
      return { ok: false, message: 'Each parameter must include a name.' };
    }

    normalizedParameters.push({
      name,
      value,
      dataType: typeof param.dataType === 'string' ? param.dataType : '',
    });
  }

  const normalizedGlobals = [];
  for (const globalVar of globals) {
    if (!globalVar || typeof globalVar !== 'object') {
      return { ok: false, message: 'Each global entry must be an object.' };
    }

    const name = typeof globalVar.name === 'string' ? globalVar.name.trim() : '';
    const value = typeof globalVar.value === 'string' ? globalVar.value : '';
    if (!name) {
      return { ok: false, message: 'Each global variable must include a name.' };
    }

    normalizedGlobals.push({
      name,
      value,
      dataType: typeof globalVar.dataType === 'string' ? globalVar.dataType : '',
    });
  }

  return {
    ok: true,
    payload: {
      code,
      entryMode,
      targetFunction,
      parameters: normalizedParameters,
      globals: normalizedGlobals,
      sourceName: typeof body.sourceName === 'string' ? body.sourceName : 'snippet.cpp',
    },
  };
}

module.exports = {
  createJob,
  getJob,
};
