import hashlib
import json
from pathlib import Path
import tempfile
import unittest

import force_normalization as f


def row(t, value, valid=True, epoch=0, speed=30):
    return dict(time=t, value=value, valid=valid, epoch=epoch, speed=speed)


class StatisticsTests(unittest.TestCase):
    def test_time_weighting_not_sample_weighting(self):
        result = f.summarize([row(0, .1), row(.09, 1), row(.10, .5)])['bands']['all']
        self.assertAlmostEqual(result['rms'], (.109 ** .5))
        self.assertEqual(result['absP50'], .1)
        self.assertEqual(result['absP95'], 1)
        self.assertAlmostEqual(result['atUnitFraction'], .1)

    def test_resampling_same_hold_does_not_change_metrics(self):
        a = f.summarize([row(0, .2), row(.1, .4), row(.2, 0)])['bands']['all']
        b = f.summarize([row(0, .2), row(.05, .2), row(.1, .4), row(.15, .4), row(.2, 0)])['bands']['all']
        for key in a:
            self.assertAlmostEqual(a[key], b[key])

    def test_invalid_epoch_and_large_gap_are_not_sustained_force(self):
        rows = [row(0, 1), row(.05, 1, epoch=1), row(.1, 1, valid=False, epoch=1), row(.15, .1, epoch=1), row(2, .2, epoch=1), row(2.05, 0, epoch=1)]
        s = f.summarize(rows)
        self.assertAlmostEqual(s['excludedSeconds']['epoch'], .05)
        self.assertAlmostEqual(s['excludedSeconds']['invalid'], .05)
        self.assertAlmostEqual(s['excludedSeconds']['gap'], 1.85)
        self.assertAlmostEqual(s['bands']['all']['seconds'], .1)

    def test_cap_is_distinct_from_unit_limit_and_final_peak(self):
        s = f.summarize([row(0, .25), row(.1, .25), row(.2, 1)], cap=.25)
        self.assertEqual(s['bands']['all']['atCapFraction'], 1)
        self.assertEqual(s['bands']['all']['atUnitFraction'], 0)
        self.assertEqual(s['bands']['all']['peak'], .25)
        self.assertEqual(s['validSamplePeak'], 1)

    def test_empty_band_is_unavailable_not_zero_force(self):
        s = f.summarize([row(0, 0), row(.1, 0)])
        self.assertIsNone(s['bands']['60+ km/h']['rms'])
        self.assertEqual(s['bands']['all']['rms'], 0)

    def test_reversed_time_and_nonfinite_refused(self):
        with self.assertRaises(ValueError):
            f.summarize([row(.1, 0), row(0, 0)])
        for value in ('NaN', 'Infinity', '1.01'):
            with self.assertRaises(ValueError):
                f.mapped({'t': 0, 'v': value}, {'time': 't', 'value': 'v'})

    def test_bad_epoch_and_speed_conversion_overflow_refused(self):
        with self.assertRaises(ValueError):
            f.mapped({'t': 0, 'v': 0, 'e': .5}, {'time': 't', 'value': 'v', 'epoch': 'e'})
        with self.assertRaises(ValueError):
            f.mapped({'t': 0, 'v': 0, 's': 1e308}, {'time': 't', 'value': 'v', 'speed': 's', 'speedScale': 3.6})


