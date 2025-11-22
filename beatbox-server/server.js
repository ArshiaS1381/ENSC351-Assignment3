// --- C App (BeatBox) Connection ---
const dgram = require('dgram');
const C_APP_PORT = 12345;
const C_APP_IP = '127.0.0.1'; // Use 'localhost' for WSL testing
const udpClient = dgram.createSocket('udp4');

// --- Web Server Setup ---
const express = require('express');
const app = express();
const http = require('http');
const server = http.createServer(app);
const { Server } = require("socket.io");
const io = new Server(server);
const WEB_PORT = 8088; // Port for the browser

// Serve all static files from the 'public' folder
app.use(express.static('public'));

// --- Main Logic ---

// 1. Listen for browser connections
io.on('connection', (socket) => {
  console.log('Web browser connected.');

  // 2. Listen for 'command' messages from the browser
  socket.on('command', (msg) => {
    console.log(`Browser sent command: '${msg}'`);

    // 3. Forward the command to the C app via UDP
    const command = Buffer.from(msg);
    udpClient.send(command, 0, command.length, C_APP_PORT, C_APP_IP, (err) => {
      if (err) {
        console.error("Error sending UDP message:", err);
      }
    });
  });

  // Handle browser disconnect
  socket.on('disconnect', () => {
    console.log('Web browser disconnected.');
  });
});

// 4. Listen for replies from the C app
udpClient.on('message', (msg, rinfo) => {
  const reply = msg.toString();
  console.log(`C app replied: '${reply}'`);

  // Broadcast this status reply to ALL connected browsers
  io.emit('status_update', reply);
});

// Start the web server
server.listen(WEB_PORT, () => {
  console.log(`Web server listening on http://localhost:${WEB_PORT}`);
  console.log(`Forwarding commands to C app at udp://${C_APP_IP}:${C_APP_PORT}`);
});