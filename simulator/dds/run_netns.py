"""Run real Cyclone DDS binaries across distinct Linux network namespaces.

Three namespace topology is provisioned by tools/dds_netns.sh.  Publisher and
simulator never share loopback; unicast DDS discovery is pinned to veth eth0.
"""
import argparse
import os
import re
import signal
import subprocess
import sys
import time
import tempfile
from pathlib import Path
import xml.etree.ElementTree as ET

NETEM_PROFILES = {
    # Applied in BOTH directions by configuring every participating eth0 egress.
    'degraded': ('delay', '40ms', '10ms', 'loss', '1%'),
    'outage-recover': ('loss', '100%'),
}

NODES = {
    'umaa_ci_pub': '10.233.77.11',
    'umaa_ci_sim1': '10.233.77.12',
    'umaa_ci_sim2': '10.233.77.13',
}


def cyclone_config(node):
    if node not in NODES:
        raise ValueError('Unknown DDS node: ' + node)
    peers = ''.join('<Peer Address="' + address + '"/>'
                    for name, address in NODES.items() if name != node)
    xml = ('<CycloneDDS xmlns="https://cdds.io/config"><Domain Id="any">'
           '<General><Interfaces><NetworkInterface name="eth0"/></Interfaces>'
           '<AllowMulticast>false</AllowMulticast></General>'
           '<Discovery><ParticipantIndex>0</ParticipantIndex><Peers>' + peers + '</Peers></Discovery>'
           '</Domain></CycloneDDS>')
    ET.fromstring(xml)
    return xml


def command_for_node(node, executable, *arguments):
    if node not in NODES:
        raise ValueError('Unknown DDS node: ' + node)
    return ['ip', 'netns', 'exec', node, executable, *arguments]


def confirm_isolation(nodes):
    seen = set()
    for node in nodes:
        r = subprocess.run(command_for_node(node, 'readlink', '/proc/self/ns/net'),
                           text=True, capture_output=True, timeout=5, check=True)
        ident = r.stdout.strip()
        if not ident.startswith('net:[') or ident in seen:
            raise RuntimeError('Network namespace not isolated: ' + node)
        seen.add(ident)
        r = subprocess.run(command_for_node(node, 'ip', '-4', 'addr', 'show', 'dev', 'eth0'),
                           text=True, capture_output=True, timeout=5, check=True)
        if NODES[node] not in r.stdout:
            raise RuntimeError('Missing veth IP: ' + node)
    print('DDS_NETNS_ISOLATION_PASS: ' + ', '.join(nodes), flush=True)


DISPOSE_RECEIPT = b'NATIVE_BATTERY_REPORT_DISPOSE_OBSERVED\n'


def confirmed_dispose_receipts(directory, nodes):
    """Pure test-harness evidence gate: all *independent* readers proved
    exact original 20/80/Dispose in their own process before Writer retires.

    No transport ACK is inferred and a missing, wrong or stale marker is a
    hard failure. The containing temporary directory is unique per run.
    """
    root = Path(directory)
    for node in nodes:
        if node not in NODES or node == 'umaa_ci_pub':
            raise ValueError('not an independent DDS reader node: ' + node)
        path = root / (node + '.receipt')
        try:
            if path.read_bytes() != DISPOSE_RECEIPT:
                return False
        except FileNotFoundError:
            return False
    return bool(nodes)


def spawn(node, executable, *arguments, readers=1, recovery=False,
          impaired=False, receipt_dir=None, suppress_first_native_dispose=False):
    env = dict(os.environ)
    env.pop('UMAA_NETEM_SUPPRESS_FIRST_NATIVE_DISPOSE', None)
    if suppress_first_native_dispose:
        if node == 'umaa_ci_pub' or not receipt_dir:
            raise ValueError('native Dispose probe requires independent receipt-gated reader')
        env['UMAA_NETEM_SUPPRESS_FIRST_NATIVE_DISPOSE'] = '1'
    env['CYCLONEDDS_URI'] = cyclone_config(node)
    env['UMAA_EXPECT_READERS'] = str(readers)
    if recovery:
        env['UMAA_NETEM_RECOVERY'] = '1'
    # The independent Reader still must prove actual DATA and keyed Dispose.
    # Retain the real Writer briefly after its final reliability ACK while
    # the netem test subjects it to jitter, loss or discovery blackout.
    if recovery or impaired:
        env['UMAA_NETEM_RETAIN_FINAL'] = '1'
    if receipt_dir:
        if node == 'umaa_ci_pub':
            env['UMAA_NETEM_RECEIPT_DIR'] = receipt_dir
        else:
            env['UMAA_NETEM_RECEIPT_FILE'] = str(
                Path(receipt_dir) / (node + '.receipt'))
    return subprocess.Popen(command_for_node(node, executable, *arguments),
                            env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, start_new_session=True)


