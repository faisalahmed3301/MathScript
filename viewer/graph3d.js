/* Offline 3D workspace. Graph data and the sampler are embedded by MathScript. */
const $ = id => document.getElementById(id);
const canvas = $('view'), ctx = canvas.getContext('2d'), surface = $('surface');
let bounds = { lo: lo.slice(), hi: hi.slice() }, positions = new Float32Array(), triangles = new Uint32Array(), normals = new Float32Array();
let yaw = .75, pitch = .5, zoom = 1, angle = 0, playing = !matchMedia('(prefers-reduced-motion: reduce)').matches;
let drag = null, queued = false, lastTime = null, worker = null, workerURL = null, job = 0;
let gl = surface.getContext('webgl', { alpha: true, antialias: true, premultipliedAlpha: false });
let heightRange = [-1, 1];
let autoScale = 1.25, frameAverage = 16, frameCount = 0;
let uniforms = {}, attributes = {};
let program, positionBuffer, normalBuffer, indexBuffer, indexExtension, meshAvailable = false;
const palette = t => { const v = Math.max(0, Math.min(1, t)); return [0.20 + v * .38, .32 + v * .55, .85 + v * .02]; };
function shader(type, source) { const s = gl.createShader(type); gl.shaderSource(s, source); gl.compileShader(s); if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(s)); return s; }
if (gl) {
  try {
    program = gl.createProgram();
    gl.attachShader(program, shader(gl.VERTEX_SHADER, `attribute vec3 position;attribute vec3 normal;uniform vec3 center;uniform float span;uniform mat3 rotation;uniform vec2 scale;uniform float pointSize;uniform vec2 heightRange;uniform vec3 colorLow;uniform vec3 colorHigh;varying vec3 color;varying float lighting;void main(){vec3 p=rotation*((position-center)/span);gl_Position=vec4(p.x*scale.x,p.y*scale.y,-p.z*.5,1.);gl_PointSize=pointSize;float h=clamp((position.z-heightRange.x)/max(.00001,heightRange.y-heightRange.x),0.,1.);color=mix(colorLow,colorHigh,h);vec3 n=rotation*normal;lighting=length(normal)>.1?.60+.40*abs(dot(normalize(n),normalize(vec3(-.4,.8,1.)))):1.;}`));
    gl.attachShader(program, shader(gl.FRAGMENT_SHADER, `precision mediump float;varying vec3 color;varying float lighting;uniform float opacity;uniform bool mesh;void main(){if(!mesh&&distance(gl_PointCoord,vec2(.5))>.5)discard;gl_FragColor=vec4(color*lighting,opacity);}`));
    gl.linkProgram(program); if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error('Shader link failed');
    positionBuffer = gl.createBuffer(); normalBuffer = gl.createBuffer(); indexBuffer = gl.createBuffer(); indexExtension = gl.getExtension('OES_element_index_uint');
    for (const name of ['center', 'span', 'rotation', 'scale', 'pointSize', 'heightRange', 'opacity', 'mesh', 'colorLow', 'colorHigh']) uniforms[name] = gl.getUniformLocation(program, name);
    for (const name of ['position', 'normal']) attributes[name] = gl.getAttribLocation(program, name);
  } catch (error) { gl = null; }
}
function updateGeometry() {
  let minHeight = Infinity, maxHeight = -Infinity; for (let i = 2; i < positions.length; i += 3) { minHeight = Math.min(minHeight, positions[i]); maxHeight = Math.max(maxHeight, positions[i]); } heightRange = positions.length ? [minHeight, maxHeight] : [-1, 1];
  if (gl) {
    gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer); gl.bufferData(gl.ARRAY_BUFFER, positions, gl.STATIC_DRAW); gl.bindBuffer(gl.ARRAY_BUFFER, normalBuffer); gl.bufferData(gl.ARRAY_BUFFER, normals, gl.STATIC_DRAW);
    meshAvailable = triangles.length > 0 && !!(indexExtension || positions.length / 3 <= 65535);
    if (meshAvailable) { gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, indexBuffer); gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, indexExtension ? triangles : new Uint16Array(triangles), gl.STATIC_DRAW); }
  }
  $('point-count').textContent = (positions.length / 3).toLocaleString() + ' surface points'; $('empty').hidden = positions.length > 0;
  $('view-state').textContent = triangles.length ? 'Surface mesh' : 'Point surface'; schedule();
}
function format(node) { if (typeof node === 'number') return String(Number(node.toPrecision(7))); if (node[0] === 'var') return names[node[1]]; if (node[0] === 'neg') return '−' + format(node[1]); if (['+', '-', '*', '/', '^', '<', '>', '>=', '<=', '==', '!='].includes(node[0])) return '(' + format(node[1]) + ' ' + node[0] + ' ' + format(node[2]) + ')'; return node[0] + '(' + node.slice(1).map(format).join(', ') + ')'; }
$('equation').textContent = format(model.lhs) + ' = ' + format(model.rhs);
function rotateModel(p, radians) { const [x, y, z] = p, c = Math.cos(radians), s = Math.sin(radians); return rotation.axis === 'x' ? [x, y * c - z * s, y * s + z * c] : rotation.axis === 'y' ? [x * c + z * s, y, -x * s + z * c] : [x * c - y * s, x * s + y * c, z]; }
function viewVector(p) { const [x, worldY, worldZ] = rotateModel(p, angle), y = worldZ, z = -worldY, a = x * Math.cos(yaw) + z * Math.sin(yaw), b = -x * Math.sin(yaw) + z * Math.cos(yaw); return [a, y * Math.cos(pitch) - b * Math.sin(pitch), y * Math.sin(pitch) + b * Math.cos(pitch)]; }
function metrics() { const { lo, hi } = bounds; return { span: Math.max(...hi.map((v, i) => v - lo[i])), center: lo.map((v, i) => v / 2 + hi[i] / 2) }; }
function project(p) { const { span, center } = metrics(); return viewVector(p.map((v, i) => (v - center[i]) / span)); }
function renderScale(w, h) { const desired = $('quality').value === 'auto' ? autoScale : Number($('quality').value); return Math.min(desired, Math.sqrt(2400000 / Math.max(1, w * h))); }
function draw() {
  const w = canvas.clientWidth, h = canvas.clientHeight; if (!w || !h) return;
  const dpr = renderScale(w, h), width = Math.max(1, Math.round(w * dpr)), height = Math.max(1, Math.round(h * dpr));
  for (const c of [canvas, surface]) if (c.width !== width || c.height !== height) { c.width = width; c.height = height; }
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0); ctx.clearRect(0, 0, w, h);
  const s = Math.min(w, h) * .72 * zoom, origin = [w * .48, h * .47], xy = p => [origin[0] + p[0] * s, origin[1] - p[1] * s];
  const isLight = document.documentElement.dataset.theme === 'light';
  // The same orthographic camera drives the GPU surface and the overlay.
  if (gl) {
    const { span, center } = metrics(); gl.viewport(0, 0, width, height); gl.clearColor(0, 0, 0, 0); gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT); gl.useProgram(program);
    gl.enable(gl.DEPTH_TEST); gl.enable(gl.BLEND); gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
    for (const [name, buffer] of [['position', positionBuffer], ['normal', normalBuffer]]) { const attr = attributes[name]; gl.bindBuffer(gl.ARRAY_BUFFER, buffer); gl.enableVertexAttribArray(attr); gl.vertexAttribPointer(attr, 3, gl.FLOAT, false, 0, 0); }
    // Shift the GL viewport center to match the canvas camera origin.
    gl.viewport(Math.round((origin[0] - w / 2) * dpr), Math.round((h / 2 - origin[1]) * dpr), width, height);
    gl.uniform3fv(uniforms.center, center); gl.uniform1f(uniforms.span, span);
    gl.uniformMatrix3fv(uniforms.rotation, false, new Float32Array([[1, 0, 0], [0, 1, 0], [0, 0, 1]].map(viewVector).flat()));
    gl.uniform2f(uniforms.scale, 2 * s / w, 2 * s / h); gl.uniform1f(uniforms.pointSize, Math.max(1.6, 2.1 * Math.sqrt(zoom)) * dpr);
    gl.uniform2f(uniforms.heightRange, heightRange[0], heightRange[1]);
    gl.uniform1f(uniforms.opacity, Number($('opacity').value) / 100); gl.uniform1i(uniforms.mesh, meshAvailable);
    // In light mode: use opposite warm ruby-to-amber palette for enhanced visibility & contrast
    gl.uniform3fv(uniforms.colorLow, isLight ? [0.82, 0.12, 0.32] : [0.20, 0.27, 0.75]);
    gl.uniform3fv(uniforms.colorHigh, isLight ? [0.98, 0.62, 0.12] : [0.60, 0.91, 0.86]);
    if (meshAvailable) { gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, indexBuffer); gl.drawElements(gl.TRIANGLES, triangles.length, indexExtension ? gl.UNSIGNED_INT : gl.UNSIGNED_SHORT, 0); } else gl.drawArrays(gl.POINTS, 0, positions.length / 3);
  } else {
    const count = positions.length / 3, step = Math.max(1, Math.ceil(count / (playing || drag ? 10000 : 24000))), { center, span } = metrics(), m = [[1, 0, 0], [0, 1, 0], [0, 0, 1]].map(viewVector);
    ctx.globalAlpha = Number($('opacity').value) / 100; const size = 1.8 * Math.sqrt(zoom);
    for (let i = 0; i < count; i += step) {
      const k = i * 3, x = (positions[k] - center[0]) / span, y = (positions[k + 1] - center[1]) / span, z = (positions[k + 2] - center[2]) / span;
      const px = origin[0] + (m[0][0] * x + m[1][0] * y + m[2][0] * z) * s, py = origin[1] - (m[0][1] * x + m[1][1] * y + m[2][1] * z) * s;
      if (px < 0 || py < 0 || px > w || py > h) continue; ctx.fillStyle = isLight ? (z < 0 ? '#b91c1c' : '#ea580c') : (z < 0 ? '#6896d9' : '#8ed7d8'); ctx.fillRect(px, py, size, size);
    } ctx.globalAlpha = 1;
  }
  if ($('grid').checked) drawGrid(xy, isLight);
  if ($('axes').checked) drawAxes(xy, w, h, dpr, isLight);
}
function drawGrid(xy, isLight) { const { lo, hi } = bounds, z = lo[2], steps = 10; ctx.strokeStyle = isLight ? '#0f172a18' : '#6683a21e'; ctx.lineWidth = .7; ctx.beginPath(); for (let i = 0; i <= steps; i++) { const t = i / steps; for (const pair of [[[lo[0] + (hi[0] - lo[0]) * t, lo[1], z], [lo[0] + (hi[0] - lo[0]) * t, hi[1], z]], [[lo[0], lo[1] + (hi[1] - lo[1]) * t, z], [hi[0], lo[1] + (hi[1] - lo[1]) * t, z]]]) { const a = xy(project(pair[0])), b = xy(project(pair[1])); ctx.moveTo(...a); ctx.lineTo(...b); } } ctx.stroke(); }
function axisMarks(a, capacity, divisions = 3) { const { lo, hi } = bounds, values = []; for (let i = 0; i < capacity; i++) { const lower = lo[a] + (hi[a] - lo[a]) * Math.max(0, (i - .5) / (capacity - 1)), upper = lo[a] + (hi[a] - lo[a]) * Math.min(1, (i + .5) / (capacity - 1)), v = Math.ceil(lower * divisions) / divisions; if (Number.isFinite(v) && v >= lo[a] && v <= upper && v <= hi[a] && (!values.length || v > values[values.length - 1])) values.push(v === 0 ? 0 : v); } return values; }
function axisUnits(a, capacity) { return axisMarks(a, capacity, 1); }
function drawAxes(xy, w, h, dpr, isLight) {
  const { lo, hi } = bounds; ctx.font = '10px -apple-system, sans-serif'; for (let a = 0; a < 3; a++) {
    const p = lo.map((v, i) => Math.max(v, Math.min(0, hi[i]))); ctx.fillStyle = isLight ? ['#dc2626', '#16a34a', '#2563eb'][a] : ['#d9919d', '#8ac5a8', '#89aae1'][a]; p[a] = lo[a]; const u = xy(project(p)); p[a] = hi[a]; const v = xy(project(p));
    const length = Math.hypot(v[0] - u[0], v[1] - u[1]);
    ctx.strokeStyle = ctx.fillStyle; ctx.lineWidth = .8; ctx.setLineDash(a < 2 ? [.7, .45] : [.7, .9]); ctx.beginPath(); ctx.moveTo(u[0], u[1]); ctx.lineTo(v[0], v[1]); ctx.stroke(); ctx.setLineDash([]);
    const raw = (hi[a] - lo[a]) / Math.max(2, Math.min(10, length / 50)), power = Math.pow(10, Math.floor(Math.log10(raw))), step = [1, 2, 5, 10].find(n => n * power >= raw) * power;
    ctx.beginPath(); for (let value = Math.ceil(lo[a] / step) * step; value <= hi[a] + step * 1e-6; value += step) { p[a] = value; const [x, y] = xy(project(p)); if (x < 12 || x > w - 28 || y < 20 || y > h - 20) continue; ctx.moveTo(x + 2, y); ctx.arc(x, y, 2, 0, Math.PI * 2); ctx.fillText(String(Number(value.toPrecision(6))), x + 7, y + 13); } ctx.fill();
    ctx.fillText(names[a].toUpperCase() + ' axis', Math.max(10, Math.min(w - 55, v[0] + 9)), Math.max(16, Math.min(h - 15, v[1] - 7)));
  }
}
function animate(time) {
  queued = false;
  if (lastTime !== null && playing && !drag && !document.hidden) {
    const elapsed = Math.max(0, (time - lastTime) / 1000); angle = (angle + rotation.direction * elapsed * Math.PI / 6) % (2 * Math.PI);
    frameAverage = frameAverage * .9 + Math.min(100, elapsed * 1000) * .1;
    if (++frameCount % 45 === 0 && $('quality').value === 'auto' && frameAverage > 22 && autoScale > .75) { autoScale = Math.max(.75, autoScale - .25); }
  } lastTime = time; draw(); if (playing && !document.hidden) schedule();
}
function schedule() { if (!queued) { queued = true; requestAnimationFrame(animate); } }
function setZoom(value) { zoom = Math.max(.15, Math.min(4, value)); $('zoom').value = Math.round(zoom * 100); $('zoom-value').value = Math.round(zoom * 100) + '%'; schedule(); }
function updateRotation() { rotation.axis = $('rotation-axis').value; rotation.direction = Number($('rotation-direction').value); lastTime = null; $('rotation-label').textContent = rotation.axis.toUpperCase() + ' axis · ' + (rotation.direction > 0 ? 'counterclockwise' : 'clockwise'); schedule(); }
$('rotation-axis').value = rotation.axis; $('rotation-direction').value = String(rotation.direction); updateRotation();
function updatePlaying() { $('pause').textContent = playing ? 'Ⅱ' : '▶'; $('pause').title = playing ? 'Pause rotation' : 'Resume rotation'; $('pause').setAttribute('aria-pressed', String(!playing)); $('rotating-dot').style.opacity = playing ? '1' : '.2'; lastTime = null; schedule(); }
$('pause').onclick = () => { playing = !playing; updatePlaying(); }; updatePlaying();
$('rotation-axis').onchange = $('rotation-direction').onchange = updateRotation;
$('reset').onclick = () => { yaw = .75; pitch = .5; angle = 0; lastTime = null; setZoom(1); };
$('zoom').oninput = () => setZoom(Number($('zoom').value) / 100); $('zoom-in').onclick = () => setZoom(zoom * 1.2); $('zoom-out').onclick = () => setZoom(zoom / 1.2);
canvas.onpointerdown = e => { drag = [e.clientX, e.clientY]; canvas.setPointerCapture(e.pointerId); canvas.focus(); };
canvas.onpointermove = e => { if (!drag) return; yaw += (e.clientX - drag[0]) * .008; pitch = Math.max(-1.5, Math.min(1.5, pitch + (e.clientY - drag[1]) * .008)); drag = [e.clientX, e.clientY]; schedule(); };
canvas.onpointerup = canvas.onpointercancel = () => { drag = null; lastTime = null; };
canvas.addEventListener('wheel', e => { e.preventDefault(); setZoom(zoom * Math.exp(-e.deltaY * .001)); }, { passive: false });
canvas.onkeydown = e => { if (e.key === 'ArrowLeft') yaw -= .1; else if (e.key === 'ArrowRight') yaw += .1; else if (e.key === 'ArrowUp') pitch = Math.min(1.5, pitch + .1); else if (e.key === 'ArrowDown') pitch = Math.max(-1.5, pitch - .1); else if (e.key === '+' || e.key === '=') setZoom(zoom * 1.1); else if (e.key === '-') setZoom(zoom / 1.1); else return; e.preventDefault(); schedule(); };
$('grid').onchange = $('axes').onchange = schedule;
$('opacity').oninput = () => { $('opacity-value').value = $('opacity').value + '%'; schedule(); };
// Resolution: reset auto-scale and force immediate redraw
$('quality').onchange = () => { autoScale = 1.25; frameAverage = 16; $('resolution-label').textContent = $('quality').selectedOptions[0].textContent; schedule(); };
$('fullscreen').onclick = async () => { try { if (document.fullscreenElement) await document.exitFullscreen(); else await $('workspace').requestFullscreen(); } catch (error) { $('status-message').textContent = 'Fullscreen is unavailable in this browser. Try expanding the window.'; } };
document.addEventListener('fullscreenchange', () => { $('fullscreen').setAttribute('aria-pressed', String(!!document.fullscreenElement)); $('fullscreen').querySelector('span').textContent = document.fullscreenElement ? 'Exit fullscreen' : 'Fullscreen'; schedule(); });
if (!$('workspace').requestFullscreen) { $('fullscreen').disabled = true; $('fullscreen').title = 'Fullscreen is unavailable in this browser'; }
document.addEventListener('visibilitychange', () => { lastTime = null; if (!document.hidden) schedule(); }); window.addEventListener('resize', schedule);
new ResizeObserver(schedule).observe($('viewport'));

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

