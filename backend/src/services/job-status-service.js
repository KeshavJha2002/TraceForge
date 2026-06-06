const jobs = new Map();

function enqueueJob(payload) {
  const jobId = createJobId();
  const now = new Date().toISOString();

  const record = {
    jobId,
    status: 'generating',
    createdAt: now,
    updatedAt: now,
    request: payload,
    progress: {
      stage: 'accepted',
      message: 'Generating the trace graph - please wait.',
    },
    result: {
      graph: null,
      flatTrace: null,
      preview: {
        title: 'Trace graph placeholder',
        body: 'Execution graph output will render here once the worker pipeline is connected.',
      },
    },
  };

  jobs.set(jobId, record);
  return record;
}

function readJob(jobId) {
  return jobs.get(jobId) || null;
}

function createJobId() {
  return `job_${Math.random().toString(36).slice(2, 10)}`;
}

module.exports = {
  enqueueJob,
  readJob,
};
