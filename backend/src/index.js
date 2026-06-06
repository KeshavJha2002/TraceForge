const { createServer } = require('./api/server');

const port = Number(process.env.PORT || 3001);
const app = createServer();

app.listen(port, () => {
  console.log(`TraceForge API listening on http://localhost:${port}`);
});
