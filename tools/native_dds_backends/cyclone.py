"""Cyclone DDS TypeSupport compiler plugin; no SDK-orchestrator changes."""
from pathlib import Path
import shutil
import subprocess


def available(*, executable: str | None = None) -> bool:
    return shutil.which(executable or "idlc") is not None


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


def type_probe(type_name: str, source: str, keys: list[str]) -> dict:
    """Compiler-checked C identity and key FIELD EXISTENCE (not @key policy)."""
    import re
    if not re.fullmatch(r"[A-Za-z_]\w*(?:::[A-Za-z_]\w*)+", type_name):
        raise ValueError("unsafe Cyclone native type identifier")
    if any(not re.fullmatch(r"[A-Za-z_]\w*", key) for key in keys):
        raise ValueError("unsafe Cyclone key field")
    native = type_name.replace("::", "_")
    header = Path(source).with_suffix(".h").as_posix()
    lines = [
        '#include "' + header + '"',
        'typedef char umaa_native_type_present[(sizeof(' + native + ') > 0) ? 1 : -1];',
    ]
    for idx, key in enumerate(keys):
        lines.append('typedef char umaa_native_field_' + str(idx) +
                     '[(sizeof(((' + native + '*)0)->' + key +
                     ') > 0) ? 1 : -1];')
    return {"language": "C", "extension": ".c", "code": "\n".join(lines) + "\n"}



def runtime_adapter(binding: dict) -> dict:
    """Emit real native TypeSupport to stable DDS C ABI Report bridge.

    This is explicitly a scalar synthetic fixture profile. The host uses
    bounded int32 samples; vendor DDS serialization is handled by the
    actual compiler TypeSupport, never by a Python parser or manual CDR.
    """
    from ._runtime_report import render
    reference = (Path(__file__).resolve().parents[2] /
                 "dds-backends/cyclone/src/adapter.cpp")
    return {
        "source": render(reference, binding, language="cyclone"),
        "extension": ".cpp",
        "language": "CXX",
    }
