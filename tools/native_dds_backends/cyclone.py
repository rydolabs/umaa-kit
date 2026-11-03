"""Cyclone DDS TypeSupport compiler plugin; no SDK-orchestrator changes."""
from pathlib import Path
import shutil
import subprocess


def generate(root: Path, target: Path, closure: list[Path], output: Path,
             *, executable: str | None = None) -> dict:
    program = executable or "idlc"
    if shutil.which(program) is None:
        raise FileNotFoundError("native compiler unavailable: " + program)
    for dep in closure:
        relative = dep.relative_to(root)
        dest = output / relative.parent
        dest.mkdir(parents=True, exist_ok=True)
        subprocess.run([program, "-I", str(root), "-o", str(dest), str(dep)],
                       check=True, timeout=120)
    expected = output / target.relative_to(root).with_suffix(".c")
    if not expected.is_file() or not expected.with_suffix(".h").is_file():
        raise ValueError("Cyclone idlc did not generate target TypeSupport")
    sources = sorted(output.rglob("*.c"))
    if not sources:
        raise ValueError("Cyclone idlc generated no C sources")
    return {
        "sources": [str(p.relative_to(output)) for p in sources],
        "includeDirs": ["."],
        "language": "C",
        "cmakePackages": ["CycloneDDS CONFIG REQUIRED"],
        "cmakeLibraries": ["CycloneDDS::ddsc"],
        "cmakeFeatures": [],
        "cmakeProperties": {"C_STANDARD": "11"},
    }
