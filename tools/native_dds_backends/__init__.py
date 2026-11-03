"""Trusted native DDS compiler backend interfaces and plugin discovery.

Built-ins are modules in this package. Third parties can publish entry points
in the 'umaa.native_dds_backends' group, requiring NO changes to SDK core,
Component CLI, ICD code generator, or generated project CMake.

An installed plugin executes arbitrary Python code. Install only trusted,
version-pinned packages. An IDL path, YAML text or generated output can NEVER
supply executable module names beyond this installed registry.
"""
from __future__ import annotations

import importlib
from importlib import metadata
import pkgutil
import re

BACKEND_ID = re.compile(r"^[a-z][a-z0-9_]{0,39}$")
GROUP = "umaa.native_dds_backends"


def _builtins() -> tuple[str, ...]:
    return tuple(sorted(m.name for m in pkgutil.iter_modules(__path__)
                        if not m.name.startswith("_")))


def _external() -> dict:
    registry = metadata.entry_points()
    candidates = registry.select(group=GROUP) if hasattr(registry, "select") else registry.get(GROUP, [])
    return {p.name: p for p in candidates}


def discover() -> tuple[str, ...]:
    builtins = set(_builtins())
    externals = _external()
    for name in externals:
        if not BACKEND_ID.fullmatch(name) or name in builtins:
            raise ValueError("invalid/duplicate installed DDS compiler entry point")
    return tuple(sorted(builtins | set(externals)))


def load(name: str):
    if not isinstance(name, str) or not BACKEND_ID.fullmatch(name):
        raise ValueError("unsafe DDS backend plugin name")
    if name not in discover():
        raise ValueError("unregistered DDS backend plugin: " + name)
    if name in _builtins():
        module = importlib.import_module(__name__ + "." + name)
    else:
        module = _external()[name].load()
    if not callable(getattr(module, "generate", None)):
        raise ValueError("DDS backend plugin does not implement generate()")
    return module
