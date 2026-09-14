const $ = id => document.getElementById(id), canvas = $('view'), ctx = canvas.getContext('2d');
const original = { lo: lo.slice(), hi: hi.slice() }; let bounds = { lo: lo.slice(), hi: hi.slice() }, positions = new Float32Array(), segments = new Float32Array();
let drag = null, worker = null, workerURL = null, job = 0, queued = false, timer = null;
$('equation').textContent = formatExpression(model.lhs, names) + ' = ' + formatExpression(model.rhs, names);
$('kind-label').textContent = graphKind === 'numberline' ? 'Root number line' : '2D curve';
const format = n => Number(n.toPrecision(6)).toLocaleString('en-US', { maximumFractionDigits: 6 });
function tickStep(span, pixels) { const raw = span / Math.max(2, pixels / 85), power = 10 ** Math.floor(Math.log10(raw)); return [1, 2, 5, 10].find(n => n * power >= raw) * power; }
function draw() {
  queued = false; const w = canvas.clientWidth, h = canvas.clientHeight, dpr = Math.min(2, devicePixelRatio || 1, Math.sqrt(2400000 / Math.max(1, w * h))); if (!w || !h) return;
  const width = Math.round(w * dpr), height = Math.round(h * dpr); if (canvas.width !== width || canvas.height !== height) { canvas.width = width; canvas.height = height; }
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0); ctx.clearRect(0, 0, w, h);
  const sx = w / (bounds.hi[0] - bounds.lo[0]), sy = h / (bounds.hi[1] - bounds.lo[1]), px = x => (x - bounds.lo[0]) * sx, py = y => h - (y - bounds.lo[1]) * sy;
  const axisX = Math.max(0, Math.min(w, px(0))), axisY = graphKind === 'numberline' ? h * .5 : Math.max(0, Math.min(h, py(0)));
  ctx.font = '11px -apple-system,sans-serif';
  for (let a = 0; a < 2; a++) {
    if (a === 1 && graphKind === 'numberline') continue;
    const step = tickStep(bounds.hi[a] - bounds.lo[a], a ? h : w), start = Math.ceil(bounds.lo[a] / step) * step;
    const isLight = document.documentElement.dataset.theme === 'light';
    for (let i = 0; i < 100; i++) {
      const v = start + i * step; if (v > bounds.hi[a]) break; const q = a ? py(v) : px(v);
      if ($('grid').checked) { ctx.strokeStyle = isLight ? '#0f172a15' : '#52678425'; ctx.lineWidth = .7; ctx.beginPath(); ctx.moveTo(a ? 0 : q, a ? q : 0); ctx.lineTo(a ? w : q, a ? q : h); ctx.stroke(); }
      if ($('axes').checked) { ctx.fillStyle = isLight ? '#334155' : '#8ca0bc'; if (a) ctx.fillText(format(v), Math.max(8, Math.min(w - 65, axisX + 8)), q - 5); else ctx.fillText(format(v), q + 5, Math.max(20, Math.min(h - 12, axisY + 17))); }
    }
  }
  const isLight = document.documentElement.dataset.theme === 'light';
  if ($('axes').checked) { ctx.strokeStyle = isLight ? '#475569aa' : '#93afce88'; ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(0, axisY); ctx.lineTo(w, axisY); if (graphKind !== 'numberline') { ctx.moveTo(axisX, 0); ctx.lineTo(axisX, h); } ctx.stroke(); ctx.fillStyle = isLight ? '#0f172a' : '#9ccddd'; ctx.fillText(names[0], w - 20, Math.max(20, Math.min(h - 15, axisY - 10))); if (graphKind !== 'numberline') ctx.fillText(names[1], Math.max(10, Math.min(w - 20, axisX + 10)), 20); }
  const lineWidth = Number($('width').value);
  const curveColor = isLight ? '#dc2626' : '#79dae0';
  ctx.strokeStyle = curveColor; ctx.fillStyle = curveColor; ctx.lineWidth = lineWidth;
  if (segments.length) { ctx.beginPath(); for (let i = 0; i < segments.length; i += 4) { ctx.moveTo(px(segments[i]), py(segments[i + 1])); ctx.lineTo(px(segments[i + 2]), py(segments[i + 3])); } ctx.stroke(); }
  else if (graphKind === 'numberline') { ctx.beginPath(); for (let i = 0; i < positions.length; i += 2) { const x = px(positions[i]); ctx.moveTo(x + 4, axisY); ctx.arc(x, axisY, 4, 0, 2 * Math.PI); ctx.fillText(format(positions[i]), x + 6, axisY - 12); } ctx.fill(); }
  else for (let i = 0; i < positions.length; i += 2)ctx.fillRect(px(positions[i]) - lineWidth / 2, py(positions[i + 1]) - lineWidth / 2, lineWidth, lineWidth);
}
function schedule() { if (!queued) { queued = true; requestAnimationFrame(draw); } }
function setFields() { for (let a = 0; a < 2; a++) { $('min-' + a).value = bounds.lo[a]; $('max-' + a).value = bounds.hi[a]; } $('bounds-label').textContent = names.slice(0, graphKind === 'numberline' ? 1 : 2).map((n, a) => n + ' ' + format(bounds.lo[a]) + '…' + format(bounds.hi[a])).join(' · '); }
for (let a = 0; a < 2; a++) { const row = document.createElement('div'); row.className = 'axis-control axis-' + a; row.style.marginBottom = '15px'; row.innerHTML = `<div class="axis-inputs"><span>${names[a]}</span><input id="min-${a}" type="number" step="any" required aria-label="${names[a]} minimum"><input id="max-${a}" type="number" step="any" required aria-label="${names[a]} maximum"></div>`; $('range-fields').appendChild(row); if (a === 1 && graphKind === 'numberline') row.hidden = true; }
function cleanup() { if (worker) worker.terminate(); if (workerURL) URL.revokeObjectURL(workerURL); worker = null; workerURL = null; }
function resample() {
  clearTimeout(timer); cleanup(); const id = ++job, start = performance.now(); $('range-error').textContent = ''; $('graph-status').textContent = 'Calculating…'; $('empty').hidden = true;
  try {
    const code = `const compileExpression=${compileExpression.toString()},usesCoordinate=${usesCoordinate.toString()},scanRoots=${scanRoots.toString()},sampleCurve=${sampleCurve.toString()};onmessage=e=>{try{const result=sampleCurve(e.data);postMessage(result,[result.positions.buffer,result.segments.buffer]);}catch(error){postMessage({error:String(error)});}};`;
    workerURL = URL.createObjectURL(new Blob([code], { type: 'text/javascript' })); worker = new Worker(workerURL);
    const fail = () => { if (id !== job) return; cleanup(); $('graph-status').textContent = 'Could not calculate'; $('range-error').textContent = 'Try a narrower range or lower detail.'; };
    worker.onerror = fail; worker.onmessage = e => { if (id !== job) return; if (e.data.error) { fail(); return; } cleanup(); positions = e.data.positions; segments = e.data.segments; $('point-count').textContent = (positions.length / 2).toLocaleString() + (graphKind === 'numberline' ? (positions.length === 2 ? ' root' : ' roots') : ' curve points'); $('graph-status').textContent = e.data.capped ? 'Preview · limit reached' : 'Ready'; $('status-message').textContent = 'Curve ready in ' + ((performance.now() - start) / 1000).toFixed(2) + 's. Reload this page after generating the next equation.'; $('empty').hidden = positions.length > 0; schedule(); };
    worker.postMessage({ model, lo: bounds.lo, hi: bounds.hi, kind: graphKind, detail: Number($('detail').value) });
  } catch (error) { cleanup(); $('graph-status').textContent = 'Could not calculate'; $('range-error').textContent = error.message; }
}
function queueSampling() { clearTimeout(timer); timer = setTimeout(resample, 150); }
$('range-form').onsubmit = e => { e.preventDefault(); const next = { lo: bounds.lo.slice(), hi: bounds.hi.slice() }; for (let a = 0; a < 2; a++) { const min = $('min-' + a).value, max = $('max-' + a).value, l = Number(min), h = Number(max); if (!min || !max || !Number.isFinite(l) || !Number.isFinite(h) || h <= l || h - l > 1e6 || Math.max(Math.abs(l), Math.abs(h)) > 1e9) { $('range-error').textContent = 'Enter increasing bounds with a span up to 1,000,000 units and coordinates within ±1 billion.'; return; } next.lo[a] = l; next.hi[a] = h; } bounds = next; setFields(); resample(); schedule(); };
$('range-reset').onclick = $('reset').onclick = () => { bounds = { lo: original.lo.slice(), hi: original.hi.slice() }; setFields(); resample(); schedule(); };
function zoom(factor, x = canvas.clientWidth / 2, y = canvas.clientHeight / 2) { const anchors = [bounds.lo[0] + x / canvas.clientWidth * (bounds.hi[0] - bounds.lo[0]), bounds.hi[1] - y / canvas.clientHeight * (bounds.hi[1] - bounds.lo[1])]; for (let a = 0; a < 2; a++) { const span = (bounds.hi[a] - bounds.lo[a]) * factor; if (span < 1e-8 || span > 1e6) return; } for (let a = 0; a < 2; a++) { bounds.lo[a] = anchors[a] + (bounds.lo[a] - anchors[a]) * factor; bounds.hi[a] = anchors[a] + (bounds.hi[a] - anchors[a]) * factor; } setFields(); schedule(); queueSampling(); }
$('zoom-in').onclick = () => zoom(.8); $('zoom-out').onclick = () => zoom(1.25);
canvas.addEventListener('wheel', e => { e.preventDefault(); const rect = canvas.getBoundingClientRect(); zoom(Math.exp(Math.max(-.3, Math.min(.3, e.deltaY * .001))), e.clientX - rect.left, e.clientY - rect.top); }, { passive: false });
canvas.onpointerdown = e => { drag = { x: e.clientX, y: e.clientY, lo: bounds.lo.slice(), hi: bounds.hi.slice() }; canvas.setPointerCapture(e.pointerId); canvas.focus(); };
canvas.onpointermove = e => { const rect = canvas.getBoundingClientRect(), x = bounds.lo[0] + (e.clientX - rect.left) / rect.width * (bounds.hi[0] - bounds.lo[0]), y = bounds.hi[1] - (e.clientY - rect.top) / rect.height * (bounds.hi[1] - bounds.lo[1]); $('coordinates').textContent = format(x) + ', ' + format(y); if (!drag) return; const delta = [-(e.clientX - drag.x) / rect.width * (drag.hi[0] - drag.lo[0]), (e.clientY - drag.y) / rect.height * (drag.hi[1] - drag.lo[1])]; for (let a = 0; a < 2; a++) { bounds.lo[a] = drag.lo[a] + delta[a]; bounds.hi[a] = drag.hi[a] + delta[a]; } setFields(); schedule(); };
canvas.onpointerup = canvas.onpointercancel = () => { if (drag) { drag = null; resample(); } };
canvas.onkeydown = e => { if (e.key === '+' || e.key === '=') zoom(.8); else if (e.key === '-') zoom(1.25); else if (e.key.startsWith('Arrow')) { const axis = e.key === 'ArrowLeft' || e.key === 'ArrowRight' ? 0 : 1, direction = e.key === 'ArrowLeft' || e.key === 'ArrowDown' ? -1 : 1, delta = (bounds.hi[axis] - bounds.lo[axis]) * .1 * direction; bounds.lo[axis] += delta; bounds.hi[axis] += delta; setFields(); schedule(); queueSampling(); } else return; e.preventDefault(); };
$('grid').onchange = $('axes').onchange = schedule; $('width').oninput = () => { $('width-value').value = $('width').value + ' px'; schedule(); }; $('detail').onchange = resample;
$('fullscreen').onclick = async () => { try { if (document.fullscreenElement) await document.exitFullscreen(); else await $('workspace').requestFullscreen(); } catch (error) { $('status-message').textContent = 'Fullscreen is unavailable in this browser.'; } };
document.addEventListener('fullscreenchange', () => { $('fullscreen').setAttribute('aria-pressed', String(!!document.fullscreenElement)); $('fullscreen').querySelector('span').textContent = document.fullscreenElement ? 'Exit fullscreen' : 'Fullscreen'; schedule(); });
if($('reload')) $('reload').onclick = () => location.reload(); new ResizeObserver(schedule).observe($('viewport')); window.addEventListener('resize', schedule); window.addEventListener('pagehide', () => { clearTimeout(timer); cleanup(); }); setFields(); schedule(); resample();

