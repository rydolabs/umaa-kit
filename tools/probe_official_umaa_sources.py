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
            # Non-authoritative draft locators only. Clause IDs cannot be
            # inferred from a text line; independent visual/source review
            # of the PDF is still necessary for requirement acceptance.
            if doc_id == "COMPLIANCE_REV1":
                out["shall_locator_candidates"] = [
                    {"extracted_line": i + 1,
                     "candidate_text": line.strip()[:260],
                     "next_line": lines[i + 1].strip()[:150] if i + 1 < len(lines) else ""}
                    for i, line in enumerate(lines)
                    if re.search(r"\bshall\b", line, re.I)
                ]
            if doc_id == "COMPONENT_DEFINITIONS_1_0":
                names = ("Autopilot", "COLREGS", "Logging", "Mission Executor",
                         "USV Navigation", "UUV Hazard Avoidance",
                         "UUV Navigation", "Water Environment", "Weather")
                out["component_section_context"] = [
                    {"line": i + 1,
                     "text": line.strip()[:180],
                     "next_line": lines[i + 1].strip()[:180] if i + 1 < len(lines) else ""}
                    for i, line in enumerate(lines)
                    if (any(n.lower() in line.lower() for n in names)
                        or re.search(r"\b(provides|consumes|provided|consumed)\b", line, re.I))
                ][:130]
            if doc_id == "COMPONENT_DEFINITIONS_1_0":
                section_headers = [
                    (idx, row.strip())
                    for idx, row in enumerate(lines)
                    if re.match(r"^\s*3\.[1-9]\.3\b", row)
                ]
                out["component_service_subsections"] = [
                    {"start_line": idx + 1,
                     "heading": heading,
                     "following_lines": [
                         {"line": i + 1, "text": lines[i].strip()[:160]}
                         for i in range(idx + 1, min(len(lines), idx + 42))
                         if lines[i].strip()
                     ]}
                    for idx, heading in section_headers
                ]
            if doc_id == "ICD_CO_6_0":
                out["section_locator_candidates"] = [
                    {"line": i + 1, "text": line.strip()[:140]}
                    for i, line in enumerate(lines)
                    if re.match(r"^\s*6\.(?:1|2)\.[0-9]+(?:\.[0-9]+)?\s+\S", line)
                ][:70]
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
