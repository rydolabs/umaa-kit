#!/usr/bin/env python3
"""Probe official public UMAA PDFs on CI; output only reproducible metadata.

This is a source-acquisition audit, NOT an extractor of validated requirements.
No original PDFs or raw requirement paragraphs are written to the repository
or uploaded as artifacts.
"""
import argparse
import hashlib
import json
import re
import subprocess
import tempfile
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

DOCUMENTS = {
    "COMPLIANCE_REV1": [
        "https://www.auvsi.org/wp-content/uploads/2025/03/T0300-BE-IDS-010-Rev.-1.pdf",
        "https://www.auvsi.org/sites/default/files/T0300-BE-IDS-010-Rev.-1.pdf",
    ],
    "COMPONENT_DEFINITIONS_1_0": [
        "https://www.auvsi.org/wp-content/uploads/2025/03/Distro_A_UMAA_Component_Definitions_v1.0.pdf",
        "https://www.auvsi.org/sites/default/files/Distro_A_UMAA_Component_Definitions_v1.0.pdf",
    ],
    "ICD_CO_6_0": [
        "https://www.auvsi.org/wp-content/uploads/2025/03/UMAA-SPEC-CO-EXP-ICD.pdf",
    ],
}


def download(url, target):
    request = Request(url, headers={"User-Agent": "Mozilla/5.0 UMAAKit-Public-Spec-Provenance-Audit/1.0",
                                    "Accept": "application/pdf,*/*"})
    with urlopen(request, timeout=22) as response:
        data = response.read(25_000_001)
    if len(data) > 25_000_000 or not data.startswith(b"%PDF-"):
        raise ValueError("source unavailable, oversized or not PDF")
    target.write_bytes(data)
    return data


def inspect(doc_id, work):
    out = {"source_id": doc_id, "acquisition": "unavailable", "sha256": None,
           "pdf_pages": None, "candidate_shall_lines": None,
           "numbered_heading_count": None, "note": "NOT normative requirement extraction"}
    for url in DOCUMENTS[doc_id]:
        try:
            path = work / (doc_id + ".pdf")
            data = download(url, path)
            extracted = subprocess.run(["pdftotext", "-layout", str(path), "-"],
                                       capture_output=True, text=True, timeout=50, check=True).stdout
            info = subprocess.run(["pdfinfo", str(path)], capture_output=True,
                                  text=True, timeout=20, check=True).stdout
            pages = re.search(r"^Pages:\s+(\d+)", info, re.MULTILINE)
            lines = extracted.splitlines()
            # Candidate counts are NOT requirement counts: cross-page wrapping,
            # tables and notes must be independently reviewed in original PDF.
            out.update({"acquisition": "downloaded_metadata_only",
                        "source_url": url, "sha256": hashlib.sha256(data).hexdigest(),
                        "bytes": len(data), "pdf_pages": int(pages.group(1)) if pages else None,
                        "text_lines": len(lines),
                        "candidate_shall_lines": sum(bool(re.search(r"\bshall\b", x, re.I)) for x in lines),
                        "numbered_heading_count": sum(bool(re.match(r"^\s*[1-9]\d*(?:\.\d+){1,4}\s+[A-Za-z]", x)) for x in lines)})
            path.unlink(missing_ok=True)
            return out
        except (HTTPError, URLError, OSError, ValueError, subprocess.SubprocessError) as exc:
            out["last_error"] = str(exc)[:160]
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, default=Path("build/compliance/source-provenance.json"))
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="umaa-authority-") as temp:
        records = [inspect(doc_id, Path(temp)) for doc_id in DOCUMENTS]
    result = {"schema_version": "1.0.0", "audit_kind": "official_public_pdf_source_metadata_only",
              "full_requirement_trace_created": False,
              "sources": records,
              "retrieved_count": sum(r["acquisition"] == "downloaded_metadata_only" for r in records)}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, sort_keys=True, indent=2))
    print("UMAA_PUBLIC_PDF_ACQUISITION_AUDIT_NOT_COMPLIANCE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
