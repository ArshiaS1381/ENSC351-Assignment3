// Connect to the Node.js server
const socket = io();

// --- Get HTML elements ---
const tempoDisplay = document.getElementById('tempo-display');
const volumeDisplay = document.getElementById('volume-display');
const volumeSlider = document.getElementById('volume-slider');

const modeButtons = {
    none: document.getElementById('btn-mode-none'),
    rock: document.getElementById('btn-mode-rock'),
    custom: document.getElementById('btn-mode-custom'),
};

// --- Helper function to send commands ---
function sendCommand(cmd) {
    console.log(`Sending command: ${cmd}`);
    socket.emit('command', cmd);

    // After sending a command that changes state,
    // immediately ask for the new status.
    const nonStatusCmds = ['get_status', 'help', 'start_program'];
    if (!nonStatusCmds.includes(cmd) && !cmd.startsWith('play')) {
        setTimeout(() => socket.emit('command', 'get_status'), 50); // Small delay
    }
}

// --- Helper function to update button UI ---
function updateActiveButtons(mode) {
    // 0 = none, 1 = rock, 2 = custom
    modeButtons.none.classList.remove('active');
    modeButtons.rock.classList.remove('active');
    modeButtons.custom.classList.remove('active');

    if (mode == 0) {
        modeButtons.none.classList.add('active');
    } else if (mode == 1) {
        modeButtons.rock.classList.add('active');
    } else if (mode == 2) {
        modeButtons.custom.classList.add('active');
    }
}

// ==========================================================
// --- THIS IS THE CRITICAL MISSING SECTION ---
// Listen for 'status_update' messages from the Node server
// ==========================================================
socket.on('status_update', (msg) => {
    console.log("Status from server:", msg);

    // Parse tempo (e.g., "tempo=125, mode=1, volume=8\n")
    let match = msg.match(/tempo=(\d+)/);
    if (match) {
        tempoDisplay.textContent = match[1];
    }
    
    // Parse volume
    match = msg.match(/volume=(\d+)/);
    if (match) {
        const vol = match[1];
        volumeDisplay.textContent = vol;
        volumeSlider.value = vol;
    }
    
    // Parse mode
    match = msg.match(/mode=(\d+)/);
    if (match) {
        updateActiveButtons(match[1]);
    }
});


// --- Mode Buttons ---
document.getElementById('btn-mode-none').addEventListener('click', () => {
    sendCommand('mode none');
});
document.getElementById('btn-mode-rock').addEventListener('click', () => {
    sendCommand('mode rock');
});
document.getElementById('btn-mode-custom').addEventListener('click', () => {
    sendCommand('mode custom');
});

// --- Tempo Buttons ---
document.getElementById('btn-tempo-down').addEventListener('click', () => {
    sendCommand('tempo -');
});
document.getElementById('btn-tempo-up').addEventListener('click', () => {
    sendCommand('tempo +');
});

// --- Volume Slider ---
volumeSlider.addEventListener('input', () => {
    volumeDisplay.textContent = volumeSlider.value;
});
volumeSlider.addEventListener('change', () => {
    sendCommand('volume ' + volumeSlider.value);
});

// --- Play Sound Buttons ---
document.getElementById('btn-play-hihat').addEventListener('click', () => {
    sendCommand('play hihat');
});
document.getElementById('btn-play-snare').addEventListener('click', () => {
    sendCommand('play snare');
});
document.getElementById('btn-play-base').addEventListener('click', () => {
    sendCommand('play base');
});

// --- Stop Program Button ---
document.getElementById('btn-stop').addEventListener('click', () => {
    sendCommand('stop');
});

// --- Start Program Button ---
document.getElementById('btn-start-program').addEventListener('click', () => {
    console.log("Sending 'start_program' command...");
    socket.emit('command', 'start_program');
    
    // After starting, wait a second and get status
    setTimeout(() => sendCommand('get_status'), 1000);
});

// --- Get initial status (if app is already running) ---
window.addEventListener('load', () => {
    console.log("Page loaded. Requesting initial status in case app is running.");
    sendCommand('get_status');
});