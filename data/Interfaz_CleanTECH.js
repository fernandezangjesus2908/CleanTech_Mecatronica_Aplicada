// ================================================================
// Cliente MJPEG para ESP32-CAM · CleanTECH
// Consume el endpoint /stream y calcula FPS reales en el navegador.
// ================================================================
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
(function () {
    'use strict';
    // ============================================================
    // Helpers de DOM: obtiene un elemento por id con tipado fuerte
    // ============================================================
    const $ = (id) => {
        const el = document.getElementById(id);
        if (!el)
            throw new Error(`Elemento #${id} no encontrado`);
        return el;
    };
    // ============================================================
    // Referencias a los elementos de la interfaz
    // ============================================================
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
    // ============================================================
    // Utilidades binarias para parsear el multipart MJPEG
    // ============================================================
    const enc = new TextEncoder();
    const HDR_SEP = enc.encode('\r\n\r\n');
    const BOUNDARY = enc.encode('--123456789000000000000987654321');
    //Busca la primera aparicion de needle dentro de haystack a partir de from
    function findSubarray(haystack, needle, from = 0) {
        const max = haystack.length - needle.length;
        outer: for (let i = from; i <= max; i++) {
            for (let j = 0; j < needle.length; j++) {
                if (haystack[i + j] !== needle[j])
                    continue outer;
            }
            return i;
        }
        return -1;
    }
    // ============================================================
    // Refresco del estado visual (dot, textos y botones)
    // ============================================================
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
        btnConnect.textContent = connected ? 'Desconectar' : 'Conectar';
        btnConnect.classList.toggle('active', connected);
        btnPause.disabled = !connected;
        btnSnapshot.disabled = !(state === 'streaming' || state === 'paused');
        btnFull.disabled = !connected;
        btnPause.textContent = state === 'paused' ? 'Reanudar' : 'Pausar';
        valState.textContent = labels[state];
        //Si no hay conexion se limpian las metricas
        if (!connected) {
            valRes.textContent = '—';
            valFps.textContent = '—';
            valSize.textContent = '—';
            infoRes.textContent = '—';
            infoFps.textContent = '0.0 FPS';
        }
    }
    // ============================================================
    // Contador de FPS reales basado en frames decodificados
    // ============================================================
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
    // ============================================================
    // Render del frame recibido y deteccion de resolucion
    // ============================================================
    function displayFrame(jpegBytes) {
        if (currentBlobUrl)
            URL.revokeObjectURL(currentBlobUrl);
        //Copia a un Uint8Array<ArrayBuffer> nuevo para satisfacer BlobPart
        //(TS 5.7+ distingue entre ArrayBuffer y SharedArrayBuffer)
        const blob = new Blob([new Uint8Array(jpegBytes)], { type: 'image/jpeg' });
        currentBlobUrl = URL.createObjectURL(blob);
        streamEl.src = currentBlobUrl;
        frameCount++;
        lastFrame = { bytes: jpegBytes };
        //Al cargar el primer frame se lee su resolucion natural
        streamEl.onload = () => {
            if (!lastFrame || lastFrame.w)
                return;
            const w = streamEl.naturalWidth;
            const h = streamEl.naturalHeight;
            lastFrame.w = w;
            lastFrame.h = h;
            valSize.textContent = w + ' × ' + h;
            valRes.textContent = w + '×' + h;
            infoRes.textContent = w + '×' + h;
        };
    }
    // ============================================================
    // Apertura del stream MJPEG con fetch + parser incremental
    // ============================================================
    function connect() {
        return __awaiter(this, void 0, void 0, function* () {
            if (state === 'streaming' || state === 'connecting')
                return;
            state = 'connecting';
            updateUI();
            abortCtrl = new AbortController();
            try {
                yield runMjpegStream(abortCtrl.signal);
            }
            catch (err) {
                if (err.name !== 'AbortError') {
                    console.error('[CleanTECH Camera] stream error:', err);
                    disconnect();
                }
            }
        });
    }
    function runMjpegStream(signal) {
        return __awaiter(this, void 0, void 0, function* () {
            const res = yield fetch('/stream', { signal, cache: 'no-store' });
            if (!res.ok)
                throw new Error('HTTP ' + res.status);
            if (!res.body)
                throw new Error('Respuesta sin body');
            //Boundary real anunciado por el firmware en el Content-Type
            const ct = res.headers.get('content-type') || '';
            const bm = ct.match(/boundary=(.+)/i);
            const boundaryBytes = bm
                ? enc.encode('--' + bm[1].trim().replace(/^"|"$/g, ''))
                : BOUNDARY;
            const reader = res.body.getReader();
            let buffer = new Uint8Array(0);
            let firstFrame = true;
            while (true) {
                const chunk = yield reader.read();
                if (chunk.done)
                    break;
                //Acumula el chunk en el buffer pendiente
                const merged = new Uint8Array(buffer.length + chunk.value.length);
                merged.set(buffer, 0);
                merged.set(chunk.value, buffer.length);
                buffer = merged;
                //Extrae todos los frames completos disponibles
                while (true) {
                    const startIdx = findSubarray(buffer, boundaryBytes, 0);
                    if (startIdx === -1)
                        break;
                    const searchFrom = startIdx + boundaryBytes.length;
                    const nextIdx = findSubarray(buffer, boundaryBytes, searchFrom);
                    if (nextIdx === -1)
                        break;
                    const part = buffer.subarray(searchFrom, nextIdx);
                    //Salta el CRLF inicial del multipart
                    let pStart = 0;
                    if (part[0] === 13 && part[1] === 10)
                        pStart = 2;
                    const hEnd = findSubarray(part, HDR_SEP, pStart);
                    if (hEnd !== -1) {
                        const headerText = new TextDecoder().decode(part.subarray(pStart, hEnd));
                        const lenMatch = headerText.match(/Content-Length:\s*(\d+)/i);
                        const bodyStart = hEnd + 4;
                        let bodyEnd;
                        if (lenMatch) {
                            bodyEnd = bodyStart + parseInt(lenMatch[1], 10);
                        }
                        else {
                            bodyEnd = part.length;
                            while (bodyEnd >= 2 && part[bodyEnd - 2] === 13 && part[bodyEnd - 1] === 10) {
                                bodyEnd -= 2;
                            }
                        }
                        if (bodyEnd > bodyStart) {
                            displayFrame(part.slice(bodyStart, bodyEnd));
                            //Primer frame => confirma estado streaming y arranca FPS
                            if (firstFrame) {
                                firstFrame = false;
                                state = 'streaming';
                                updateUI();
                                startFpsCounter();
                            }
                        }
                    }
                    buffer = buffer.subarray(nextIdx);
                }
            }
        });
    }
    // ============================================================
    // Cierre ordenado del stream y liberacion de recursos
    // ============================================================
    function disconnect() {
        if (abortCtrl) {
            abortCtrl.abort();
            abortCtrl = null;
        }
        stopFpsCounter();
        if (currentBlobUrl) {
            URL.revokeObjectURL(currentBlobUrl);
            currentBlobUrl = null;
        }
        streamEl.removeAttribute('src');
        lastFrame = null;
        state = 'disconnected';
        updateUI();
    }
    // ============================================================
    // Manejadores de los botones de la toolbar
    // ============================================================
    //Conectar / desconectar segun el estado actual
    btnConnect.addEventListener('click', () => {
        if (state === 'streaming' || state === 'paused' || state === 'connecting') {
            disconnect();
        }
        else {
            void connect();
        }
    });
    //Pausar: congela el ultimo frame y libera la conexion MJPEG
    btnPause.addEventListener('click', () => {
        if (state === 'streaming') {
            if (abortCtrl) {
                abortCtrl.abort();
                abortCtrl = null;
            }
            stopFpsCounter();
            state = 'paused';
            updateUI();
        }
        else if (state === 'paused') {
            void connect();
        }
    });
    //Snapshot: descarga el frame actual como PNG
    btnSnapshot.addEventListener('click', () => {
        if (!lastFrame || !streamEl.naturalWidth)
            return;
        canvasEl.width = streamEl.naturalWidth;
        canvasEl.height = streamEl.naturalHeight;
        const ctx = canvasEl.getContext('2d');
        if (!ctx)
            return;
        ctx.drawImage(streamEl, 0, 0);
        canvasEl.toBlob((blob) => {
            if (!blob)
                return;
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
    //Pantalla completa sobre el visor
    btnFull.addEventListener('click', () => {
        const wrap = document.getElementById('scopeWrap');
        if (!wrap)
            return;
        if (!document.fullscreenElement) {
            const fn = wrap.requestFullscreen || wrap.webkitRequestFullscreen;
            if (fn)
                void fn.call(wrap);
        }
        else {
            const fn = document.exitFullscreen || document.webkitExitFullscreen;
            if (fn)
                void fn.call(document);
        }
    });
    // ============================================================
    // Inicializacion de la interfaz
    // ============================================================
    updateUI();
})();
