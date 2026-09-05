const statusElement = document.getElementById('firmware-status');
const fileElement = document.getElementById('firmware-file');
const uploadButton = document.getElementById('firmware-upload-btn');
const installButton = document.getElementById('firmware-install-btn');
const cancelButton = document.getElementById('firmware-cancel-btn');
const progressWrap = document.getElementById('firmware-progress-wrap');
const progressBar = document.getElementById('firmware-progress-bar');
const progressText = document.getElementById('firmware-progress-text');

let currentStatus = null;

function setStatus(message, isError = false) {
  statusElement.textContent = message;
  statusElement.style.color = isError ? '#b91c1c' : '';
}

function describeStatus(data) {
  const state = data.state || 'unknown';
  const device = data.device || 'device';
  const version = data.version || 'unknown version';
  if (data.error) return `${device} — ${state}\n${data.error}`;
  if (state === 'ready') return `${device} — ${version}\nImage validated and ready to install (${data.size || 0} bytes).`;
  if (state === 'install_requested') return 'Installation queued. The device will reboot shortly.';
  if (state === 'installing') return `Installing ${device}… ${data.received || 0} / ${data.size || 0} bytes`;
  if (state === 'rebooting') return 'Firmware written successfully. Waiting for the device to reboot…';
  if (state === 'completed') return `${device} — ${version}\nThe last browser firmware update completed.`;
  if (state === 'interrupted') return `${device}\nThe previous installation was interrupted. The active slot was retained.`;
  if (state === 'failed') return `${device}\nFirmware update failed.`;
  return `${device} — ${version}\nNo staged firmware image.`;
}

function renderStatus(data) {
  currentStatus = data;
  setStatus(describeStatus(data), Boolean(data.error));
  const state = data.state || 'idle';
  const ready = state === 'ready';
  const busy = ['uploading', 'install_requested', 'installing', 'rebooting'].includes(state);
  installButton.disabled = !ready || busy;
  cancelButton.disabled = busy || (!ready && state !== 'failed' && state !== 'interrupted');
  uploadButton.disabled = busy;
  if (state === 'installing' && data.size) {
    progressWrap.hidden = false;
    const percent = Math.min(100, Math.round(((data.received || 0) * 100) / data.size));
    progressBar.style.width = `${percent}%`;
    progressText.textContent = `${percent}%`;
  }
}

async function refreshStatus() {
  try {
    const response = await fetch('/api/firmware/status?_=' + Date.now());
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    renderStatus(await response.json());
  } catch (error) {
    setStatus(`Device is restarting or unavailable: ${error.message}`, true);
  }
}

function uploadFirmware(file) {
  return new Promise((resolve, reject) => {
    const form = new FormData();
    form.append('file', file, file.name);
    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/firmware/upload', true);
    xhr.upload.onprogress = (event) => {
      if (!event.lengthComputable) return;
      progressWrap.hidden = false;
      const percent = Math.round((event.loaded * 100) / event.total);
      progressBar.style.width = `${percent}%`;
      progressText.textContent = `Uploading — ${percent}%`;
    };
    xhr.onload = () => {
      let data;
      try { data = JSON.parse(xhr.responseText); } catch (_) { data = { error: xhr.responseText }; }
      if (xhr.status >= 200 && xhr.status < 300) resolve(data);
      else reject(new Error(data.error || `HTTP ${xhr.status}`));
    };
    xhr.onerror = () => reject(new Error('Network error during firmware upload'));
    xhr.onabort = () => reject(new Error('Firmware upload cancelled'));
    xhr.send(form);
  });
}

uploadButton.addEventListener('click', async () => {
  const file = fileElement.files[0];
  if (!file) return setStatus('Choose a firmware .bin file first.', true);
  if (!file.name.toLowerCase().endsWith('.bin')) return setStatus('The firmware file must end in .bin.', true);
  uploadButton.disabled = true;
  try {
    renderStatus(await uploadFirmware(file));
  } catch (error) {
    setStatus(error.message, true);
    uploadButton.disabled = false;
  }
});

installButton.addEventListener('click', async () => {
  if (!currentStatus || currentStatus.state !== 'ready') return;
  if (!window.confirm('Install the validated firmware and reboot the device now?')) return;
  installButton.disabled = true;
  try {
    renderStatus(await (await fetch('/api/firmware/install', { method: 'POST' })).json());
    setStatus('Installation queued. Keep the device powered while it reboots.');
  } catch (error) {
    setStatus(error.message, true);
  }
});

cancelButton.addEventListener('click', async () => {
  if (!window.confirm('Discard the staged firmware image?')) return;
  try {
    renderStatus(await (await fetch('/api/firmware/cancel', { method: 'POST' })).json());
    progressWrap.hidden = true;
  } catch (error) {
    setStatus(error.message, true);
  }
});

refreshStatus();
setInterval(refreshStatus, 2000);
