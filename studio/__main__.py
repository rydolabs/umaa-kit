"""Offline QEA adapter PoC: never writes to EA, does not require PostgreSQL."""
import argparse
import json
from pathlib import Path

from .qea import extract, model_diff, save_snapshot
from .documents import generate, verify_document
from .verification_links import link_verification, verify_link
from .idl import draft_profile, generate_idl, verify_idl, check_idlc


def main(argv=None):
    parser = argparse.ArgumentParser(description='Read-only selected EA QEA model importer (PoC)')
    sub = parser.add_subparsers(dest='action', required=True)
    imp = sub.add_parser('import-qea')
    imp.add_argument('--input', required=True, type=Path)
    imp.add_argument('--output', required=True, type=Path)
    diff = sub.add_parser('diff')
    diff.add_argument('--previous', required=True, type=Path)
    diff.add_argument('--current', required=True, type=Path)
    diff.add_argument('--output', required=True, type=Path)
    report = sub.add_parser('generate-docx', help='Generate a non-normative EA model inventory draft')
    report.add_argument('--snapshot', required=True, type=Path)
    report.add_argument('--output', required=True, type=Path)
    report.add_argument('--manifest', required=True, type=Path)
    report.add_argument('--template', type=Path)
    report.add_argument('--title', default='EA Model Inventory')
    check = sub.add_parser('verify-docx', help='Verify snapshot/DOCX/manifest consistency')
    check.add_argument('--snapshot', required=True, type=Path)
    check.add_argument('--output', required=True, type=Path)
    check.add_argument('--manifest', required=True, type=Path)
    check.add_argument('--template', type=Path)
    link = sub.add_parser('link-verification', help='Administratively link model snapshot and verifier export')
    link.add_argument('--snapshot', required=True, type=Path)
    link.add_argument('--verifier-export', required=True, type=Path)
    link.add_argument('--output', required=True, type=Path)
    link.add_argument('--project-id', required=True)
    check_link = sub.add_parser('verify-link', help='Check model/verifier export association digests')
    check_link.add_argument('--snapshot', required=True, type=Path)
    check_link.add_argument('--verifier-export', required=True, type=Path)
    check_link.add_argument('--output', required=True, type=Path)
    profile = sub.add_parser('draft-idl-profile', help='Pin selected project-extension struct GUIDs to one model')
    profile.add_argument('--snapshot', required=True, type=Path)
    profile.add_argument('--module', required=True)
    profile.add_argument('--element-guid', required=True, action='append')
    profile.add_argument('--output', required=True, type=Path)
    idl = sub.add_parser('generate-idl', help='Generate a NON-NORMATIVE project-extension IDL draft')
    idl.add_argument('--snapshot', required=True, type=Path)
    idl.add_argument('--profile', required=True, type=Path)
    idl.add_argument('--output-dir', required=True, type=Path)
    idl_check = sub.add_parser('verify-idl', help='Re-render and check draft IDL and hashes')
    idl_check.add_argument('--snapshot', required=True, type=Path)
    idl_check.add_argument('--profile', required=True, type=Path)
    idl_check.add_argument('--output-dir', required=True, type=Path)
    idlc = sub.add_parser('check-idlc', help='Optional local Cyclone idlc syntax check; not conformance')
    idlc.add_argument('--snapshot', required=True, type=Path)
    idlc.add_argument('--profile', required=True, type=Path)
    idlc.add_argument('--output-dir', required=True, type=Path)
    idlc.add_argument('--compiler', default='idlc')
    idlc.add_argument('--report', type=Path)
    args = parser.parse_args(argv)
    try:
        if args.action == 'import-qea':
            model = extract(args.input)
            save_snapshot(model, args.output)
            print(json.dumps({'counts': model['counts'], 'status': model['modelValidation'],
                              'generationApproved': False}, ensure_ascii=False))
        elif args.action == 'draft-idl-profile':
            result = draft_profile(args.snapshot, args.module, args.element_guid, args.output)
            print(json.dumps({'kind': result['kind'], 'selected': len(result['selectedElementGuids']),
                              'approved': False}, ensure_ascii=False))
        elif args.action == 'generate-idl':
            result = generate_idl(args.snapshot, args.profile, args.output_dir)
            print(json.dumps({'idlFile': result['idlFile'], 'idlSha256': result['idlSha256'],
                              'idlCompilerValidated': False, 'conformanceClaim': False}, ensure_ascii=False))
        elif args.action == 'check-idlc':
            result = check_idlc(args.snapshot, args.profile, args.output_dir,
                                compiler=args.compiler)
            if args.report:
                args.report.parent.mkdir(parents=True, exist_ok=True)
                with args.report.open('x', encoding='utf-8') as f:
                    json.dump(result, f, ensure_ascii=False, indent=2)
                    f.write('\n')
            print(json.dumps(result, ensure_ascii=False))
            if result['status'] != 'PASS':
                parser.exit(2, 'STUDIO IDLC GATE ' + result['status'] + ': ' + result['reason'] + '\n')
        elif args.action == 'verify-idl':
            errors = verify_idl(args.snapshot, args.profile, args.output_dir)
            if errors:
                parser.exit(2, 'STUDIO IDL EVIDENCE FAILED: ' + '; '.join(errors) + '\n')
            print('STUDIO IDL DRAFT VERIFIED (render/hash consistency; not DDS compilation)')
        elif args.action == 'generate-docx':
            result = generate(args.snapshot, args.output, args.manifest,
                              template=args.template, title=args.title)
            print(json.dumps({'docxSha256': result['docxSha256'],
                              'sourceQeaSha256': result['sourceQeaSha256AsDeclaredBySnapshot'],
                              'conformanceClaim': False}, ensure_ascii=False))
        elif args.action == 'link-verification':
            result = link_verification(args.snapshot, args.verifier_export,
                                       args.output, project_id=args.project_id)
            print(json.dumps({'projectId': result['projectId'],
                              'associationType': result['associationType'],
                              'conformanceClaim': False}, ensure_ascii=False))
        elif args.action == 'verify-link':
            errors = verify_link(args.snapshot, args.verifier_export, args.output)
            if errors:
                parser.exit(2, 'STUDIO LINK FAILED: ' + '; '.join(errors) + '\n')
            print('STUDIO ASSOCIATION DIGESTS MATCH (not proof of SUT/model identity)')
        elif args.action == 'verify-docx':
            errors = verify_document(args.snapshot, args.output, args.manifest,
                                     template=args.template)
            if errors:
                parser.exit(2, 'STUDIO DOCUMENT EVIDENCE FAILED: ' + '; '.join(errors) + '\n')
            print('STUDIO DOCUMENT EVIDENCE VERIFIED (consistency only, not authenticity)')
        else:
            old = json.loads(args.previous.read_text(encoding='utf-8'))
            new = json.loads(args.current.read_text(encoding='utf-8'))
            save_snapshot(model_diff(old, new), args.output)
            print('Model change summary written; generated artifacts require separate review')
    except (OSError, ValueError, KeyError, TypeError) as exc:
        parser.exit(2, 'STUDIO MODEL ERROR: ' + str(exc) + '\n')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
