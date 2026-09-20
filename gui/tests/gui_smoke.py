#!/usr/bin/env python3
"""Exercise actual ImGui controls in an SDL/OpenGL window and capture SAPF audio.
Requires a graphical session, pactl, parec. Test input is injected as ImGui IO
mouse/key events; actions never mutate the document directly.
"""
import array
import json
import math
import os
from pathlib import Path
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
BINARY = ROOT / 'build/gui/sapf-experiments'
OUT = ROOT / 'build/verification'
OUT.mkdir(parents=True, exist_ok=True)


def eventually(fn, timeout=8):
    end = time.monotonic() + timeout
    last = None
    while time.monotonic() < end:
        try:
            last = fn()
            if last:
                return last
        except (FileNotFoundError, json.JSONDecodeError, KeyError):
            pass
        time.sleep(.04)
    raise AssertionError(f'Timed out: {last}')


with tempfile.TemporaryDirectory(prefix='sapf-imgui-') as tmp:
    tmp = Path(tmp)
    probe, inputs = tmp / 'probe.json', tmp / 'input.json'
    sink = f'sapf_imgui_{os.getpid()}'
    module = subprocess.check_output(['pactl', 'load-module', 'module-null-sink',
        f'sink_name={sink}', 'rate=48000', 'channels=2'], text=True).strip()
    app = None
    seq = 0
    logfile = (OUT / 'gui-smoke-app.log').open('w')
    try:
        env = dict(os.environ, PULSE_SINK=sink, SDL_VIDEODRIVER='x11')
        app = subprocess.Popen([str(BINARY), '--scale', '1.5', '--probe', str(probe),
            '--test-input', str(inputs)], env=env, stdout=logfile, stderr=subprocess.STDOUT)
        def state():
            if app.poll() is not None:
                raise RuntimeError(f'GUI exited {app.returncode}: {(OUT / "gui-smoke-app.log").read_text()}')
            return json.loads(probe.read_text())
        def nodes():
            return dict(state()['state']['graph']['nodes'])
        def action(kind='click', target=None, **kwargs):
            global seq
            seq += 1
            command = dict(seq=seq, kind=kind, **kwargs)
            if target:
                eventually(lambda: target in state()['items'])
                command['target'] = target
            temp = inputs.with_suffix('.tmp')
            temp.write_text(json.dumps(command)); temp.replace(inputs)
            eventually(lambda: state().get('testSequence') == seq)
            time.sleep(.07)
        def numeric(node, value):
            action(target=f'node/{node}/number')
            action('type', text=str(value))
            eventually(lambda: abs(nodes()[node]['value']-value) < 1e-5)
        def capture():
            with tempfile.TemporaryFile() as f:
                rec = subprocess.Popen(['parec', f'--device={sink}.monitor', '--latency-msec=20',
                    '--format=float32le', '--rate=48000', '--channels=2'], stdout=f)
                time.sleep(.8)
                rec.terminate(); rec.wait(timeout=3); f.seek(0); raw=f.read()
            samples = array.array('f'); samples.frombytes(raw[:len(raw)//4*4])
            mono = samples[::2][-24000:]
            assert len(mono)>1000, len(mono)
            assert all(math.isfinite(x) for x in mono)
            peak = max(map(abs,mono), default=0)
            crossings = sum(a<=0<b for a,b in zip(mono,mono[1:]))
            hz = crossings * 48000 / len(mono)
            return peak, hz
        eventually(lambda: 'node/1/number' in state()['items'])
        implementation=nodes()[4]['implementation']
        numeric(1, 75)
        action(target='node/1/number');action('type',text='74',enter=False)
        action(target='commit-label');assert nodes()[1]['value']==74
        numeric(1,75)
        action(target='commit')
        assert len(state()['history'])==2
        action(target=f'node/{implementation}/open', edge=True)
        eventually(lambda: nodes()[implementation]['open'])
        action(target='commit')
        branch = state()['current']
        action(target='undo')
        assert not nodes()[implementation]['open']
        numeric(1, 60)
        action(target='commit')
        fork = state()['current']
        assert state()['history'][fork]['parent']==state()['history'][branch]['parent']
        action(target=f'history/{branch}')
        assert nodes()[implementation]['open'] and nodes()[1]['value']==75
        action(target=f'node/{implementation}/open', edge=True)
        action(target='node/4/copy')
        root=state()['state']['selected']; center=nodes()[root]['args'][0]
        numeric(center, 62)
        assert nodes()[1]['value']==75 and nodes()[center]['value']==62
        print('PASS: numeric entry, expansion, commit/undo, retained branch, independent clone')
        for i in range(5):
            action(target=f'syntax/{i}')
            assert state()['state']['style']['syntax']==i
        action(target=f'node/{root}/text')
        assert nodes()[root]['text']
        action(target=f'node/{root}/text')
        print('PASS: all five syntax selectors and per-expression text/UI toggle')
        action(target='page/3')
        action('drag',target='widget/0',fraction=.8)
        changed=state()['state']['widgets'][0]
        assert changed!=880
        action('drag',target='knob/Atk',dy=-20)
        assert state()['state']['widgets'][15]>.1
        action(target='page/2'); assert state()['state']['page']==2
        action(target='page/0'); assert state()['state']['page']==0
        action(target='page/1')
        print('PASS: prototype/waves/widget pages, frequency slider and attack knob')
        action(target='save')
        action(target='file-path'); action('type',text=str(tmp/'session.json'))
        action(target='file-confirm')
        saved=json.loads((tmp/'session.json').read_text())
        assert len(saved['history'])>=4
        numeric(1, 55)
        action(target='open'); action(target='file-confirm')
        assert nodes()[1]['value']==75
        assert state()['state']['widgets'][0]==changed
        print('PASS: native Save/Open preserves branches and widget state')
        # Replace the first root through its visible Edit button, then play a known tone.
        action(target='node/4/open',edge=True)  # collapse so edit stays visible
        action(target='node/4/edit')
        action(target='postfix-input'); action('type',text='440 0 sinosc 0.04 *')
        action(target='postfix-apply')
        assert nodes()[4]['op']=='*'
        freq=next(k for k,n in nodes().items() if n['label']=='freq' and n['value']==440)
        action(target='root/4/play')
        eventually(lambda: state()['playing'], timeout=12)
        time.sleep(.5)
        peak,hz=capture();assert .02<peak<.06 and abs(hz-440)<8,(peak,hz,state()['log'])
        action(target='node/4/open',edge=True)  # expand replacement (preserves old closed state? see model)
        # Nested oscillator may need opening; only click if it is closed.
        osc=nodes()[4]['args'][0]
        if not nodes()[4]['open']: action(target='node/4/open',edge=True)
        if not nodes()[osc]['open']: action(target=f'node/{osc}/open',edge=True)
        numeric(freq, 880)
        time.sleep(.4)
        peak2,hz2=capture();assert .02<peak2<.06 and abs(hz2-880)<8,(peak2,hz2,state()['log'])
        assert 'GUI_PLAYING_2' not in state()['log'], 'Live edit unexpectedly restarted playback'
        action(target='stop');time.sleep(.25)
        silence,_=capture();assert silence<1e-6,silence
        print(f'PASS: GUI -> real SAPF -> PipeWire {hz:.1f} Hz / {peak:.5f}; live edit {hz2:.1f} Hz / {peak2:.5f}; Stop {silence}')
        # Console is full SAPF, independently of the structural vocabulary.
        action(target='console');action(target='console-source');action('type',text='6 7 * pr cr')
        action(target='evaluate');eventually(lambda:'\n42\n' in state()['log'])
        action(target='reset-engine');time.sleep(.3)
        action(target='evaluate');eventually(lambda:state()['log'].count('\n42\n')>=2)
        print('PASS: console evaluation and engine reset')
        (OUT/'gui-smoke-final.json').write_text(json.dumps(state(),indent=2))
    finally:
        if app and app.poll() is None:
            app.terminate()
            try:app.wait(timeout=3)
            except subprocess.TimeoutExpired:app.kill();app.wait()
        logfile.close()
        subprocess.run(['pactl','unload-module',module],check=True)
