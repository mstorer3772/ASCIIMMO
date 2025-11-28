document.getElementById('gen').addEventListener('click', async () => {
  const seed = document.getElementById('seed').value;
  const width = document.getElementById('width').value;
  const height = document.getElementById('height').value;
  const url = `/world?seed=${encodeURIComponent(seed)}&width=${encodeURIComponent(width)}&height=${encodeURIComponent(height)}`;
  try {
    const resp = await fetch(url);
    if (!resp.ok) throw new Error('Network response was not ok');
    const text = await resp.text();
    document.getElementById('map').textContent = text;
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