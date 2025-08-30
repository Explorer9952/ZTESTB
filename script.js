async function loadPlaylist() {
  const response = await fetch('playlist.txt');
  const text = await response.text();
  const lines = text.split('\n');

  const container = document.getElementById('player-container');
  for (let i = 0; i < lines.length; i++) {
    if (lines[i].startsWith('#EXTINF')) {
      const title = lines[i].split(',')[1]?.trim() || 'Канал';
      const url = lines[i + 1]?.trim();

      const wrapper = document.createElement('div');
      const label = document.createElement('h3');
      label.textContent = title;

      const video = document.createElement('video');
      video.controls = true;
      video.width = 640;

      if (Hls.isSupported()) {
        const hls = new Hls();
        hls.loadSource(url);
        hls.attachMedia(video);
      } else if (video.canPlayType('application/vnd.apple.mpegurl')) {
        video.src = url;
      }

      wrapper.appendChild(label);
      wrapper.appendChild(video);
      container.appendChild(wrapper);
    }
  }
}

loadPlaylist();
