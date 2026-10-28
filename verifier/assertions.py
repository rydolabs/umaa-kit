"""Independent evidence-level success assertions for a *development* test run.

Legacy supervisor return code alone is insufficient evidence of a PASS.
These markers only identify the narrowly scoped simulator result, never
establish UMAA normative compliance or authenticate the runner.
"""
import json
from pathlib import Path


DDS_MARKERS = {
    'dds-engine': ('SIM: PASS',),
    'dds-anchor': ('ANCHOR_HOST: PASS', 'ANCHOR_SIM: PASS'),
    'dds-anchor-extended': ('ANCHOR_CONTROL_HOST_PASS',
                            'ANCHOR_CONTROL_ORACLE: PASS official UMAA AnchorControl'),
    'dds-anchor-specs': ('ANCHOR_SPECS_HOST_PASS meaningful_fields=2 dispose=1',
                         'ANCHOR_SPECS_ORACLE_PASS validated_anchor_size_holding_power_rode_length_description update=1 dispose=1'),
    'dds-reviewed-discovery': ('REVIEWED_SERVICE_DISCOVERY_HOST_PASS',
                               'REVIEWED_SERVICE_DISCOVERY_SIM_PASS'),
}


def success_errors(profile_id: str, option: str | None, stdout: Path,
                   report: Path | None = None, scenario: Path | None = None) -> list[str]:
    """Re-evaluate successful evidence from files, without loading SDK code."""
    try:
        if profile_id == 'stdio-fixture':
            if report is None or scenario is None:
                return ['stdio PASS missing report or scenario snapshot']
            d = json.loads(report.read_text(encoding='utf-8'))
            s = json.loads(scenario.read_text(encoding='utf-8'))
            if not isinstance(d, dict) or not isinstance(s, dict):
                return ['stdio report or scenario is not an object']
            if (d.get('result') != 'PASS' or d.get('scenario') != s.get('name')
                    or d.get('transport') != 'stdio fixture only (NOT DDS)'
                    or d.get('trace') != s.get('expected_events')
                    or not isinstance(s.get('expected_events'), list)
                    or d.get('events') != len(s['expected_events'])):
                return ['stdio oracle report does not match scenario snapshot']
            return []
        if profile_id not in DDS_MARKERS:
            return ['unknown DDS verification profile']
        required = list(DDS_MARKERS[profile_id])
        if option == '--bad-destination':
            required = {
                'dds-engine': ['ENGINE_SIM: PASS invalid destination with no Status/Ack',
                               'command destination did not match configured provider'],
                'dds-anchor': ['ANCHOR_SIM: PASS invalid destination with no Status/Ack',
                               'command destination did not match configured provider'],
                'dds-anchor-extended': ['ANCHOR_CONTROL_ORACLE: PASS invalid destination with no Status/Ack',
                                        'command destination did not match configured provider'],
            }.get(profile_id, ['unsupported destination scenario'])
        if profile_id == 'dds-reviewed-discovery' and option in ('--wire', '--semantic'):
            required += ['REVIEWED_SERVICE_TYPED_WIRE_HOST_PASS', 'REVIEWED_SERVICE_TYPED_WIRE_SIM_PASS']
        if profile_id == 'dds-reviewed-discovery' and option == '--semantic':
            required += ['ANCHOR_SPECS_SEMANTIC_HOST_PASS', 'ANCHOR_SPECS_SEMANTIC_SIM_PASS']
        # Avoid reading unbounded log content from a compromised runner.
        with stdout.open('rb') as stream:
            data = stream.read(8 * 1024 * 1024 + 1)
        if len(data) > 8 * 1024 * 1024:
            return ['stdout log exceeds evidence inspection limit']
        output = data.decode('utf-8', errors='replace')
        return ['missing DDS supervisor/oracle marker: ' + marker
                for marker in required if marker not in output]
    except (OSError, ValueError, TypeError, KeyError) as exc:
        return ['cannot check independent success evidence: ' + str(exc)]
