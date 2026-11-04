#!/usr/bin/env python3
"""Verify AUVSI UMAA Component Definition §3 service role rows (no conformance).

Only the explicit public v1.0 Component Definitions tilde legend and exact
section 3.x.3 service rows are authenticated. An acquisition Component
Specification, normative QoS, service applicability and Runtime approval are
NOT inferred from this source.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

from tools.verify_compliance_scope import (
    CATALOG, SCOPE, COMPONENT_DEFINITION_SHA256, load_json, validate,
)

TITLE_RE = re.compile(r'(?m)^\s*3\.([1-9])\.3\s+Component Services\s*$')
NEXT_COMPONENT_RE = re.compile(r'(?m)^\s*3\.[1-9]\s+[A-Z]')
SERVICE_RE = re.compile(r'^(~?[A-Z][A-Za-z0-9]+)$')


class OriginalRoleError(ValueError):
    pass


def extract_service_roles(pdf_text: str) -> list[list[tuple[str, str]]]:
    """Extract the nine original section 3.x.3 lists without inferring names."""
    headings = list(TITLE_RE.finditer(pdf_text))
    if len(headings) != 9 or [m.group(1) for m in headings] != list('123456789'):
        raise OriginalRoleError('official Section 3.x.3 heading inventory changed')
    groups = []
    for i, match in enumerate(headings):
        tail = pdf_text[match.end():]
        next_component = NEXT_COMPONENT_RE.search(tail)
        block = tail[:next_component.start()] if next_component else tail
        found = []
        for line in block.splitlines():
            value = line.strip().lstrip('•●').strip()
            if SERVICE_RE.fullmatch(value):
                found.append((value.lstrip('~'),
                              'consumer' if value.startswith('~') else 'provider'))
        groups.append(found)
    return groups


def verify_original(pdf_path: Path, scope_path: Path = SCOPE) -> dict:
    document = pdf_path.read_bytes()
    digest = hashlib.sha256(document).hexdigest()
    if digest != COMPONENT_DEFINITION_SHA256:
        raise OriginalRoleError('original Component Definitions SHA-256 mismatch')
    result = subprocess.run(
        ['pdftotext', '-layout', '-enc', 'UTF-8', str(pdf_path), '-'],
        check=True, capture_output=True, text=True, encoding='utf-8',
    )
    text = result.stdout
    # Source language explicitly defines symbol direction, not applicability.
    legend = re.sub(r'\s+', ' ', text)
    if not re.search(r'services annotated with.{0,110}[~].{0,160}consumer'
                     r'.{0,130}Otherwise.{0,120}provider', legend, re.I):
        raise OriginalRoleError('authoritative Consumer/Provider tilde legend missing')
    actual = extract_service_roles(text)
    scope = load_json(scope_path)
    status = validate(load_json(CATALOG), scope)
    if status['errors']:
        raise OriginalRoleError('scope integrity failure: ' + '; '.join(status['errors']))
    for index, component in enumerate(scope['component_candidates']):
        expected = [(row['service_name'], row['provider_consumer_role'])
                    for row in component['service_assignment_candidates']]
        if actual[index] != expected:
            raise OriginalRoleError(
                'source service/role row mismatch: ' + component['name'] +
                ' expected=' + repr(expected) + ' observed=' + repr(actual[index]))
    return {
        'kind': 'OFFICIAL_COMPONENT_ROLE_LEGEND_AND_ROWS_ONLY',
        'sourceSha256': digest,
        'components': len(actual),
        'providerRows': sum(role == 'provider' for group in actual
                            for _, role in group),
        'consumerRows': sum(role == 'consumer' for group in actual
                            for _, role in group),
        'membershipApproved': False,
        'componentSpecificationApproved': False,
        'qosProfileApproved': False,
        'officialRuntimeEnabled': False,
        'formalConformance': False,
    }


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pdf', required=True, type=Path)
    parser.add_argument('--scope', default=SCOPE, type=Path)
    opts = parser.parse_args(argv)
    try:
        print(json.dumps(verify_original(opts.pdf, opts.scope), sort_keys=True))
        return 0
    except (OriginalRoleError, OSError, ValueError,
            subprocess.CalledProcessError) as exc:
        print('UMAA_COMPONENT_ROLE_SOURCE_FAIL: ' + str(exc), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