// ── Theme toggle ──────────────────────────────────────────────────────────
(function initTheme() {
  const saved = localStorage.getItem('mathscript-theme') || 'dark';
  document.documentElement.dataset.theme = saved;
  const btn = $('theme-toggle');
  function applyTheme(theme) {
    document.documentElement.dataset.theme = theme;
    if (btn) {
      btn.title = theme === 'dark' ? 'Switch to light mode' : 'Switch to dark mode';
      btn.checked = theme === 'light';
    }
    localStorage.setItem('mathscript-theme', theme);
    schedule();
  }
  applyTheme(saved);
  if (btn) {
    btn.onclick = () => applyTheme(document.documentElement.dataset.theme === 'dark' ? 'light' : 'dark');
  }
})();

// ── Equation resizer ──────────────────────────────────────────────────────
(function initEquationResizer() {
  const eq = $('equation'), resizer = $('equation-resizer');
  if (!eq || !resizer) return;
  let startY = 0, startH = 0;
  resizer.addEventListener('pointerdown', e => {
    if (e.button !== 0) return;
    startY = e.clientY;
    startH = eq.getBoundingClientRect().height;
    resizer.setPointerCapture(e.pointerId);
    document.body.style.userSelect = 'none';
    document.body.style.cursor = 'ns-resize';
    function onPointerMove(ev) {
      const dy = ev.clientY - startY;
      eq.style.height = Math.max(32, Math.round(startH + dy)) + 'px';
    }
    function onPointerUp(ev) {
      try { resizer.releasePointerCapture(ev.pointerId); } catch (_) { }
      document.body.style.userSelect = '';
      document.body.style.cursor = '';
      window.removeEventListener('pointermove', onPointerMove);
      window.removeEventListener('pointerup', onPointerUp);
      window.removeEventListener('pointercancel', onPointerUp);
    }
    window.addEventListener('pointermove', onPointerMove);
    window.addEventListener('pointerup', onPointerUp);
    window.addEventListener('pointercancel', onPointerUp);
  });
  resizer.addEventListener('dblclick', () => { eq.style.height = 'auto'; });
})();

