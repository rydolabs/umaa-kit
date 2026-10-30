"""Trusted LOCAL offline process supervisor for independent legacy simulator tests.

NOT a multi-user or hardened execution service. The called SUT/oracle binaries
must be trusted by the operator; never expose this API with arbitrary paths to
untrusted web clients. PASS has a narrow profile-specific meaning only.
"""
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import signal
import shutil
import subprocess
import sys
import time
import uuid
from typing import Optional

from .contracts import REGISTRY, load_contracts, sha256
from .assertions import success_errors
from .evidence import write_index
from .profiles import PROFILES, command_for

ROOT = Path(__file__).resolve().parents[1]
MAX_LOG_BYTES = 8 * 1024 * 1024


def _limit_output():
    """Best-effort POSIX limit on each log; not a security sandbox."""
    import resource
    resource.setrlimit(resource.RLIMIT_FSIZE, (MAX_LOG_BYTES, MAX_LOG_BYTES))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def _input_info(path: Optional[Path]) -> Optional[dict]:
    if path is None:
        return None
    p = path.resolve()
    return {'path': str(p), 'sha256': sha256(p) if p.is_file() else None,
            'bytes': p.stat().st_size if p.is_file() else None}


def run(profile_id: str, sut: Path, output_dir: Path, *, oracle: Optional[Path] = None,
        scenario: Optional[Path] = None, module: Optional[Path] = None,
        option: Optional[str] = None, timeout: int = 90, repo_root: Path = ROOT,
        dds_network_approved: bool = False) -> dict:
    """Execute one allow-listed profile; no evidence overwrite; leave result on failure."""
    if profile_id not in PROFILES:
        raise ValueError('Unknown profile: ' + profile_id)
    if type(timeout) is not int or not 1 <= timeout <= 180:
        raise ValueError('Timeout must be 1..180 seconds')
    profile = PROFILES[profile_id]
    contracts = load_contracts()
    contract = contracts[profile_id]
    output_dir.mkdir(parents=True, exist_ok=False)
    raw_report = output_dir / 'simulator-result.json'
    scenario_snapshot = output_dir / 'scenario.snapshot.json'
    stdout_file = output_dir / 'runner.stdout.log'
    stderr_file = output_dir / 'runner.stderr.log'
    started = time.monotonic()
    start_time = datetime.now(timezone.utc).isoformat()
    result, return_code, message = 'BLOCKED', None, None
    proc = None
    runner_path = repo_root / profile.runner
    # Hashes are computed once before execution; modifying a binary concurrently
    # is out of scope for this trusted offline prototype.
    inputs = {}
    try:
        # Explicit opt-in is required before any real-wire DDS subprocess starts.
        # This is an operator declaration, NOT technical network isolation.
        if profile.transport == 'cyclonedds' and dds_network_approved is not True:
            raise ValueError('DDS test network approval required: use --dds-network-approved '
                             'only on an isolated, authorized test network')
        command = command_for(profile, repo_root, sut, oracle, scenario, module,
                              option, raw_report, sys.executable)
        inputs = {key: _input_info(p) for key, p in (
            ('sut', sut), ('oracle', oracle if profile.needs_oracle else None),
            ('module', module if profile.needs_module else None),
            ('scenario', scenario if profile.needs_scenario else None),
            ('runner', runner_path), ('contractRegistry', REGISTRY)) if p is not None}
        if profile.needs_scenario:
            shutil.copyfile(scenario, scenario_snapshot)
            if sha256(scenario_snapshot) != inputs['scenario']['sha256']:
                raise ValueError('scenario changed while snapshot was created')
        with stdout_file.open('wb') as out, stderr_file.open('wb') as err:
            kwargs = {'cwd': str(repo_root), 'stdin': subprocess.DEVNULL,
                      'stdout': out, 'stderr': err}
            if os.name == 'posix':
                kwargs.update(start_new_session=True, preexec_fn=_limit_output)
            proc = subprocess.Popen(command, **kwargs)
            try:
                return_code = proc.wait(timeout=timeout)
                result = 'PASS' if return_code == 0 else 'FAIL'
                if return_code == 0:
                    failures = success_errors(profile.id, option, stdout_file, raw_report,
                                              scenario_snapshot if profile.needs_scenario else None)
                    if failures:
                        result, message = 'FAIL', '; '.join(failures)
                if return_code != 0:
                    message = f'Simulator supervisor exited with code {return_code}'
            except subprocess.TimeoutExpired:
                result = 'ERROR'
                message = f'Supervisor exceeded {timeout}s; process group terminated where supported'
                try:
                    if os.name == 'posix':
                        os.killpg(proc.pid, signal.SIGKILL)
                    else:
                        proc.kill()
                except ProcessLookupError:
                    pass
                proc.wait()
                return_code = proc.returncode
        if any(p.exists() and p.stat().st_size >= MAX_LOG_BYTES
               for p in (stdout_file, stderr_file)):
            result, message = 'ERROR', 'Log limit reached; result not reliable'
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        result = 'BLOCKED' if isinstance(exc, ValueError) else 'ERROR'
        message = str(exc)
    except KeyboardInterrupt:
        result, message = 'ERROR', 'Interrupted locally; process terminated'
        if proc is not None and proc.poll() is None:
            if os.name == 'posix':
                os.killpg(proc.pid, signal.SIGKILL)
            else:
                proc.kill()
            proc.wait()
            return_code = proc.returncode
    finally:
        artifacts = {}
        for name, path in (('stdout', stdout_file), ('stderr', stderr_file),
                           ('simulatorReport', raw_report), ('scenarioSnapshot', scenario_snapshot)):
            if path.is_file():
                artifacts[name] = {'file': path.name, 'sha256': sha256(path),
                                   'bytes': path.stat().st_size}
        # Always provide log evidence, including pre-launch validation failures.
        for name, path in (('stdout', stdout_file), ('stderr', stderr_file)):
            if not path.exists():
                path.write_bytes(b'')
                artifacts[name] = {'file': path.name, 'sha256': sha256(path), 'bytes': 0}
        data = {
            'schemaVersion': '1.1', 'runId': uuid.uuid4().hex,
            'profileId': profile.id, 'transport': profile.transport,
            'scope': profile.scope, 'conformanceClaim': False,
            'ddsNetworkApprovalDeclared': dds_network_approved is True if profile.transport == 'cyclonedds' else False,
            'contract': {'contractId': contract['contractId'],
                         'basis': contract['basis'], 'reviewStatus': 'development-unreviewed',
                         'normativeRequirementRefs': [], 'conformanceEligible': False},
            'result': result, 'exitCode': return_code, 'startedAtUtc': start_time,
            'durationMs': round((time.monotonic() - started) * 1000),
            'runner': profile.runner, 'option': option,
            'inputs': inputs, 'artifacts': artifacts, 'message': message,
        }
        (output_dir / 'verdict.json').write_text(
            json.dumps(data, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
        write_index(output_dir)
    return data
