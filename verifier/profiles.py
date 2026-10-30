"""Explicit allow-list of existing independent simulator runner entrypoints.

The profiles describe observed narrow test capabilities, not complete UMAA
service compliance. Test execution never imports SDK or simulator oracle code.
"""
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Optional, Tuple


@dataclass(frozen=True)
class Profile:
    id: str
    runner: str
    scope: str
    transport: str
    needs_oracle: bool = False
    needs_scenario: bool = False
    needs_module: bool = False
    choices: Tuple[str, ...] = ()


PROFILES: Dict[str, Profile] = {
    "stdio-fixture": Profile(
        "stdio-fixture", "simulator/src/run.py",
        "Development-only line-protocol scenario and independent trace oracle; not DDS",
        "stdio-fixture", needs_scenario=True,
    ),
    "dds-engine": Profile(
        "dds-engine", "simulator/dds/run.py",
        "Selected real Cyclone DDS EngineControl simulator integration",
        "cyclonedds", needs_oracle=True, needs_module=True,
        choices=("--bad-destination",),
    ),
    "dds-anchor": Profile(
        "dds-anchor", "simulator/dds/run_anchor_control.py",
        "Selected real Cyclone DDS AnchorControl command/ack/status scenarios",
        "cyclonedds", needs_oracle=True,
        choices=("--update", "--stop", "--bad-destination"),
    ),
    "dds-anchor-extended": Profile(
        "dds-anchor-extended", "simulator/dds/run_anchor_control_extended.py",
        "Selected real Cyclone DDS AnchorControl extended virtual service scenarios",
        "cyclonedds", needs_oracle=True,
        choices=("--update", "--raise", "--bad-destination", "--complete", "--fail", "--timeout"),
    ),
    "dds-anchor-specs": Profile(
        "dds-anchor-specs", "simulator/dds/run_anchor_specs.py",
        "Selected real Cyclone DDS AnchorSpecs report value/update/dispose scenario",
        "cyclonedds", needs_oracle=True,
    ),
    "dds-reviewed-discovery": Profile(
        "dds-reviewed-discovery", "simulator/dds/run_reviewed_discovery.py",
        "Selected real Cyclone DDS service discovery/typed-wire scenarios, not full semantics",
        "cyclonedds", needs_oracle=True,
        choices=("--wire", "--semantic"),
    ),
}


def command_for(profile: Profile, repo_root: Path, sut: Path, oracle: Optional[Path],
                scenario: Optional[Path], module: Optional[Path], option: Optional[str],
                raw_report: Path, python_executable: str) -> list[str]:
    """Resolve to argv without shell interpretation; reject unsupported profiles/options."""
    if profile.id not in PROFILES or PROFILES[profile.id] != profile:
        raise ValueError("Unrecognized/modified verification profile")
    if option is not None and option not in profile.choices:
        raise ValueError(f"Unsupported option for {profile.id}: {option}")
    if profile.id == 'stdio-fixture' and scenario is not None:
        allowed_dir = (repo_root / 'simulator' / 'scenarios').resolve()
        if scenario.resolve().parent != allowed_dir or scenario.suffix != '.json':
            raise ValueError('stdio scenario must be a repository simulator/scenarios JSON fixture')
    if profile.needs_oracle and oracle is None:
        raise ValueError("--oracle is required")
    if profile.needs_scenario and scenario is None:
        raise ValueError("--scenario is required")
    if profile.needs_module and module is None:
        raise ValueError("--module is required")
    for label, path in (("SUT", sut), ("oracle", oracle if profile.needs_oracle else None),
                        ("scenario", scenario if profile.needs_scenario else None),
                        ("module", module if profile.needs_module else None)):
        if path is not None and not path.is_file():
            raise ValueError(f"{label} file not found: {path}")
    runner = repo_root / profile.runner
    if not runner.is_file():
        raise ValueError(f"Simulator runner not found: {runner}")
    cmd = [python_executable, str(runner)]
    if profile.id == "stdio-fixture":
        cmd += ["--sut", str(sut), "--scenario", str(scenario), "--report", str(raw_report)]
    elif profile.id == "dds-engine":
        cmd += [str(sut), str(oracle), str(module)]
    else:
        cmd += [str(sut), str(oracle)]
    if option is not None:
        cmd += [option]
    return cmd
