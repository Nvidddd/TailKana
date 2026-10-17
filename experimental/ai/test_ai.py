# SPDX-License-Identifier: GPL-3.0-or-later
"""Integration tests against real offline models, not fabricated model replies."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import unittest
import urllib.error
import urllib.request

from models import DATA, ROOT, Zenz, SemanticSearch, SemanticIndex, katakana
from server import Trial


class ModelsTest(unittest.TestCase):
    def test_batch_groups_match_independent_candidate_scores(self):
        zenz=Zenz()
        choices=('橋','箸','端','はし','ハシ','土師','波子','葉し','橋梁','梯子','竹','端子')
        try:
            together={r['text']:r['score'] for r in zenz.rank('川を渡るために','はし',choices)}
            self.assertEqual(set(together),set(choices))
            for word in choices:
                single=zenz.rank('川を渡るために','はし',(word,))[0]['score']
                self.assertAlmostEqual(together[word],single,delta=.02)
        finally:
            zenz.close()

    def test_incomplete_syllable_avoids_neural_work(self):
        trial=Trial()
        try:
            result=trial.run('rank',{'roman':'hash','context':'川を渡るために'})
            self.assertIn('skipped',result)
            self.assertEqual(result['ranked'],[])
            self.assertIsNone(trial.zenz)
            self.assertGreaterEqual(result['mozc_ms'],0)
        finally:
            trial.closed=True
            trial.pool.shutdown()
            trial.unload()

    def test_zenz_context_changes_same_reading(self):
        zenz=Zenz()
        try:
            choices=('橋', '箸', '端')
            self.assertEqual(zenz.rank('ご飯を食べるために', 'はし', choices)[0]['text'], '箸')
            self.assertEqual(zenz.rank('川を渡るために', 'はし', choices)[0]['text'], '橋')
            self.assertEqual(choices, ('橋', '箸', '端'))
        finally:
            zenz.close()

    def test_semantic_retrieval_and_dictionary_coverage(self):
        search=SemanticIndex()
        self.assertEqual(search.vectors.shape, (112173, 384))
        results=search.search('下雨时用来遮雨的东西')
        self.assertIn('傘', [r['word'] for r in results[:3]])
        entry=next(r for r in results if r['word']=='傘')
        self.assertEqual(entry['reading'], 'かさ')
        self.assertIn('遮蔽雨水', entry['meaning'])
        self.assertEqual(len({r['word'] for r in results}), len(results))
        search.search.cache_clear()

    def test_helper_complete_composition_and_isolated_profile(self):
        trial=Trial()
        try:
            result=trial.candidates('watasihanihongo')
            self.assertEqual(result['reading'], 'わたしはにほんご')
            self.assertEqual(result['candidates'][0]['text'], '私は日本語')
            self.assertEqual(trial.candidates('kikan')['reading'], 'きかん')
            self.assertEqual(trial.candidates('nihao!')['candidates'], [])
        finally:
            trial.closed=True
            trial.pool.shutdown()
            trial.unload()

    def test_validation_before_loading(self):
        trial=Trial()
        try:
            for operation, data in [('rank', {'roman':'a\nb'}), ('rank', {'roman':'a', 'context':'x'*1001}), ('search', {'query':''})]:
                with self.assertRaises(ValueError):trial.run(operation, data)
            self.assertIsNone(trial.zenz)
            self.assertIsNone(trial.semantic)
        finally:
            trial.closed=True
            trial.pool.shutdown()
            trial.unload()

    def test_explicit_unload_clears_cached_model_references(self):
        trial=Trial()
        try:
            trial.run('rank', {'roman':'hashi', 'context':'川を渡るために'})
            self.assertGreater(trial.zenz.rank.cache_info().currsize, 0)
            trial.run('search', {'query':'遮雨的用具'})
            self.assertGreater(trial.semantic.search.cache_info().currsize, 0)
            self.assertEqual(trial.run('unload', {}), {'released': True})
            self.assertIsNone(trial.zenz)
            self.assertIsNone(trial.semantic)
            self.assertEqual(Zenz.rank.cache_info().currsize, 0)
            self.assertEqual(SemanticSearch.search.cache_info().currsize, 0)
        finally:
            trial.closed=True
            trial.pool.shutdown()
            trial.unload()


if __name__ == '__main__':
    unittest.main(verbosity=2)
