import unittest
import xml.etree.ElementTree as ET
import subprocess
import sys
import tempfile
from pathlib import Path
from unittest.mock import patch
from run_netns import (
    NODES, NETEM_PROFILES, DISPOSE_RECEIPT, cyclone_config, command_for_node,
    tc_command, parse_netem_drops, confirmed_dispose_receipts, spawn, stop
)


class NetnsConfigurationTests(unittest.TestCase):
    def test_three_unique_addresses(self):
        self.assertEqual(len(NODES), 3)
        self.assertEqual(len(set(NODES.values())), 3)

    def test_discovery_excludes_self_and_loopback(self):
        for node, address in NODES.items():
            doc = ET.fromstring(cyclone_config(node))
            ns = {'c': 'https://cdds.io/config'}
            self.assertEqual(doc.findtext('.//c:AllowMulticast', namespaces=ns), 'false')
            peers = [p.attrib['Address'] for p in doc.findall('.//c:Peer', ns)]
            self.assertEqual(set(peers), set(NODES.values()) - {address})
            self.assertTrue(all(not p.startswith('127.') for p in peers))

    def test_tc_netem_kernel_drop_statistics(self):
        sample = ('qdisc netem 8006: root refcnt 5 limit 1000 loss 100%\\n'
                  ' Sent 0 bytes 0 pkt (dropped 37, overlimits 0 requeues 0)')
        self.assertEqual(parse_netem_drops(sample), 37)
        self.assertEqual(parse_netem_drops('Sent 1 bytes 1 pkt (dropped 0, overlimits 0)'), 0)
        with self.assertRaises(ValueError):
            parse_netem_drops('qdisc noqueue 0: root')

    def test_uses_tc_netem_egress_on_isolated_veth(self):
        self.assertEqual(tc_command('umaa_ci_pub', 'replace', 'dev', 'eth0',
                                    'root', 'netem', *NETEM_PROFILES['degraded']),
                         ['ip', 'netns', 'exec', 'umaa_ci_pub', 'tc', 'qdisc',
                          'replace', 'dev', 'eth0', 'root', 'netem',
                          'delay', '40ms', '10ms', 'loss', '1%'])
        self.assertEqual(NETEM_PROFILES['outage-recover'], ('loss', '100%'))
        with self.assertRaises(ValueError):
            tc_command('host-network', 'show')

    def test_observer_receipts_require_two_independent_real_disposes(self):
        with tempfile.TemporaryDirectory() as base:
            directory = Path(base)
            nodes = ['umaa_ci_sim1', 'umaa_ci_sim2']
            self.assertFalse(confirmed_dispose_receipts(directory, nodes))
            (directory / 'umaa_ci_sim1.receipt').write_bytes(DISPOSE_RECEIPT)
            self.assertFalse(confirmed_dispose_receipts(directory, nodes))
            (directory / 'umaa_ci_sim2.receipt').write_bytes(b'DDS_ACK_ONLY\\n')
            self.assertFalse(confirmed_dispose_receipts(directory, nodes))
            (directory / 'umaa_ci_sim2.receipt').write_bytes(DISPOSE_RECEIPT)
            self.assertTrue(confirmed_dispose_receipts(directory, nodes))
            # Reusing an earlier PASS is forbidden by the unique directory.
            with tempfile.TemporaryDirectory() as other:
                self.assertFalse(confirmed_dispose_receipts(other, nodes))
            (directory / 'umaa_ci_sim1.receipt').write_bytes(DISPOSE_RECEIPT +
                                                              b'forged-extra')
            self.assertFalse(confirmed_dispose_receipts(directory, nodes))
            with self.assertRaises(ValueError):
                confirmed_dispose_receipts(directory, ['umaa_ci_pub'])

    def test_receipt_path_only_for_opt_in_impaired_namespace_process(self):
        with patch('run_netns.subprocess.Popen') as subprocess_spawn:
            spawn('umaa_ci_pub', '/publisher', '/module', readers=2,
                  impaired=True, receipt_dir='/unique/session')
            options = subprocess_spawn.call_args.kwargs
            self.assertEqual(options['env']['UMAA_NETEM_RECEIPT_DIR'],
                             '/unique/session')
            self.assertNotIn('UMAA_NETEM_RECEIPT_FILE', options['env'])
            spawn('umaa_ci_sim2', '/independent-reader', impaired=True,
                  receipt_dir='/unique/session')
            options = subprocess_spawn.call_args.kwargs
            self.assertEqual(options['env']['UMAA_NETEM_RECEIPT_FILE'],
                             '/unique/session/umaa_ci_sim2.receipt')
            self.assertNotIn('UMAA_NETEM_RECEIPT_DIR', options['env'])

    def test_cleanup_after_communicate_closed_pipe_preserves_original_failure(self):
        # Reproduce the observed failure path: a native DDS Writer exits
        # nonzero after missing a real Dispose receipt; its stdout was
        # already consumed and closed by the supervising code.
        with subprocess.Popen(
            [sys.executable, '-c', 'import sys; sys.exit(4)'],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
            start_new_session=True,
        ) as proc:
            proc.communicate(timeout=5)
            self.assertEqual(proc.returncode, 4)
            proc.stdout.close()
            stop(proc)  # must not raise ValueError or overwrite rc=4
            self.assertEqual(proc.returncode, 4)

    def test_cleanup_reaps_unfinished_process_without_fabricated_success(self):
        with subprocess.Popen(
            [sys.executable, '-c', 'import time; time.sleep(30)'],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
            start_new_session=True,
        ) as proc:
            stop(proc)
            self.assertIsNotNone(proc.returncode)
            self.assertNotEqual(proc.returncode, 0)

    def test_first_dispose_fault_injection_never_leaks_into_publisher_or_normal_reader(self):
        with patch('run_netns.subprocess.Popen') as child:
            spawn('umaa_ci_sim1', '/original-reader', impaired=True,
                  receipt_dir='/private/fresh-session',
                  suppress_first_native_dispose=True)
            reader_env = child.call_args.kwargs['env']
            self.assertEqual(reader_env['UMAA_NETEM_SUPPRESS_FIRST_NATIVE_DISPOSE'], '1')
            self.assertEqual(reader_env['UMAA_NETEM_RECEIPT_FILE'],
                             '/private/fresh-session/umaa_ci_sim1.receipt')
            spawn('umaa_ci_sim2', '/normal-reader', impaired=True,
                  receipt_dir='/private/fresh-session')
            self.assertNotIn('UMAA_NETEM_SUPPRESS_FIRST_NATIVE_DISPOSE',
                             child.call_args.kwargs['env'])
            with self.assertRaises(ValueError):
                spawn('umaa_ci_pub', '/publisher', impaired=True,
                      receipt_dir='/private/fresh-session',
                      suppress_first_native_dispose=True)
            with self.assertRaises(ValueError):
                spawn('umaa_ci_sim1', '/reader-no-receipt',
                      suppress_first_native_dispose=True)

    def test_uses_netns_exec(self):
        self.assertEqual(command_for_node('umaa_ci_pub', '/bin/true'),
                         ['ip', 'netns', 'exec', 'umaa_ci_pub', '/bin/true'])
        with self.assertRaises(ValueError):
            cyclone_config('host-network')


if __name__ == '__main__':
    unittest.main()
