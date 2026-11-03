"""Trusted, locally installed DDS compiler plugins.

Each module adds a backend without modifying the orchestrator, contract IR,
component CLI, or CMake integration. Third-party code is never imported from
an untrusted IDL input path or a generated build directory.
"""
from __future__ import annotations

import importlib
import pkgutil
import re

BACKEND_ID = re.compile(r"^[a-z][a-z0-9_]{0,39}$")


def discover() -> tuple[str, ...]:
    return tuple(sorted(m.name for m in pkgutil.iter_modules(__path__)
                        if not m.name.startswith("_")))


def load(name: str):
    if not isinstance(name, str) or not BACKEND_ID.fullmatch(name):
        raise ValueError("unsafe DDS backend plugin name")
    if name not in discover():
        raise ValueError("unregistered DDS backend plugin: " + name)
    module = importlib.import_module(__name__ + "." + name)
    if not callable(getattr(module, "generate", None)):
        raise ValueError("DDS backend plugin does not implement generate()")
    return module