def tc_command(node, *arguments):
    return command_for_node(node, 'tc', 'qdisc', *arguments)


def tc_run(node, *arguments):
    proc = subprocess.run(tc_command(node, *arguments), check=True, text=True,
                          capture_output=True, timeout=10)
    return proc.stdout


def set_netem(nodes, profile):
    settings = NETEM_PROFILES[profile]
    for node in nodes:
        tc_run(node, 'replace', 'dev', 'eth0', 'root', 'netem', *settings)
        status = tc_run(node, 'show', 'dev', 'eth0')
        if 'netem' not in status:
            raise RuntimeError('netem qdisc not present in ' + node)
        print('DDS_NETEM_APPLIED ' + node + ' ' + profile + ': ' + status.strip(),
              flush=True)


def parse_netem_drops(stats):
    match = re.search(r'dropped[ ]+([0-9]+)', stats)
    if not match:
        raise ValueError('tc netem counters absent: ' + stats)
    return int(match.group(1))


def inspect_outage(nodes):
    drops = 0
    for node in nodes:
        status = tc_run(node, 'show', 'dev', 'eth0')
        # tc -s qdisc show is needed for kernel counters (and differs from tc qdisc show).
        proc = subprocess.run(command_for_node(node, 'tc', '-s', 'qdisc', 'show',
                                                'dev', 'eth0'),
                              check=True, text=True, capture_output=True, timeout=10)
        stats = proc.stdout
        if 'netem' not in status:
            raise RuntimeError('no netem qdisc in ' + node + ': ' + stats)
        drops += parse_netem_drops(stats)
        print('DDS_NETEM_OUTAGE_STATS ' + node + ': ' + stats.strip(), flush=True)
    if drops < 1:
        raise RuntimeError('outage was configured but no packet drops were observed')
    print('DDS_NETEM_OUTAGE_CONFIRMED dropped_packets=' + str(drops), flush=True)


def clear_netem(nodes):
    for node in nodes:
        tc_run(node, 'del', 'dev', 'eth0', 'root')
        status = tc_run(node, 'show', 'dev', 'eth0')
        if 'netem' in status:
            raise RuntimeError('netem still applied in ' + node)
        print('DDS_NETEM_CLEARED ' + node, flush=True)


