# SPDX-License-Identifier: GPL-3.0-or-later
"""Reuse a live trial session so repeated launches don't load duplicate models."""
import json
import urllib.request
import webbrowser
from models import DATA
from server import serve

session=DATA/'session.json'
try:
    url=json.loads(session.read_text())['url']
    if not url.startswith('http://127.0.0.1:'):
        raise ValueError('invalid local session')
    origin=url.split('/#')[0]
    with urllib.request.urlopen(origin+'/health', timeout=1) as response:
        if json.load(response).get('application') != 'TailKana-AI-trial':
            raise ValueError('stale trial session')
    webbrowser.open(url)
except (OSError, ValueError, KeyError):
    serve(open_browser=True)
