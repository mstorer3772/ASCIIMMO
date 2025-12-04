const express = require('express');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 80;

// Serve static files from current directory
app.use(express.static(__dirname));

// Default route serves index.html
app.get('/', (_req, res) => {
    res.sendFile(path.join(__dirname, 'index.html'));
});

app.listen(PORT, () => {
    console.log(`ASCIIMMO client server running at http://localhost:${PORT}`);
    console.log(`Serving files from: ${__dirname}`);
});
