const world_pb = require('./proto/world_pb.js');

// Cookie helper functions
function setCookie(name, value, days = 30) {
  const expires = new Date();
  expires.setTime(expires.getTime() + days * 24 * 60 * 60 * 1000);
  document.cookie = `${name}=${value};expires=${expires.toUTCString()};path=/`;
}

function getCookie(name) {
  const nameEQ = name + "=";
  const ca = document.cookie.split(';');
  for (let i = 0; i < ca.length; i++) {
    let c = ca[i];
    while (c.charAt(0) === ' ') c = c.substring(1, c.length);
    if (c.indexOf(nameEQ) === 0) return c.substring(nameEQ.length, c.length);
  }
  return null;
}

function deleteCookie(name) {
  document.cookie = `${name}=;expires=Thu, 01 Jan 1970 00:00:00 UTC;path=/;`;
}

// Auth state management
let sessionToken = getCookie('session_token');
let username = getCookie('username');

function updateAuthUI() {
  const authStatus = document.getElementById('auth-status');
  const loginForm = document.getElementById('login-form');
  const registerForm = document.getElementById('register-form');
  const loggedIn = document.getElementById('logged-in');
  const currentUsername = document.getElementById('current-username');

  if (sessionToken && username) {
    loginForm.classList.add('hidden');
    registerForm.classList.add('hidden');
    loggedIn.classList.remove('hidden');
    currentUsername.textContent = username;
    authStatus.textContent = '';
  } else {
    loginForm.classList.remove('hidden');
    registerForm.classList.add('hidden');
    loggedIn.classList.add('hidden');
  }
}

function showMessage(elementId, message, isError = false) {
  const element = document.getElementById(elementId);
  element.textContent = message;
  element.className = 'status ' + (isError ? 'error' : 'success');
}

// Registration
document.getElementById('register-btn').addEventListener('click', async () => {
  const username = document.getElementById('register-username').value;
  const email = document.getElementById('register-email').value;
  const password = document.getElementById('register-password').value;

  if (!username || !email || !password) {
    showMessage('auth-status', 'Please fill in all fields', true);
    return;
  }

  try {
    const resp = await fetch('https://localhost:8081/auth/register', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username, email, password })
    });

    const data = await resp.json();

    if (resp.ok) {
      showMessage('auth-status', 'Registration successful! Please check your email to confirm your account.', false);
      // Clear form
      document.getElementById('register-username').value = '';
      document.getElementById('register-email').value = '';
      document.getElementById('register-password').value = '';
      // Show login form
      setTimeout(() => {
        document.getElementById('show-login').click();
      }, 2000);
    } else {
      showMessage('auth-status', `Registration failed: ${data.message || 'Unknown error'}`, true);
    }
  } catch (err) {
    showMessage('auth-status', `Error: ${err.message}`, true);
  }
});

// Login
document.getElementById('login-btn').addEventListener('click', async () => {
  const loginUsername = document.getElementById('login-username').value;
  const password = document.getElementById('login-password').value;

  if (!loginUsername || !password) {
    showMessage('auth-status', 'Please enter username and password', true);
    return;
  }

  try {
    const resp = await fetch('https://localhost:8081/auth/login', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username: loginUsername, password })
    });

    const data = await resp.json();

    if (resp.ok && data.session_token) {
      sessionToken = data.session_token;
      username = loginUsername;

      // Store in cookies
      setCookie('session_token', sessionToken);
      setCookie('username', username);

      showMessage('auth-status', 'Login successful!', false);

      // Clear password
      document.getElementById('login-password').value = '';

      // Update UI
      updateAuthUI();
    } else {
      showMessage('auth-status', `Login failed: ${data.message || 'Unknown error'}`, true);
    }
  } catch (err) {
    showMessage('auth-status', `Error: ${err.message}`, true);
  }
});

// Logout
document.getElementById('logout-btn').addEventListener('click', () => {
  deleteCookie('session_token');
  deleteCookie('username');
  sessionToken = null;
  username = null;
  updateAuthUI();
  showMessage('auth-status', 'Logged out successfully', false);
});

// Toggle between login and register forms
document.getElementById('show-register').addEventListener('click', () => {
  document.getElementById('login-form').classList.add('hidden');
  document.getElementById('register-form').classList.remove('hidden');
  showMessage('auth-status', '', false);
});

document.getElementById('show-login').addEventListener('click', () => {
  document.getElementById('register-form').classList.add('hidden');
  document.getElementById('login-form').classList.remove('hidden');
  showMessage('auth-status', '', false);
});

// Initialize auth UI on page load
updateAuthUI();

// World generation (existing functionality)
document.getElementById('gen').addEventListener('click', async () => {
  const seed = document.getElementById('seed').value;
  const width = document.getElementById('width').value;
  const height = document.getElementById('height').value;

  // Create a protobuf request message
  const request = new world_pb.WorldRequest();
  request.setSeed(parseInt(seed));
  request.setWidth(parseInt(width));
  request.setHeight(parseInt(height));

  // Build URL with session token if available
  let url = `https://localhost:8080/world?seed=${encodeURIComponent(seed)}&width=${encodeURIComponent(width)}&height=${encodeURIComponent(height)}`;
  if (sessionToken) {
    url += `&session_token=${encodeURIComponent(sessionToken)}`;
  }

  try {
    const resp = await fetch(url);
    if (!resp.ok) throw new Error('Network response was not ok');
    const text = await resp.text();

    // Create a protobuf response (for validation/structure)
    const response = new world_pb.WorldResponse();
    response.setMap(text);

    document.getElementById('map').textContent = response.getMap();
  } catch (err) {
    document.getElementById('map').textContent = 'Error fetching /world: ' + err + '\n\n' + 'As a fallback, run the CLI generator and save to client/world.txt, then click "Load client/world.txt".';
  }
});

// Load fallback local file saved by the CLI generator
document.getElementById('loadfile').addEventListener('click', async () => {
  try {
    const resp = await fetch('world.txt');
    if (!resp.ok) throw new Error('Not found');
    const text = await resp.text();
    document.getElementById('map').textContent = text;
  } catch (err) {
    document.getElementById('map').textContent = 'Error loading client/world.txt: ' + err;
  }
});
