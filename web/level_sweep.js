/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Plays every shipped level for a few seconds with throttle held, recording what
 * state the game reaches and any crash. Paste into the page console on
 * index.html?test (a profile must already exist), then read window.sweep.
 *
 *   window.sweep = { done, i, total, results, startedAt, finishedAt }
 *
 * A result's state is the top game state after the wait. StatePlayingLocal or
 * StateDeadMenu/StateDeadJust/StateFinished mean the level loaded and ran. A
 * StateMessageBox means the game refused it; `log` then has the reason.
 */
(async function () {
  const LOG = '/home/web_user/.cache/xmoto/logs/xmoto.log';
  const PLAY_MS = 2500;
  const LOAD_TIMEOUT_MS = 30000;
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  const top = () => Module.UTF8ToString(Module._xm_web_top_state());
  const key = (type) => window.dispatchEvent(new KeyboardEvent(type, {
    code: 'ArrowUp', key: 'ArrowUp', keyCode: 38, which: 38, bubbles: true, cancelable: true,
  }));
  const logSize = () => FS.stat(LOG).size;
  const logSince = (from) => FS.readFile(LOG, { encoding: 'utf8' }).slice(from);

  let crash = null;
  window.addEventListener('error', (e) => { crash = e.message; });
  window.addEventListener('unhandledrejection', (e) => { crash = String(e.reason); });

  const levels = await (await fetch('levels.json')).json();
  const sweep = window.sweep = {
    done: false, i: 0, total: levels.length, results: [], startedAt: Date.now(), finishedAt: null,
  };
  const SETTLED = ['StatePlayingLocal', 'StateDeadMenu', 'StateDeadJust', 'StateFinished', 'StateMessageBox'];

  for (const level of levels) {
    sweep.i++;
    const r = { id: level.id, file: level.file, chipmunk: level.chipmunk };
    const from = logSize();
    const t0 = performance.now();
    if (!level.id) { r.state = 'NO_ID'; sweep.results.push(r); continue; }
    try {
      Module.ccall('xm_web_play_level', null, ['string'], [level.id]);
      while (performance.now() - t0 < LOAD_TIMEOUT_MS && !crash && !SETTLED.includes(top())) {
        await sleep(50);
      }
      r.loadMs = Math.round(performance.now() - t0);
      key('keydown');
      await sleep(PLAY_MS);
      key('keyup');
      r.state = top();
      const interesting = logSince(from).split('\n').filter((l) => /error|exception|cannot|failed|fatal/i.test(l));
      if (interesting.length) r.log = interesting.slice(0, 4).join(' | ').slice(0, 300);
    } catch (e) {
      crash = String(e);
    }
    if (crash) {
      r.crash = crash;
      sweep.results.push(r);
      sweep.fatal = level.id;
      break; /* the wasm instance is unusable after this */
    }
    sweep.results.push(r);
  }
  sweep.done = true;
  sweep.finishedAt = Date.now();
})();
