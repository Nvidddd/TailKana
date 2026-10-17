#!/usr/bin/env python3
"""Prepare only the native IME's pinned model/runtime, without the browser trial."""
from pathlib import Path
import hashlib
import json
import subprocess

root = Path(__file__).resolve().parents[1]
lock = json.loads((root / 'upstream-lock.json').read_text())['llama.cpp']
runtime = root / lock['path']
if not runtime.exists():
    runtime.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['git', 'clone', '--no-checkout', lock['url'], str(runtime)], check=True)
subprocess.run(['git', '-C', str(runtime), 'checkout', '--detach', lock['commit']], check=True)

model = next(item for item in json.loads((root / 'experimental/ai/model-lock.json').read_text())
             if item['file'] == 'zenz.gguf')
destination = root / 'work/ai-experiment/models' / model['file']
destination.parent.mkdir(parents=True, exist_ok=True)
def valid(path):
    return (path.is_file() and path.stat().st_size == model['bytes'] and
            hashlib.sha256(path.read_bytes()).hexdigest() == model['sha256'])
if not valid(destination):
    temporary = destination.with_suffix('.partial')
    url = f"https://huggingface.co/{model['repo']}/resolve/{model['revision']}/{model['source']}?download=true"
    subprocess.run(['curl', '--fail', '--location', '--retry', '4', '--connect-timeout', '20',
                    '--continue-at', '-', url, '-o', str(temporary)], check=True)
    if not valid(temporary):
        raise SystemExit('Zenz model checksum mismatch')
    temporary.replace(destination)

# The native regression suite resolves its subprocess/resources as a real bundle
# would. Symlinks remain valid when the build is created after this preparation.
home = root / 'work/ai-ime-test-home'
for relative, target in (
    ('bin/TailKanaAI', root / 'work/fcitx5-macos/build/arm64/tailkana/TailKanaAI'),
    ('share/tailkana/ai/zenz.gguf', destination),
):
    link = home / relative
    link.parent.mkdir(parents=True, exist_ok=True)
    if link.is_symlink():
        link.unlink()
    if not link.exists():
        link.symlink_to(target)
print('Native AI runtime and model verified.')
