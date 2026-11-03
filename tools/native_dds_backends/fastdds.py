"""Fast DDS-Gen TypeSupport compiler plugin; owns its flat-output rules."""
from pathlib import Path
import shutil
import subprocess


def generate(root: Path, target: Path, closure: list[Path], output: Path,
             *, executable: str | None = None) -> dict:
    program = executable or "fastddsgen"
    if shutil.which(program) is None:
        raise FileNotFoundError("native compiler unavailable: " + program)
    names = [p.stem for p in closure]
    if len(set(names)) != len(names):
        raise ValueError("Fast DDS-Gen basename collision in IDL closure")
    output.mkdir(parents=True, exist_ok=True)
    for dep in closure:
        subprocess.run([program, "-replace", "-I", str(root), "-d", str(output),
                        str(dep)], check=True, timeout=180)
    # Generated headers retain relative source includes even though the
    # vendor emits flat filenames. Mirror ONLY generated headers; do not
    # redistribute input IDLs or duplicate .cxx translation units.
    for dep in closure:
        directory = output / dep.relative_to(root).parent
        if directory == output:
            continue
        directory.mkdir(parents=True, exist_ok=True)
        for header in output.iterdir():
            if (header.is_file() and header.name.startswith(dep.stem)
                    and header.suffix in (".h", ".hpp", ".ipp")):
                shutil.copy2(header, directory / header.name)
    sources = sorted(output.rglob("*.cxx"))
    if not any(p.name.startswith(target.stem) and "PubSubType" in p.name
               for p in sources):
        raise ValueError("Fast DDS-Gen missing target PubSubType")
    includes = {output}
    for header in output.rglob("*"):
        if header.is_file() and header.suffix in (".h", ".hpp", ".ipp"):
            parent = header.parent
            while parent != output and output in parent.parents:
                includes.add(parent)
                parent = parent.parent
    return {
        "sources": [str(p.relative_to(output)) for p in sources],
        "includeDirs": sorted(str(p.relative_to(output)) for p in includes),
        "language": "CXX",
        "cmakePackages": [],
        "cmakeLibraries": ["fastrtps", "fastcdr"],
        "cmakeFeatures": ["cxx_std_17"],
        "cmakeProperties": {},
    }
