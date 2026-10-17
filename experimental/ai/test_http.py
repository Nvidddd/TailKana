# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the live loopback service without involving external websites."""
import json
import unittest
import urllib.request
import urllib.error
from models import DATA

session=json.loads((DATA/'session.json').read_text())
origin,token=session['url'].split('/#')

class HTTPTest(unittest.TestCase):
    def test_realtime_scripts_have_javascript_mime(self):
        for path in ('/realtime.js','/trial.js'):
            with urllib.request.urlopen(origin+path,timeout=10) as response:
                self.assertEqual(response.status,200)
                self.assertIn('text/javascript',response.headers['Content-Type'])
                self.assertGreater(len(response.read()),100)

    def request(self, path, data, headers=None):
        request=urllib.request.Request(origin+path,data=json.dumps(data).encode(),headers=headers or {})
        try:
            with urllib.request.urlopen(request,timeout=45) as response:
                return response.status,json.load(response)
        except urllib.error.HTTPError as error:
            return error.code,json.load(error)

    def test_no_cross_origin_or_missing_token(self):
        self.assertEqual(self.request('/api/search',{'query':'筷子'})[0],403)
        self.assertEqual(self.request('/api/search',{'query':'筷子'},{'X-TailKana-Token':token,'Origin':'https://example.com'})[0],403)

    def test_input_validation(self):
        status,value=self.request('/api/rank',{'roman':'a\nb'},{'X-TailKana-Token':token})
        self.assertEqual(status,400)
        self.assertIn('小写罗马',value['error'])

    def test_real_inference_and_unload(self):
        headers={'X-TailKana-Token':token,'Origin':origin}
        status,value=self.request('/api/rank',{'roman':'hashi','context':'ご飯を食べるために'},headers)
        self.assertEqual(status,200)
        self.assertEqual(value['ranked'][0]['text'],'箸')
        self.assertEqual(value['candidates'][0]['text'],'橋')
        self.assertGreaterEqual(value['model_ms'],0)
        self.assertGreaterEqual(value['mozc_ms'],0)
        self.assertGreaterEqual(value['queue_ms'],0)
        status,value=self.request('/api/search',{'query':'下雨时用来遮雨的东西'},headers)
        self.assertEqual(status,200)
        self.assertIn('傘',[x['word'] for x in value['results'][:3]])
        self.assertEqual(self.request('/api/unload',{},headers),(200,{'released':True}))

if __name__=='__main__':unittest.main(verbosity=2)