class TemporalTests(unittest.TestCase):
    def timing(self, rows, band='all'):
        return f.summarize(rows)['temporal']['bands'][band]

    def test_same_magnitudes_different_order_exposes_sustained_vs_alternating(self):
        a = [row(i*.1, v) for i, v in enumerate((.6, .6, -.6, -.6, 0))]
        b = [row(i*.1, v) for i, v in enumerate((.6, -.6, .6, -.6, 0))]
        x, y = f.summarize(a), f.summarize(b)
        self.assertEqual(x['bands'], y['bands'])
        x, y = x['temporal']['bands']['all'], y['temporal']['bands']['all']
        self.assertEqual((x['signReversals'], y['signReversals']), (1, 3))
        self.assertAlmostEqual(x['thresholds']['0.5']['longestSameSignSeconds'], .2)
        self.assertAlmostEqual(y['thresholds']['0.5']['longestSameSignSeconds'], .1)
        self.assertAlmostEqual(x['variationPerSecond'], 3)
        self.assertAlmostEqual(y['variationPerSecond'], 9)

    def test_neutral_zone_does_not_count_noise_but_breaks_strong_hold(self):
        s = self.timing([row(i*.1,v) for i,v in enumerate((.6,.01,-.01,-.6,0))])
        self.assertEqual(s['signReversals'], 1)
        self.assertEqual(s['thresholds']['0.5']['runs'], 2)
        self.assertAlmostEqual(s['thresholds']['0.5']['fraction'], .5)
        self.assertAlmostEqual(s['meanSigned'], 0)

    def test_exclusions_do_not_join_force_holds_or_reversals(self):
        s = self.timing([row(0,.6),row(.05,.6,epoch=1),row(.1,-.6,False,1),
                         row(.15,-.6,epoch=1),row(2,-.6,epoch=1),row(2.05,0,epoch=1)])
        self.assertEqual(s['segments'], 2)
        self.assertEqual(s['signReversals'], 0)
        self.assertEqual(s['variationPerSecond'], 0)
        self.assertAlmostEqual(s['thresholds']['0.5']['longestSameSignSeconds'], .05)

    def test_speed_band_reentry_breaks_hold_only_in_that_band(self):
        rows = [row(0,.6),row(.1,.6,speed=70),row(.2,.6),row(.3,.6),row(.4,0)]
        all_rows, band = self.timing(rows), self.timing(rows,'20-60 km/h')
        self.assertAlmostEqual(all_rows['thresholds']['0.5']['longestSameSignSeconds'], .4)
        self.assertAlmostEqual(band['thresholds']['0.5']['longestSameSignSeconds'], .2)
        self.assertEqual(band['segments'], 2)

    def test_repeated_samples_do_not_change_order_metrics(self):
        a = self.timing([row(0,.6),row(.1,-.6),row(.2,0)])
        b = self.timing([row(0,.6),row(.05,.6),row(.1,-.6),row(.15,-.6),row(.2,0)])
        for key in ('seconds','meanSigned','meanAbsolute','variationPerSecond','signReversals','segments'):
            self.assertAlmostEqual(a[key], b[key])
        for key in a['thresholds']:
            for metric in a['thresholds'][key]:
                self.assertAlmostEqual(a['thresholds'][key][metric], b['thresholds'][key][metric])

    def test_final_sample_does_not_invent_force_duration(self):
        s = self.timing([row(0,.2),row(.1,1)])
        self.assertAlmostEqual(s['meanAbsolute'], .2)
        self.assertEqual(s['thresholds']['0.5']['fraction'], 0)
        self.assertEqual(s['variationPerSecond'], 0)

    def test_duplicate_timestamp_epoch_round_trip_does_not_bridge_reset(self):
        s = self.timing([row(0,.6),row(.1,.6),row(.1,.6,epoch=1),row(.1,.6),row(.2,0)])
        self.assertEqual(s['segments'], 2)
        self.assertEqual(s['thresholds']['0.5']['runs'], 2)
        self.assertAlmostEqual(s['thresholds']['0.5']['longestSameSignSeconds'], .1)

    def test_empty_band_is_unavailable(self):
        s = self.timing([row(0,.6),row(.1,0)],'60+ km/h')
        self.assertIsNone(s['meanAbsolute'])
        self.assertIsNone(s['thresholds']['0.5']['fraction'])
        self.assertIsNone(s['thresholds']['0.5']['longestSameSignSeconds'])

    def test_threshold_fraction_is_duration_weighted_and_unit_based(self):
        s = self.timing([row(0,.2),row(.09,.8),row(.1,0)])
        self.assertAlmostEqual(s['thresholds']['0.5']['fraction'], .1)
        self.assertAlmostEqual(s['meanAbsolute'], .26)
        capped = f.summarize([row(0,.25),row(.1,.25)],cap=.25)
        self.assertEqual(capped['bands']['all']['atCapFraction'],1)
        self.assertEqual(capped['temporal']['bands']['all']['thresholds']['0.5']['fraction'],0)


class FileTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.source = self.root / 'samples.csv'
        self.source.write_text('t,v\n0,0.1\n0.1,0.2\n', encoding='utf-8')
        self.validation = self.root / 'validation.json'
        self.validation.write_text('{"passed":true}', encoding='utf-8')
        art = lambda p: {'path': p.name, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
        self.spec = dict(id='fixture', game='test', quantity='constant-force-request', evidence='synthetic',
                         model='fixture@1', settings={'strength': 50}, limitations='Synthetic only',
                         validation='unit fixture', artifacts=[art(self.validation)], source=art(self.source),
                         format='csv', mapping={'time': 't', 'value': 'v'}, cap=1, maxGapSeconds=.1)
        self.manifest = self.root / 'manifest.json'
        self.save()

    def save(self):
        self.manifest.write_text(json.dumps({'schema': 'dbce.ffb-normalization', 'version': 1, 'series': [self.spec]}), encoding='utf-8')

    def test_end_to_end_and_no_overwrite(self):
        output = self.root / 'report'
        self.assertEqual(f.main([str(self.manifest), str(output)]), 0)
        before = (output / 'report.json').read_bytes()
        self.assertEqual(f.main([str(self.manifest), str(output)]), 2)
        self.assertEqual(before, (output / 'report.json').read_bytes())

    def test_report_labels_temporal_scope_and_keeps_magnitude_table_together(self):
        report = f.markdown(f.analyze(self.manifest))
        self.assertIn('not detected corner phases', report)
        self.assertLess(report.index('| fixture / all |'), report.index('## Request timing: fixture'))
        self.assertIn('| all | 10.00% | 10.00% | 0.00% |', report)

    def test_tampered_source_and_validation_refused(self):
        self.source.write_text('t,v\n0,0\n.1,0\n')
        with self.assertRaisesRegex(ValueError, 'hash mismatch'):
            f.analyze(self.manifest)
        self.source.write_text('t,v\n0,0.1\n0.1,0.2\n')
        self.validation.write_text('{}')
        with self.assertRaisesRegex(ValueError, 'hash mismatch'):
            f.analyze(self.manifest)

    def test_quantity_cannot_imply_physical_torque(self):
        self.spec['quantity'] = 'measured-Nm'
        self.save()
        with self.assertRaisesRegex(ValueError, 'quantity'):
            f.analyze(self.manifest)

    def test_pinned_reference_preserves_statistics_and_records_full_profile(self):
        baseline = f.analyze(self.manifest)
        reference = self.root / 'reference.json'
        profile = dict(schema='dbce.ffb-normalization-reference', version=1,
                       id='art-50', game='Art of Rally', modelVersion='0.4.1',
                       sourceCommit='fixture', strengthPercent=50, effects=[{'durationMs': 120}])
        reference.write_text(json.dumps(profile))
        manifest = json.loads(self.manifest.read_text())
        manifest['reference'] = dict(path=reference.name, sha256=hashlib.sha256(reference.read_bytes()).hexdigest())
        self.manifest.write_text(json.dumps(manifest))
        report = f.analyze(self.manifest)
        self.assertEqual(baseline['series'], report['series'])
        self.assertEqual(report['reference']['profile'], profile)
        self.assertIn('Art of Rally at Strength 50', f.markdown(report))
        reference.write_text('{}')
        with self.assertRaisesRegex(ValueError, 'hash mismatch'):
            f.analyze(self.manifest)

    def test_reference_schema_and_strength_refused_even_when_hash_matches(self):
        for changes in ({'version': 9}, {'strengthPercent': True}, {'strengthPercent': 0},
                        {'strengthPercent': float('nan')}, {'sourceCommit': ''}):
            profile = dict(schema='dbce.ffb-normalization-reference', version=1,
                           id='art-50', game='Art of Rally', modelVersion='0.4.1',
                           sourceCommit='fixture', strengthPercent=50)
            profile.update(changes)
            reference = self.root / 'reference.json'
            reference.write_text(json.dumps(profile))
            manifest = json.loads(self.manifest.read_text())
            manifest['reference'] = dict(path=reference.name, sha256=hashlib.sha256(reference.read_bytes()).hexdigest())
            self.manifest.write_text(json.dumps(manifest))
            with self.assertRaises(ValueError):
                f.analyze(self.manifest)

    def test_complete_session_and_dropped_sample_refusal(self):
        p = self.root / 'session.jsonl'
        records = [{'metadata': {}}, *[{'sample': {'sequence': i, 'elapsedSeconds': i*.1, 'channels': {'v': .2}}} for i in range(2)],
                   {'footer': {'completed': True, 'counts': {'writtenSamples': 2, 'droppedSamples': 0, 'droppedMarkers': 0, 'invalidCount': 0, 'errorCount': 0, 'queueFullCount': 0, 'limitCount': 0, 'contentionCount': 0}}}]
        spec = dict(format='session-jsonl', mapping={'time': '$elapsed', 'value': 'v'})
        def write(rs):
            p.write_text(''.join(json.dumps(r)+'\n' for r in rs), encoding='utf-8')
        write(records)
        self.assertEqual(len(f.read_rows(p, spec)), 2)
        write(records[:-1])
        with self.assertRaisesRegex(ValueError, 'incomplete'):
            f.read_rows(p, spec)
        records[-1]['footer']['counts']['droppedSamples'] = 1
        write(records)
        with self.assertRaisesRegex(ValueError, 'lost data'):
            f.read_rows(p, spec)


if __name__ == '__main__':
    unittest.main()
