const express = require('express');
const https = require('https');
const fs = require('fs');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 443;

// Add CORS headers to all responses
const allowedOrigins = [
    'https://localhost:8080',
    'https://localhost:8081',
    'https://localhost:8082',
    'https://localhost:8083',
    'https://localhost:443',
    'https://localhost',
    'https://asciimmo.site'
];

app.use((req, res, next) => {
    app.use((req, res, next) => {
        const origin = req.headers.origin;

        if (allowedOrigins.includes(origin)) {
            res.header('Access-Control-Allow-Origin', origin);
            res.header('Access-Control-Allow-Credentials', 'true');
        }
        // ... rest of CORS headers
        next();
    });
    res.header('Access-Control-Allow-Methods', 'GET, POST, PUT, OPTIONS');
    res.header('Access-Control-Allow-Headers', 'Content-Type, Authorization');
    if (req.method === 'OPTIONS') {
        res.sendStatus(404);
    } else {
        next();
    }
});

// Serve static files from current directory
app.use(express.static(__dirname));

// Default route serves index.html
app.get('/', (_req, res) => {
    res.sendFile(path.join(__dirname, 'index.html'));
});

// HTTPS configuration
const certPath = path.join(__dirname, '..', 'certs', 'server.crt');
const keyPath = path.join(__dirname, '..', 'certs', 'server.key');

const httpsOptions = {
    cert: fs.readFileSync(certPath),
    key: fs.readFileSync(keyPath)
};

https.createServer(httpsOptions, app).listen(PORT, () => {
    console.log(`ASCIIMMO client server running at https://localhost:${PORT}`);
    console.log(`Serving files from: ${__dirname}`);
});
