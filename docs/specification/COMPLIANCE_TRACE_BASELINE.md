# UMAA 6.0 normative trace baseline — NOT a compliance certificate

Reviewed against the public **T0300-BE-IDS-010 Rev 1** chapter 1 text
([PDF](https://auvsilink.org/PDFs/2022%20revisions_v10_T0300-BE-IDS-010_08.04.20.pdf)).
Each tracked obligation is paraphrased (not a substitute for the authoritative text)
in [the machine-readable matrix](../../catalog/umaa6-compliance-trace.json).
The scope and applicability of every row remain **UNRESOLVED** pending Component
selection and access to a fully reviewed Component Definitions v1.0 PDF.
A scheme-level SDK interface inventory is **not** the same as Component conformance.

## Verified source-level progress

- Chapter 1 clauses 1-1(1–6), 1-2(a–f), 1-3, 1-3.1 and 1-3.2(a–c)
  have been summarized as separate requirements, plus Appendix A-4.2's
  conditional DDS Security reference. This is the **compliance document
  baseline**, not an exhaustive ICD/Component acceptance corpus.
- The current six-domain reviewed Catalog represents 53 literal service sections.
  Its per-service provider/consumer/collection arrays have **114 entries in
  total**, including the extra collection structure; the historical **113
  explicit ICD Topic** count is a different measure. Neither count is the
  normative Component-scope service total.
- Exact Topic strings, DDS instance keys, roles and QoS are not yet
  certified to the official Component Definitions and ICD text. The Catalog
  has `complete=false` and `dds_qos_verified=false`.
- Linux/Windows official IDL builds and the Battery/Engine/Anchor semantic DDS
  tests are valuable **partial** runtime evidence, not clause-level
  closure. See [Delivery Gates](../DELIVERY_GATES.md).

## Scope blockers

1. Confirm source-revision authority against the acquisition contract, and
   complete a controlled review of the Component Definitions v1.0 PDF. The
   document URL is known but retrieval was incomplete during this checkpoint.
2. Select a **specific target Component** (e.g. Autopilot) and capture its
   official Provider and Consumer tables. Do not infer membership from
   the 53 six-ICD service headings.
3. Ingest applicable CO 6.0 ICD and all applicable 6.0 service-level
   requirements. EXP is not mandatory for general compliance, unless the
   acquisition defines additional requirements.
4. Assign each Compliance Specification clause to the target Component as
   APPLICABLE/NOT_APPLICABLE with evidence and a rationale; trace required
   ICD messages/fields/QoS to independently reproducible test cases.
5. Add signed/reviewed evidence for every applicable requirement and reject
   PASS if a stable test/log and source revision are missing.

## Fail-closed automation

```sh
python tools/verify_compliance_trace.py --root .
```

This validates schema integrity, unique requirement IDs, minimum coverage,
reviewed Catalog baseline and blocks unsupported PASS/N/A claims. It returns
success for an honest **open** trace ledger; it does **not** certify UMAA.
Do not change `scope.claim` to PASS until independently adjudicated against
all applicable Component/ICD requirements and program-specific approval.
