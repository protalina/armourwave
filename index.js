const express = require('express');
const WebSocket = require('ws');
const net = require('net');
const fs = require('fs');

let newString = "";
// Serve static files (e.g., HTML, CSS, JS)

let fileCounter = 1; // Counter to keep track of file numbers
let mainBuffer = Buffer.alloc(200 * 1024); // Buffer to store partial data from main clients (200 KB)
let espBuffer = Buffer.alloc(200 * 1024); // Buffer to store partial data from ESP device (200 KB)

// Create server for ESP communication
const espServer = net.createServer((espSocket) => {
    console.log('ESP device connected');

    espSocket.on('data', (data) => {
        // Add received data to the ESP buffer
        espBuffer = Buffer.concat([espBuffer, data]);

        // Check if the ESP buffer contains a complete string
        let newlineIndex;
        while ((newlineIndex = espBuffer.indexOf('\n')) !== -1) {
            // Extract the complete string from the buffer
            const completeString = espBuffer.slice(0, newlineIndex).toString().trim();
            espBuffer = espBuffer.slice(newlineIndex + 1);

            // Process the data received from ESP device
            console.log('Received from ESP:', completeString);
            newString = completeString;
        }
    });

    espSocket.on('end', () => {
        console.log('ESP device disconnected');
    });

    espSocket.on('error', (err) => {
        console.error('ESP socket error:', err);
    });
});

const ESP_PORT = 3001; // Port for ESP communication
espServer.listen(ESP_PORT, () => {
    console.log(`ESP server listening on port ${ESP_PORT}`);
});

// Create server for other clients
const mainServer = net.createServer((socket) => {
    console.log('Client connected');

    socket.on('data', (data) => {
        // Add received data to the main buffer
        mainBuffer = Buffer.concat([mainBuffer, data]);

        // Check if the main buffer contains a complete string
        let newlineIndex;
        while ((newlineIndex = mainBuffer.indexOf('\n')) !== -1) {
            // Extract the complete string from the buffer
            const completeString = mainBuffer.slice(0, newlineIndex).toString().trim();
            mainBuffer = mainBuffer.slice(newlineIndex + 1);

            // Generate unique file name for each string
            const fileName = `data${fileCounter}.txt`;

            // Write the complete string to a .txt file
            fs.writeFile(fileName, completeString, (err) => {
                if (err) {
                    console.error(`Error writing data to ${fileName}:`, err);
                } else {
                    console.log(`Data has been written to ${fileName}`);
                }
            });

            fileCounter++; // Increment file counter for the next string
        }
    });

    socket.on('end', () => {
        console.log('Client disconnected');
    });

    socket.on('error', (err) => {
        console.error('Socket error:', err);
    });
});

const MAIN_PORT = 3000; // Port for other clients
mainServer.listen(MAIN_PORT, () => {
    console.log(`Main server listening on port ${MAIN_PORT}`);
});

const ip = '192.168.126.67';
const port = 8080;
const wss = new WebSocket.Server({ host: ip, port });

wss.on('listening', () => {
  console.log(`WebSocket server listening at ${ip}:${port}`);
});

wss.on('connection', (ws) => {
  console.log('WebSocket client connected');

  ws.on('message', (data) => {
    processImage(data, () => {});
  });

  ws.on('close', () => {
    console.log('WebSocket client disconnected');
  });

  ws.on('error', (err) => {
    console.error('WebSocket error:', err);
  });

  ws.send('Hello from WebSocket server!');
});

function processImage(imageData, callback) {
  const fs = require('fs');
  const path = `public/images/image.png`;
  fs.writeFile(path, imageData, (err) => {
    if (err) {
      console.error('Failed to save the image:', err);
      callback();
      return;
    }
    callback();
  });
}




const appOne = express();
const portOne = 3005;

// Serve static files from 'public' directory
appOne.use(express.static('public'));

// API endpoint sending data to the frontend
appOne.get('/data', (req, res) => {
    const sampleData = {
        message: newString,
        time: new Date().toISOString()
    };
    res.json(sampleData);
});

appOne.listen(portOne, () => {
    console.log(`Server listening at http://localhost:${portOne}`);
});
