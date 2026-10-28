"""Trusted offline CLI for scoped UMAA simulator-driven verification."""
import argparse
import json
from pathlib import Path
import sys

from .contracts import load_contracts, load_suite
from .evidence import verify_run, verify_suite
from .profiles import PROFILES
from .report import render
from .exchange import export_results
from .scope import assess_scope
from .runner import run
from .suite import run_suite

EXIT_CODE = {'PASS': 0, 'FAIL': 1, 'BLOCKED': 2, 'ERROR': 3}


def _binary_args(p):
    p.add_argument('--sut', required=True, type=Path, help='prebuilt trusted SUT executable')
    p.add_argument('--oracle', type=Path, help='prebuilt independent DDS oracle executable')
    p.add_argument('--module', type=Path, help='prebuilt engine service module')
    p.add_argument('--output-dir', required=True, type=Path, help='new output directory (no overwrite)')


def main(argv=None):
    p = argparse.ArgumentParser(description='UMAA local verifier (NOT official conformance)')
    sub = p.add_subparsers(dest='action', required=True)
    sub.add_parser('list', help='list narrowly scoped, registered simulator profiles')
    scope = sub.add_parser('scope-report', help='report unmapped normative requirements and blocked acceptance gate')
    scope.add_argument('--require-ready', action='store_true', help='exit BLOCKED unless normative release gate is approved')
    cmd = sub.add_parser('run', help='execute one allow-listed simulator profile')
    _binary_args(cmd)
    cmd.add_argument('--profile', required=True, choices=tuple(PROFILES))
    cmd.add_argument('--scenario', type=Path, help='existing stdio fixture scenario')
    cmd.add_argument('--option', help='profile-specific allow-listed scenario switch')
    cmd.add_argument('--timeout', type=int, default=90)
    cmd.add_argument('--dds-network-approved', action='store_true', help='operator declares an authorized isolated DDS test network; NOT isolation enforcement')
    s = sub.add_parser('run-suite', help='execute a validated suite with separate case evidence')
    _binary_args(s)
    s.add_argument('--suite', required=True, type=Path)
    s.add_argument('--dds-network-approved', action='store_true', help='operator declares an authorized isolated DDS test network; NOT isolation enforcement')
    v = sub.add_parser('verify-evidence', help='check existing run/suite evidence consistency')
    v.add_argument('--evidence-dir', required=True, type=Path)
    r = sub.add_parser('render-report', help='render verified evidence to a static offline HTML file')
    r.add_argument('--evidence-dir', required=True, type=Path)
    r.add_argument('--output-file', required=True, type=Path)
    e = sub.add_parser('export-results', help='verified read-only JSON for Studio/other tools')
    e.add_argument('--evidence-dir', required=True, type=Path)
    e.add_argument('--output-file', required=True, type=Path)
    a = p.parse_args(argv)
    try:
        if a.action == 'scope-report':
            status = assess_scope()
            print(json.dumps(status, indent=2, ensure_ascii=False))
            return 2 if a.require_ready and status['gateStatus'] != 'READY' else 0
        if a.action == 'list':
            contracts = load_contracts()
            for key, profile in PROFILES.items():
                print(f'{key:24} {profile.transport:13} {contracts[key]["contractId"]:34} {profile.scope}')
            return 0
        if a.action == 'export-results':
            output = export_results(a.evidence_dir, a.output_file)
            print('Verified non-normative result exchange: ' + str(output))
            return 0
        if a.action == 'render-report':
            output = render(a.evidence_dir, a.output_file)
            print('Offline evidence report: ' + str(output))
            return 0
        if a.action == 'verify-evidence':
            check = verify_suite if (a.evidence_dir / 'suite-verdict.json').exists() else verify_run
            errors = check(a.evidence_dir)
            if errors:
                for e in errors:
                    print('EVIDENCE ERROR: ' + e, file=sys.stderr)
                return 3
            print('INTEGRITY OK (digest consistency only; NOT authenticity or UMAA conformance)')
            return 0
        common = {'sut': a.sut.resolve(), 'output_dir': a.output_dir,
                  'oracle': a.oracle.resolve() if a.oracle else None,
                  'module': a.module.resolve() if a.module else None}
        if a.action == 'run':
            result = run(a.profile, scenario=a.scenario.resolve() if a.scenario else None,
                         option=a.option, timeout=a.timeout,
                         dds_network_approved=a.dds_network_approved, **common)
            print(f'{result["result"]} {result["profileId"]} (conformanceClaim=false)')
            print('Evidence: ' + str(a.output_dir / 'verdict.json'))
            if result['message']:
                print(result['message'], file=sys.stderr)
            return EXIT_CODE[result['result']]
        if a.action == 'run-suite':
            load_suite(a.suite)
            summary = run_suite(a.suite, dds_network_approved=a.dds_network_approved, **common)
            print(f'{summary["result"]} {summary["suiteId"]} ({summary["counts"]}; '
                  'conformanceClaim=false)')
            print('Evidence: ' + str(a.output_dir / 'suite-verdict.json'))
            return EXIT_CODE[summary['result']]
    except (OSError, ValueError, TypeError) as exc:
        print('VERIFIER SETUP ERROR: ' + str(exc), file=sys.stderr)
        return 2
    return 3
