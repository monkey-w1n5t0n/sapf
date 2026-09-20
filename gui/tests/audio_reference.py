#!/usr/bin/env python3
"""Compare the reconstructed bubbles graph with SAPF's original example offline."""
import array
import os
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
engine=os.environ.get('SAPF_ENGINE',str(Path.home()/'src/sapf-linux/run-sapf'))
code=subprocess.check_output([str(ROOT/'build/gui/model-tests'),'--bubbles'],text=True).strip()
reference='(([.4 [8 7.23]] 0 lfsaw) [24 3] * +/ 81 + nnhz) 0 sinosc .04 * .2 0 4 combn'
with tempfile.TemporaryDirectory(prefix='sapf-reference-') as temp:
    env=dict(os.environ,SAPF_RECORDINGS=temp)
    program=f'{code} 1 T "rebuilt" >sf\n{reference} 1 T "original" >sf\n440 ZR = zcheck zcheck zctl 1024 take mean pr cr\nquit\n'
    result=subprocess.run([engine],input=program,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env,timeout=20)
    assert result.returncode==0 and 'error:' not in result.stdout,result.stdout
    assert '\n440\n' in result.stdout,result.stdout
    def read(name):
        data=subprocess.check_output(['ffmpeg','-v','error','-i',f'{temp}/{name}.wav','-f','f64le','-acodec','pcm_f64le','-'])
        values=array.array('d');values.frombytes(data);return values
    rebuilt,original=read('rebuilt'),read('original')
    assert len(rebuilt)==len(original)==96000,(len(rebuilt),len(original))
    error=max(abs(a-b) for a,b in zip(rebuilt,original))
    peak=max(map(abs,rebuilt))
    assert peak>.01 and error<2e-5,(peak,error)
    print(f'PASS: 48000 stereo frames vs original bubbles; peak {peak:.9f}, max sample difference {error:.9g}')
    print('PASS: zctl starts at the reference value (1024-sample mean 440)')
