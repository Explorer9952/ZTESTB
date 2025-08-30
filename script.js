async function loadPlaylist() {
  const response = await fetch('playlist.m3u');
  const text = await response.text();
  const lines = text.split('\n').filter(line => line && !line.startsWith('#'));

  const container = document.getElementById('player-container');
  lines.forEach((url, index) => {
    const audio = document.createElement('audio');
    audio.controls = true;
    audio.src = url.trim();
    container.appendChild(audio);
  });
}

loadPlaylist();