// ── Sidebar collapse / slide toggle ──────────────────────────────────────
(function initSidebar() {
  const app = $('app') || document.querySelector('.app');
  if (!app) return;
  const toggleBtn = $('sidebar-toggle');
  const closeBtn = $('sidebar-close');
  const edgeBtn = $('sidebar-edge-toggle');

  // Default is 'collapsed'. Only expand if user explicitly opened it and saved 'open'.
  const saved = localStorage.getItem('mathscript-sidebar');
  const shouldBeOpen = saved === 'open';
  if (shouldBeOpen) {
    app.classList.remove('sidebar-collapsed');
    if (toggleBtn) toggleBtn.title = 'Slide panel closed (hide sidebar)';
  } else {
    app.classList.add('sidebar-collapsed');
    if (toggleBtn) toggleBtn.title = 'Slide panel open (show sidebar)';
  }
  if (toggleBtn) toggleBtn.setAttribute('aria-expanded', String(shouldBeOpen));
  if (closeBtn) closeBtn.setAttribute('aria-expanded', String(shouldBeOpen));

  function toggleSidebar(force) {
    const isCollapsed = typeof force === 'boolean' ? force : !app.classList.contains('sidebar-collapsed');
    app.classList.toggle('sidebar-collapsed', isCollapsed);
    localStorage.setItem('mathscript-sidebar', isCollapsed ? 'collapsed' : 'open');
    if (toggleBtn) {
      toggleBtn.title = isCollapsed ? 'Slide panel open (show sidebar)' : 'Slide panel closed (hide sidebar)';
      toggleBtn.setAttribute('aria-expanded', String(!isCollapsed));
    }
    if (closeBtn) closeBtn.setAttribute('aria-expanded', String(!isCollapsed));
    schedule();
    setTimeout(schedule, 150);
    setTimeout(schedule, 320);
  }

  if (toggleBtn) toggleBtn.onclick = () => toggleSidebar();
  if (closeBtn) closeBtn.onclick = () => toggleSidebar(true);
  if (edgeBtn) edgeBtn.onclick = () => toggleSidebar(false);

  window.addEventListener('keydown', e => {
    if (['INPUT', 'SELECT', 'TEXTAREA'].includes(e.target.tagName)) return;
    if (e.key === '[' || e.key === '\\') {
      e.preventDefault();
      toggleSidebar();
    }
  });
})();