def stop(proc):
    """Reap a namespace actor even after an earlier communicate() timed out.

    The first communicate() may have already closed stdout; a second call
    then raises ValueError and masks the *real* missing DDS Dispose failure.
    Do not turn a missing event into success, and bound process cleanup.
    """
    if proc.poll() is None:
        os.killpg(proc.pid, signal.SIGKILL)
    if proc.stdout is not None and proc.stdout.closed:
        proc.wait(timeout=5)
        return
    try:
        proc.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        if proc.poll() is None:
            os.killpg(proc.pid, signal.SIGKILL)
        proc.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host')
    parser.add_argument('simulator')
    parser.add_argument('module')
    parser.add_argument('--subscribers', type=int, choices=(1, 2), default=1)
    parser.add_argument('--update', action='store_true')
    parser.add_argument('--require-observed-dispose', action='store_true',
                        help='only for original Battery Report: require independent DDS Dispose receipts')
    parser.add_argument('--probe-native-reassert', action='store_true',
                        help='test-only: withhold FIRST genuinely observed Dispose and require another')
    parser.add_argument('--profile', choices=('clean', *NETEM_PROFILES), default='clean',
                        help='network impairment; outage-recover blocks DDS discovery then heals')
    args = parser.parse_args()
    if args.update and args.subscribers != 1:
        parser.error('update requires one EngineControl simulator')
    if args.require_observed_dispose and args.profile == 'clean':
        parser.error('Dispose observation proof requires an impaired DDS profile')
    if args.require_observed_dispose and args.update:
        parser.error('Battery Report Dispose observation cannot check Engine UPDATE')
    if args.probe_native_reassert and (
            args.profile != 'degraded' or not args.require_observed_dispose
            or args.subscribers != 1 or args.update):
        parser.error('native Dispose reassert probe requires one impaired Battery reader and strict receipt')
    sims = []
    host = None
    sim_nodes = ['umaa_ci_sim1'] + (['umaa_ci_sim2'] if args.subscribers == 2 else [])
    nodes = ['umaa_ci_pub', *sim_nodes]
    confirm_isolation(nodes)
    netem_active = False
    # Each fault-injection run, including the three degraded repetitions,
    # has a fresh private receipt directory so no earlier PASS can be reused.
    receipt_dir = tempfile.TemporaryDirectory(prefix='umaa-dds-receipts-') \
        if args.require_observed_dispose else None
    try:
        if args.profile != 'clean':
            # Even rootless unit tests can check the exact command composition.
            netem_active = True
            set_netem(nodes, args.profile)
        for node in sim_nodes:
            sims.append(spawn(node, args.simulator, *(['--update'] if args.update else []),
                              recovery=args.profile == 'outage-recover',
                              impaired=args.profile != 'clean',
                              receipt_dir=receipt_dir.name if receipt_dir else None,
                              suppress_first_native_dispose=args.probe_native_reassert))
        time.sleep(0.8)
        host = spawn('umaa_ci_pub', args.host, args.module, readers=args.subscribers,
                     recovery=args.profile == 'outage-recover',
                     impaired=args.profile != 'clean',
                     receipt_dir=receipt_dir.name if receipt_dir else None)
        if args.profile == 'outage-recover':
            # During blackout all participating egress qdiscs drop every packet.
            # Heal while DDS processes are alive, not between test runs.
            time.sleep(1.4)
            if host.poll() is not None or any(sim.poll() is not None for sim in sims):
                raise RuntimeError('DDS process terminated during deliberate outage')
            inspect_outage(nodes)
            clear_netem(nodes)
            netem_active = False
            print('DDS_NETEM_RECOVERY_STARTED: all nodes restored', flush=True)
        out, _ = host.communicate(timeout=55)
        print('HOST OUTPUT:\n' + out, flush=True)
        if host.returncode != 0:
            for node, sim in zip(sim_nodes, sims):
                if sim.poll() is not None:
                    out, _ = sim.communicate()
                    print(node + ' EARLY OUTPUT:\n' + out, flush=True)
            raise RuntimeError('host returned ' + str(host.returncode))
        for node, sim in zip(sim_nodes, sims):
            out, _ = sim.communicate(timeout=55)
            print(node + ' OUTPUT:\n' + out, flush=True)
            if sim.returncode != 0 or not any('PASS' in line and 'SIM:' in line
                                               for line in out.splitlines()):
                raise RuntimeError(node + ': simulator failed, rc=' + str(sim.returncode))
        if receipt_dir is not None:
            if not confirmed_dispose_receipts(receipt_dir.name, sim_nodes):
                raise RuntimeError('missing independently verified keyed DDS Dispose receipts')
            print('DDS_NETEM_INDEPENDENT_DISPOSE_RECEIPTS_PASS: ' +
                  ', '.join(sim_nodes), flush=True)
        if args.profile == 'outage-recover':
            print('DDS_NETEM_RECOVERY_PASS: DDS wire data observed after 100% loss', flush=True)
        elif args.profile == 'degraded':
            print('DDS_NETEM_DEGRADED_PASS: DDS wire test survived configured delay/loss',
                  flush=True)
        print('DDS_NETNS_WIRE_PASS: isolated nodes=' + str(1 + len(sims)) +
              ' profile=' + args.profile, flush=True)
        return 0
    except (subprocess.TimeoutExpired, RuntimeError, subprocess.CalledProcessError) as exc:
        print('DDS_NETNS_WIRE_FAIL: ' + str(exc), file=sys.stderr, flush=True)
        return 1
    finally:
        for proc in ([host] if host else []) + sims:
            stop(proc)
        if netem_active:
            clear_netem(nodes)
        if receipt_dir is not None:
            receipt_dir.cleanup()


if __name__ == '__main__':
    sys.exit(main())
