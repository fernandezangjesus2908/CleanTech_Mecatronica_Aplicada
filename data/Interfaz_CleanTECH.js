// ================================================================
// Cliente MJPEG para ESP32-CAM · CleanTECH (Parser + IA Overlay)
// ================================================================
(function () {
    'use strict';
    const $ = (id) => {
        const el = document.getElementById(id);
        if (!el) throw new Error(`Elemento #${id} no encontrado`);
        return el;
    };
    const streamEl = $('stream');
    const canvasEl = $('canvas');
    const overlay = $('overlay');
    const dot = $('dot');
    const statusText = $('statusText');
    const btnConnect = $('btnConnect');
    const btnPause = $('btnPause');
    const btnSnapshot = $('btnSnapshot');
    const btnFull = $('btnFullscreen');
    const valState = $('valState');
    const valRes = $('valRes');
    const valFps = $('valFps');
    const valSize = $('valSize');
    const infoRes = $('infoRes');
    const infoFps = $('infoFps');
    
    let state = 'disconnected';
    let abortCtrl = null;
    let currentBlobUrl = null;
    let frameCount = 0;
    let fpsInterval = null;
    let lastFpsTime = 0;
    let lastFrame = null;
    let drawInterval = null; // Intervalo para el Canvas

    const enc = new TextEncoder();
    const HDR_SEP = enc.encode('\r\n\r\n');
    const BOUNDARY = enc.encode('--123456789000000000000987654321');

    function findSubarray(haystack, needle, from = 0) {
        const max = haystack.length - needle.length;
        outer: for (let i = from; i <= max; i++) {
            for (let j = 0; j < needle.length; j++) {
                if (haystack[i + j] !== needle[j]) continue outer;
            }
            return i;
        }
        return -1;
    }

    // --- LÓGICA PARA DIBUJAR EL PUNTO DE LA IA ---
    function startDrawing() {
        if (drawInterval) clearInterval(drawInterval);
        drawInterval = setInterval(async () => {
            if (state !== 'streaming') {
                const ctx = canvasEl.getContext('2d');
                ctx.clearRect(0, 0, canvasEl.width, canvasEl.height);
                return;
            }

            try {
                const res = await fetch('/coords', { cache: 'no-store' });
                const data = await res.json();
                
                // Sincronizar el tamaño del canvas con la imagen
                if (canvasEl.width !== streamEl.clientWidth || canvasEl.height !== streamEl.clientHeight) {
                    canvasEl.width = streamEl.clientWidth;
                    canvasEl.height = streamEl.clientHeight;
                }

                const ctx = canvasEl.getContext('2d');
                ctx.clearRect(0, 0, canvasEl.width, canvasEl.height);

                if (data.detectado) {
                    // Convertir coordenadas relativas (0 a 1) a píxeles reales
                    const px = data.x * canvasEl.width;
                    const py = data.y * canvasEl.height;

                    // Dibujar un círculo de mira
                    ctx.beginPath();
                    ctx.arc(px, py, 12, 0, 2 * Math.PI);
                    ctx.strokeStyle = '#00FF00'; // Verde brillante
                    ctx.lineWidth = 3;
                    ctx.stroke();

                    // Dibujar cruz central
                    ctx.beginPath();
                    ctx.moveTo(px - 15, py); ctx.lineTo(px + 15, py);
                    ctx.moveTo(px, py - 15); ctx.lineTo(px, py + 15);
                    ctx.strokeStyle = '#FF0000'; // Rojo
                    ctx.lineWidth = 2;
                    ctx.stroke();
                }
            } catch (err) {
                // Ignorar errores de fetch (la cámara puede estar ocupada)
            }
        }, 200); // Se actualiza 5 veces por segundo
    }

    function updateUI() {
        const connected = state === 'streaming' || state === 'paused';
        const labels = {
            disconnected: 'Desconectado',
            connecting: 'Conectando...',
            streaming: 'Transmitiendo',
            paused: 'Pausado'
        };
        statusText.textContent = labels[state];
        dot.classList.toggle('on', state === 'streaming');
        dot.classList.toggle('warn', state === 'paused' || state === 'connecting');
        overlay.classList.toggle('hidden', connected);
        streamEl.classList.toggle('on', connected);
        
        if (state === 'connecting') {
            btnConnect.textContent = 'Conectando...';
            btnConnect.disabled = true;
        } else {
            btnConnect.textContent = connected ? 'Desconectar' : 'Conectar';
            btnConnect.disabled = false;
        }
        
        btnConnect.classList.toggle('active', connected);
        btnPause.disabled = !connected;
        btnSnapshot.disabled = !(state === 'streaming' || state === 'paused');
        btnFull.disabled = !connected;
        btnPause.textContent = state === 'paused' ? 'Reanudar' : 'Pausar';
        valState.textContent = labels[state];

        if (!connected) {
            valRes.textContent = '—';
            valFps.textContent = '—';
            valSize.textContent = '—';
            infoRes.textContent = '—';
            infoFps.textContent = '0.0 FPS';
        }
    }

    function startFpsCounter() {
        stopFpsCounter();
        frameCount = 0;
        lastFpsTime = performance.now();
        fpsInterval = window.setInterval(() => {
            const now = performance.now();
            const dt = (now - lastFpsTime) / 1000;
            const fps = dt > 0 ? frameCount / dt : 0;
            valFps.textContent = fps.toFixed(1);
            infoFps.textContent = fps.toFixed(1) + ' FPS';
            frameCount = 0;
            lastFpsTime = now;
        }, 1000);
    }

    function stopFpsCounter() {
        if (fpsInterval !== null) {
            clearInterval(fpsInterval);
            fpsInterval = null;
        }
        valFps.textContent = '—';
        infoFps.textContent = '0.0 FPS';
    }

    function displayFrame(jpegBytes) {
        if (currentBlobUrl) URL.revokeObjectURL(currentBlobUrl);
        const blob = new Blob([new Uint8Array(jpegBytes)], { type: 'image/jpeg' });
        currentBlobUrl = URL.createObjectURL(blob);
        streamEl.src = currentBlobUrl;
        frameCount++;
        lastFrame = { bytes: jpegBytes };
        
        streamEl.onload = () => {
            if (!lastFrame || lastFrame.w) return;
            const w = streamEl.naturalWidth;
            const h = streamEl.naturalHeight;
            lastFrame.w = w;
            lastFrame.h = h;
            valSize.textContent = w + ' × ' + h;
            valRes.textContent = w + '×' + h;
            infoRes.textContent = w + '×' + h;
        };
    }

    async function connect() {
        if (state === 'streaming' || state === 'connecting') return;
        state = 'connecting';
        updateUI();
        abortCtrl = new AbortController();
        try {
            await runMjpegStream(abortCtrl.signal);
        } catch (err) {
            if (err.name !== 'AbortError') {
                console.error('[CleanTECH Camera] stream error:', err);
                disconnect();
            }
        }
    }

    async function runMjpegStream(signal) {
        const res = await fetch('/stream', { signal, cache: 'no-store' });
        if (!res.ok) throw new Error('HTTP ' + res.status);
        if (!res.body) throw new Error('Respuesta sin body');

        const ct = res.headers.get('content-type') || '';
        const bm = ct.match(/boundary=(.+)/i);
        const boundaryBytes = bm ? enc.encode('--' + bm[1].trim().replace(/^"|"$/g, '')) : BOUNDARY;
        
        const reader = res.body.getReader();
        let buffer = new Uint8Array(0);
        let firstFrame = true;

        while (true) {
            const chunk = await reader.read();
            if (chunk.done) break;

            const merged = new Uint8Array(buffer.length + chunk.value.length);
            merged.set(buffer, 0);
            merged.set(chunk.value, buffer.length);
            buffer = merged;

            let boundaryIdx = findSubarray(buffer, boundaryBytes);
            if (boundaryIdx === -1) continue; 

            const headerEndIdx = findSubarray(buffer, HDR_SEP, boundaryIdx);
            if (headerEndIdx === -1) continue; 

            const headerText = new TextDecoder().decode(buffer.subarray(boundaryIdx, headerEndIdx));
            const lenMatch = headerText.match(/Content-Length:\s*(\d+)/i);
            
            if (!lenMatch) {
                buffer = buffer.subarray(boundaryIdx + boundaryBytes.length);
                continue;
            }

            const contentLength = parseInt(lenMatch[1], 10);
            const frameStartIdx = headerEndIdx + 4;
            const frameEndIdx = frameStartIdx + contentLength;

            if (buffer.length < frameEndIdx) continue;

            const jpegBytes = buffer.slice(frameStartIdx, frameEndIdx);
            displayFrame(jpegBytes);

            buffer = buffer.subarray(frameEndIdx);

            if (firstFrame) {
                firstFrame = false;
                state = 'streaming';
                updateUI();
                startFpsCounter();
                startDrawing(); // <--- INICIAMOS EL DIBUJADO AQUÍ
            }
        }
    }

    function disconnect() {
        if (abortCtrl) {
            abortCtrl.abort();
            abortCtrl = null;
        }
        stopFpsCounter();
        if (drawInterval) {
            clearInterval(drawInterval);
            drawInterval = null;
        }
        if (currentBlobUrl) {
            URL.revokeObjectURL(currentBlobUrl);
            currentBlobUrl = null;
        }
        streamEl.removeAttribute('src');
        lastFrame = null;
        state = 'disconnected';
        updateUI();
    }

    btnConnect.addEventListener('click', () => {
        if (state === 'streaming' || state === 'paused') {
            disconnect();
        } else if (state === 'disconnected') {
            void connect();
        }
    });

    btnPause.addEventListener('click', () => {
        if (state === 'streaming') {
            if (abortCtrl) { abortCtrl.abort(); abortCtrl = null; }
            stopFpsCounter();
            state = 'paused';
            updateUI();
        } else if (state === 'paused') {
            void connect();
        }
    });

    btnSnapshot.addEventListener('click', () => {
        if (!lastFrame || !streamEl.naturalWidth) return;
        canvasEl.width = streamEl.naturalWidth;
        canvasEl.height = streamEl.naturalHeight;
        const ctx = canvasEl.getContext('2d');
        if (!ctx) return;
        ctx.drawImage(streamEl, 0, 0);
        canvasEl.toBlob((blob) => {
            if (!blob) return;
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            const ts = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
            a.href = url;
            a.download = 'cleantech-cam-' + ts + '.png';
            document.body.appendChild(a);
            a.click();
            a.remove();
            setTimeout(() => URL.revokeObjectURL(url), 2000);
        }, 'image/png');
    });

    btnFull.addEventListener('click', () => {
        const wrap = document.getElementById('scopeWrap');
        if (!wrap) return;
        if (!document.fullscreenElement) {
            const fn = wrap.requestFullscreen || wrap.webkitRequestFullscreen;
            if (fn) void fn.call(wrap);
        } else {
            const fn = document.exitFullscreen || document.webkitExitFullscreen;
            if (fn) void fn.call(document);
        }
    });

    updateUI();
})();