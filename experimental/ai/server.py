# SPDX-License-Identifier: GPL-3.0-or-later
"""Loopback-only isolated trial. No network inference, telemetry or input logging."""
from __future__ import annotations
import argparse
import concurrent.futures
import hmac
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import secrets
import select
import subprocess
import threading
import time
from urllib.parse import urlsplit

from models import ROOT, DATA, Zenz, SemanticSearch


class Trial:
    def __init__(self):
        self.zenz = None
        self.semantic = None
        self.helper = None
        self.lock = threading.Lock()
        self.last_used = time.monotonic()
        self.pool = concurrent.futures.ThreadPoolExecutor(max_workers=1)
        self.pending = threading.BoundedSemaphore(2)
        self.closed = False
        threading.Thread(target=self.reaper, daemon=True).start()

    def reaper(self):
        while not self.closed:
            time.sleep(15)
            with self.lock:
                if time.monotonic() - self.last_used > 120:
                    self.unload()

    def unload(self):
        if self.zenz:
            self.zenz.close()
        if self.semantic:
            self.semantic.close()
        self.zenz = self.semantic = None
        if self.helper:
            self.helper.terminate()
            try:
                self.helper.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.helper.kill()
                self.helper.wait()
            self.helper.stdin.close()
            self.helper.stdout.close()
            self.helper = None
        import gc
        gc.collect()

    def candidates(self, roman):
        if not self.helper or self.helper.poll() is not None:
            env = dict(os.environ, TAILKANA_MOZC_PROFILE=str(DATA/'mozc-profile'))
            executable = ROOT/'work/fcitx5-macos/build/arm64/tailkana/tailkana-ai-candidates'
            self.helper = subprocess.Popen([str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                           stderr=subprocess.DEVNULL, text=True, env=env)
        self.helper.stdin.write(roman+'\n')
        self.helper.stdin.flush()
        if not select.select([self.helper.stdout], [], [], 15)[0]:
            self.helper.kill()
            self.helper.wait()
            self.helper = None
            raise RuntimeError('日语候选生成超时，请重试')
        line = self.helper.stdout.readline()
        if not line:
            raise RuntimeError('日语候选进程退出，请重试')
        result = json.loads(line)
        # Mozc keeps a pending terminal n as fullwidth ｎ before Space.
        result['reading'] = result['reading'].replace('ｎ', 'ん')
        return result

    def run(self, operation, data, enqueued_at=None):
        with self.lock:
            self.last_used = time.monotonic()
            start = time.perf_counter()
            try:
                if operation == 'unload':
                    self.unload()
                    return {'released': True}
                if operation == 'rank':
                    cold_start = self.zenz is None or self.helper is None
                    roman = data.get('roman', '')
                    context = data.get('context', '')
                    if not isinstance(roman, str) or not 1 <= len(roman) <= 96 or any(c not in "abcdefghijklmnopqrstuvwxyz'-" for c in roman):
                        raise ValueError('请输入 1–96 个小写罗马字母')
                    if not isinstance(context, str) or len(context) > 1000:
                        raise ValueError('前文太长，请保留最后几句话')
                    candidate_start = time.perf_counter()
                    original = self.candidates(roman)
                    mozc_ms = round((time.perf_counter()-candidate_start)*1000, 2)
                    result = dict(original, ranked=[], mozc_ms=mozc_ms, model_ms=0, cold_start=cold_start)
                    incomplete = any('a' <= c.lower() <= 'z' or 'Ａ' <= c <= 'ｚ' for c in original['reading'])
                    if incomplete:
                        result['skipped'] = '拼写未完成，等待后续字母'
                    elif original['candidates']:
                        model_start = time.perf_counter()
                        if self.zenz is None:
                            self.zenz = Zenz()
                        try:
                            result['ranked'] = self.zenz.rank(context[-40:], original['reading'], tuple(c['text'] for c in original['candidates']))
                        except Exception:
                            self.zenz.close()
                            self.zenz = None
                            raise
                        result['model_ms'] = round((time.perf_counter()-model_start)*1000, 2)
                elif operation == 'search':
                    query = data.get('query', '')
                    if not isinstance(query, str) or not 1 <= len(query.strip()) <= 200:
                        raise ValueError('请输入 1–200 字的中文描述')
                    if self.semantic is None:
                        self.semantic = SemanticSearch()
                    try:
                        result = {'results': self.semantic.search(query.strip())}
                    except Exception:
                        self.semantic.close()
                        self.semantic = None
                        raise
                else:
                    raise ValueError('未知操作')
                result['elapsed_ms'] = round((time.perf_counter()-start)*1000)
                result['queue_ms'] = round((start-enqueued_at)*1000, 2) if enqueued_at is not None else 0
                return result
            finally:
                self.last_used = time.monotonic()


def serve(port=0, open_browser=False, token=None):
    token = token or secrets.token_urlsafe(32)
    trial = Trial()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass  # Requests may contain private text: never log them.

        def respond(self, status, value, html=False, javascript=False):
            content = value.encode() if html else json.dumps(value, ensure_ascii=False).encode()
            self.send_response(status)
            self.send_header('Content-Type', 'text/javascript; charset=utf-8' if javascript else ('text/html; charset=utf-8' if html else 'application/json; charset=utf-8'))
            self.send_header('Content-Length', str(len(content)))
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'")
            self.end_headers()
            self.wfile.write(content)

        def valid_host(self):
            return self.headers.get('Host') == f'127.0.0.1:{self.server.server_port}'

        def do_GET(self):
            if not self.valid_host():
                self.respond(403, {'error': '本地地址不匹配'})
            elif self.path == '/':
                html = (Path(__file__).parent/'index.html').read_text()
                self.respond(200, html, html=True)
            elif self.path in ('/realtime.js', '/trial.js'):
                self.respond(200, (Path(__file__).parent/self.path[1:]).read_text(), html=True, javascript=True)
            elif self.path == '/health':
                self.respond(200, {'ready': True, 'application': 'TailKana-AI-trial'})
            else:
                self.respond(404, {'error': '页面不存在'})

        def do_POST(self):
            origin = self.headers.get('Origin')
            expected = f'http://127.0.0.1:{self.server.server_port}'
            if not self.valid_host() or origin not in (None, expected) or not hmac.compare_digest(self.headers.get('X-TailKana-Token', ''), token):
                self.respond(403, {'error': '请通过本次试验入口打开页面'})
                return
            try:
                length = int(self.headers.get('Content-Length', '0'))
                if not 0 < length <= 16384:
                    raise ValueError('请求太长或为空')
                data = json.loads(self.rfile.read(length))
                if not isinstance(data, dict):
                    raise ValueError('请求格式错误')
                operation = self.path.removeprefix('/api/')
                if operation == 'shutdown':
                    self.respond(200, {'stopped': True})
                    threading.Thread(target=self.server.shutdown, daemon=True).start()
                    return
                if operation not in ('search', 'rank', 'unload'):
                    raise ValueError('未知操作')
                if not trial.pending.acquire(blocking=False):
                    self.respond(429, {'error': '正在处理，请稍后再试'})
                    return
                try:
                    future = trial.pool.submit(trial.run, operation, data, time.perf_counter())
                    result = future.result(timeout=60)
                finally:
                    # In-flight timed-out jobs retain the slot until done.
                    if 'future' in locals() and not future.done():
                        future.add_done_callback(lambda _: trial.pending.release())
                    else:
                        trial.pending.release()
                self.respond(200, result)
            except concurrent.futures.TimeoutError:
                self.respond(503, {'error': '模型响应超时，请稍后重试或释放模型'})
            except (ValueError, json.JSONDecodeError) as error:
                self.respond(400, {'error': str(error)})
            except Exception as error:
                # Keep errors useful without exposing user input or traceback.
                self.respond(503, {'error': '模型暂不可用：'+type(error).__name__})

    server = ThreadingHTTPServer(('127.0.0.1', port), Handler)
    url = f'http://127.0.0.1:{server.server_port}/#{token}'
    DATA.mkdir(parents=True, exist_ok=True)
    (DATA/'session.json').write_text(json.dumps({'url': url, 'pid': os.getpid()}))
    os.chmod(DATA/'session.json', 0o600)
    print(url, flush=True)
    if open_browser:
        import webbrowser
        webbrowser.open(url)
    try:
        server.serve_forever()
    finally:
        server.server_close()
        trial.closed = True
        trial.pool.shutdown(wait=True, cancel_futures=True)
        trial.unload()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=0)
    parser.add_argument('--open', action='store_true')
    args = parser.parse_args()
    serve(args.port, args.open)