function setRangeFields(next) { for (let a = 0; a < 3; a++) { $('min-' + a).value = next.lo[a]; $('max-' + a).value = next.hi[a]; $('span-' + a).value = Math.min(1000, next.hi[a] - next.lo[a]); } updateRangeSummary(); }
for (let a = 0; a < 3; a++) {
  const row = document.createElement('div'); row.className = 'axis-control axis-' + a;
  row.innerHTML = `<div class="axis-inputs"><span>${names[a].toUpperCase()}</span><input type="number" id="min-${a}" step="any" required aria-label="${names[a]} minimum"><input type="number" id="max-${a}" step="any" required aria-label="${names[a]} maximum"></div><input type="range" id="span-${a}" min="1" max="1000" step="1" aria-label="${names[a]} total span">`;
  $('range-fields').appendChild(row);
  $('span-' + a).oninput = () => { const min = Number($('min-' + a).value), max = Number($('max-' + a).value), center = Number.isFinite(min + max) ? (min + max) / 2 : 0, span = Number($('span-' + a).value); $('min-' + a).value = Number((center - span / 2).toFixed(5)); $('max-' + a).value = Number((center + span / 2).toFixed(5)); updateRangeSummary(); };
  $('min-' + a).oninput = $('max-' + a).oninput = updateRangeSummary;
}
function updateRangeSummary() { const spans = names.map((_, a) => Number($('max-' + a).value) - Number($('min-' + a).value)); $('range-summary').textContent = spans.map(v => Number.isFinite(v) && v > 0 ? Number(v.toFixed(2)).toLocaleString() : '—').join(' × '); spans.forEach((s, a) => $('span-' + a).value = Math.max(1, Math.min(1000, s))); }
function readBounds() { const next = { lo: [], hi: [] }; for (let a = 0; a < 3; a++) { const minText = $('min-' + a).value, maxText = $('max-' + a).value, min = Number(minText), max = Number(maxText); if (!minText || !maxText || !Number.isFinite(min) || !Number.isFinite(max) || max <= min || max - min > 1000 || Math.max(Math.abs(min), Math.abs(max)) > 1e6) throw new Error(names[a].toUpperCase() + ': enter finite increasing bounds, with a span up to 1,000 and coordinates within ±1,000,000.'); next.lo.push(min); next.hi.push(max); } return next; }
function boundsLabel() { $('bounds-label').textContent = names.map((name, a) => name.toUpperCase() + ' ' + bounds.lo[a] + '…' + bounds.hi[a]).join('   ·   '); }
function cleanupWorker() { if (worker) worker.terminate(); if (workerURL) URL.revokeObjectURL(workerURL); worker = null; workerURL = null; }
function resample(next) {
  cleanupWorker(); const id = ++job, start = performance.now(); $('range-error').textContent = ''; $('graph-status').textContent = 'Calculating…'; $('status-message').textContent = 'Preparing a quick preview, then refining the surface…'; $('empty').hidden = true;
  const finishError = () => { if (id !== job) return; cleanupWorker(); $('graph-status').textContent = 'Previous graph retained'; $('range-error').textContent = 'Could not recalculate this graph. Try lower surface detail or a narrower range.'; $('status-message').textContent = 'Range update failed. The previous graph is still displayed.'; };
  try {
    const code = `const compileExpression=${compileExpression.toString()},usesCoordinate=${usesCoordinate.toString()},scanRoots=${scanRoots.toString()},sampleSurface=${sampleSurface.toString()};onmessage=e=>{try{const data=e.data;const send=(result,phase)=>postMessage({...result,phase},[result.positions.buffer,result.triangles.buffer,result.normals.buffer]);if(data.detail>16)send(sampleSurface({...data,detail:16,maxMs:300}),'preview');send(sampleSurface({...data,maxMs:1800}),'final');}catch(error){postMessage({error:String(error)});}};`;
    workerURL = URL.createObjectURL(new Blob([code], { type: 'text/javascript' })); worker = new Worker(workerURL);
    worker.onerror = finishError;
    worker.onmessage = e => {
      if (id !== job) return; if (e.data.error) { finishError(); return; } const data = e.data, final = data.phase !== 'preview'; if (final) cleanupWorker(); bounds = { lo: next.lo.slice(), hi: next.hi.slice() }; positions = data.positions; triangles = data.triangles; normals = data.normals; updateGeometry(); boundsLabel();
      $('graph-status').textContent = final ? (data.capped ? 'Preview · limit reached' : 'Ready') : 'Refining…';
      $('status-message').textContent = final ? ('Surface ready in ' + ((performance.now() - start) / 1000).toFixed(1) + 's. ' + (data.capped ? 'Time or point limit reached; narrow the range for more detail.' : data.adaptive ? 'Sampling adjusted to keep this equation responsive.' : 'Drag to orbit. Scroll to zoom.')) : 'Preview ready. Refining in the background…';
    };
    worker.postMessage({ model, lo: next.lo, hi: next.hi, detail: Number($('detail').value) });
  } catch (error) { finishError(); }
}
$('range-form').onsubmit = e => { e.preventDefault(); try { resample(readBounds()); } catch (error) { $('range-error').textContent = error.message; } };
$('range-reset').onclick = () => { const next = { lo: [-50, -50, -50], hi: [50, 50, 50] }; setRangeFields(next); resample(next); };
$('detail').onchange = () => resample(bounds);
const initialDetail = Math.max(4, Math.min(128, requestedDetail));
if (![32, 64, 96, 128].includes(initialDetail)) { const option = document.createElement('option'); option.value = String(initialDetail); option.textContent = 'Custom · ' + initialDetail; $('detail').appendChild(option); }
$('detail').value = String(initialDetail);
setRangeFields(bounds); boundsLabel(); updateGeometry(); resample(bounds);
window.addEventListener('pagehide', cleanupWorker);
surface.addEventListener('webglcontextlost', e => { e.preventDefault(); gl = null; surface.style.display = 'none'; $('status-message').textContent = 'Using compatibility rendering.'; schedule(); });

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
