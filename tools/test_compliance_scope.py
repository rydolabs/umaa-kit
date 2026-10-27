#!/usr/bin/env python3
"""Negative tests: false UMAA requirements/coverage claims must be rejected."""
import copy
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from verify_compliance_scope import CATALOG,SCOPE,load_json,validate

class ScopeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog=load_json(CATALOG)
        cls.scope=load_json(SCOPE)

    def rejected(self,data,fragment):
        self.assertTrue(any(fragment in s for s in validate(self.catalog,data)['errors']))

    def test_current_candidate_preflight(self):
        r=validate(self.catalog,self.scope)
        self.assertEqual(r['errors'],[])
        self.assertEqual((r['candidate_services'],r['candidate_topic_entries'],r['collection_element_references']), (53,113,1))
        self.assertEqual(r['normative_requirements_traced'],0)
        self.assertTrue(r['claim_blocked'])

    def test_missing_service(self):
        d=copy.deepcopy(self.scope);d['service_candidates'].pop()
        self.rejected(d,'candidate service inventory')

    def test_topic_contract_mutation(self):
        d=copy.deepcopy(self.scope);d['service_candidates'][0]['topics'][0]['type']='wrong'
        self.rejected(d,'Topic role/type')

    def test_discrepancy_cannot_disappear(self):
        d=copy.deepcopy(self.scope);d['known_discrepancies']=[]
        self.rejected(d,'missing classification evidence')

    def test_fabricated_pass(self):
        d=copy.deepcopy(self.scope);d['service_candidates'][0]['conformance_verdict']='pass'
        self.rejected(d,'unsupported conformance PASS')

    def test_unreviewed_na(self):
        d=copy.deepcopy(self.scope);d['service_candidates'][0]['applicability']='not_applicable'
        self.rejected(d,'unreviewed not_applicable')

    def test_component_definitions_not_normative(self):
        d=copy.deepcopy(self.scope)
        next(x for x in d['sources'] if x['id']=='COMPONENT_DEFINITIONS_1_0')['kind']='normative'
        self.rejected(d,'are not the requirements')

    def test_component_row_cannot_be_dropped(self):
        d=copy.deepcopy(self.scope)
        d['component_candidates'][0]['service_assignment_candidates'].pop()
        self.rejected(d,'component candidate row coverage drift')

    def test_unreviewed_component_role_cannot_be_invented(self):
        d=copy.deepcopy(self.scope)
        d['component_candidates'][0]['service_assignment_candidates'][0]['provider_consumer_role']='provider'
        self.rejected(d,'prematurely assigned Component role')

    def test_unreviewed_pdf_line_cannot_be_marked_pass(self):
        d=copy.deepcopy(self.scope)
        d['normative_clause_candidates'][0]['verdict']='pass'
        self.rejected(d,'unreviewed PDF line upgraded')

    def test_fingerprint_does_not_mean_normative_review(self):
        d=copy.deepcopy(self.scope)
        d['official_public_pdf_provenance'][0]['normative_reviewed']=True
        self.rejected(d,'prematurely reviewed source fingerprint')

    def test_unreviewed_requirements_cannot_pass(self):
        d=copy.deepcopy(self.scope)
        d['normative_requirements'].append({'requirement_id':'FAKE','applicability':'applicable','verdict':'pass','authoritative_document':'COMPLIANCE_REV1'})
        self.rejected(d,'unproven normative PASS')

if __name__=='__main__':
    unittest.main()