// ── Live Auto-Refresh ─────────────────────────────────────────────────────
(function initLiveReload() {
  const loc = window.location || { pathname: '', search: '', protocol: '' };
  const pathname = loc.pathname || '';
  const isLive = pathname.endsWith('MathScriptLiveGraph.html') ||
    pathname.endsWith('MathScriptLiveGraph') ||
    pathname === '/' ||
    (loc.search && loc.search.includes('live=true'));

  const liveIndicator = $('live-indicator');
  if (!isLive) {
    if (liveIndicator) liveIndicator.style.display = 'none';
    return;
  }
  if (liveIndicator) liveIndicator.style.display = 'inline-flex';

  const currentVersion = typeof liveVersion !== 'undefined' ? liveVersion : '';
  let reloaded = false;

  function triggerReload() {
    if (reloaded) return;
    reloaded = true;
    const toast = document.createElement('div');
    toast.className = 'live-reload-toast';
    toast.innerHTML = '<span class="live-pulse"></span><span>New graph detected · Refreshing…</span>';
    document.body.appendChild(toast);
    setTimeout(() => {
      window.location.reload();
    }, 120);
  }

  // 1. Hidden iframe watcher for live_ping.html (works on file:// and http:// across all browsers)
  let iframe = null;
  try {
    iframe = document.createElement('iframe');
    iframe.id = 'mathscript-live-watcher';
    iframe.style.display = 'none';
    iframe.style.width = '0';
    iframe.style.height = '0';
    iframe.style.border = '0';
    iframe.setAttribute('aria-hidden', 'true');
    iframe.src = 'live_ping.html';
    document.body.appendChild(iframe);

    window.addEventListener('message', e => {
      if (e && e.data && e.data.mathscript_live) {
        if (currentVersion && e.data.mathscript_live !== currentVersion) {
          triggerReload();
        }
      }
    });

    // Re-ping iframe if tab was in background or throttled
    setInterval(() => {
      if (!reloaded && iframe) {
        try {
          iframe.contentWindow.location.reload();
        } catch (_) {
          iframe.src = 'live_ping.html';
        }
      }
    }, 2500);

    // Immediate check on window focus
    window.addEventListener('focus', () => {
      if (!reloaded && iframe) {
        try {
          iframe.contentWindow.location.reload();
        } catch (_) {
          iframe.src = 'live_ping.html';
        }
      }
    });
  } catch (_) { }

  // 2. Fetch-based polling for HTTP servers
  if (loc.protocol && loc.protocol.startsWith('http')) {
    setInterval(() => {
      if (reloaded) return;
      fetch('live_version.json?t=' + Date.now(), { cache: 'no-store' })
        .then(res => res.json())
        .then(data => {
          if (data && data.version && currentVersion && data.version !== currentVersion) {
            triggerReload();
          }
        })
        .catch(() => { });
    }, 800);
  }
})();
