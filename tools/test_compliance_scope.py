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
        self.assertTrue(r['component_role_symbol_legend_reviewed'])
        self.assertEqual((r['component_provider_role_rows'], r['component_consumer_role_rows']), (51, 40))

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

    def test_reviewed_symbol_legend_yields_exact_provider_consumer_roles(self):
        def by_name(name):
            return next(c for c in self.scope['component_candidates']
                        if c['name'] == name)['service_assignment_candidates']
        self.assertEqual(
            [(r['service_name'], r['provider_consumer_role'])
             for r in by_name('Logging')],
            [('LogReport', 'consumer')])
        autopilot = by_name('Autopilot')
        self.assertEqual(
            next(r for r in autopilot if r['service_name']=='GlobalPoseStatus')['provider_consumer_role'],
            'consumer')
        usv = by_name('USV Navigation')
        self.assertEqual(
            next(r for r in usv if r['service_name']=='GlobalPoseStatus')['provider_consumer_role'],
            'provider')
        # A Component can carry a service in both roles; preserve rows.
        uuv = [r['provider_consumer_role'] for r in by_name('UUV Navigation')
               if r['service_name']=='GlobalPoseStatus']
        self.assertEqual(sorted(uuv), ['consumer', 'provider'])
        for comp in self.scope['component_candidates']:
            for row in comp['service_assignment_candidates']:
                self.assertEqual(row['provider_consumer_role'],
                                 'consumer' if row['tilde_annotation_present'] else 'provider')
                self.assertTrue(row['authoritative_role_reviewed'])
                self.assertEqual(row['applicability'],'undetermined')
                self.assertFalse(comp['assignment_reviewed'])
        self.assertFalse(next(s for s in self.scope['sources']
                              if s['id']=='COMPONENT_DEFINITIONS_1_0')['full_text_reviewed'])

    def test_source_role_forgery_is_rejected(self):
        mutations = []
        def mutate_role(d):
            d['component_candidates'][0]['service_assignment_candidates'][0]['provider_consumer_role']='consumer'
        def mutate_tilde(d):
            d['component_candidates'][0]['service_assignment_candidates'][0]['tilde_annotation_present']=True
        def mutate_source(d):
            d['component_definition_extraction']['source_sha256']='f'*64
        def mutate_legend(d):
            d['component_definition_extraction']['role_and_tilde_semantics_reviewed']=False
        for mutate, reason in ((mutate_role,'Component role/tilde evidence mismatch'),
                               (mutate_tilde,'Component role/tilde evidence mismatch'),
                               (mutate_source,'Component role source provenance altered'),
                               (mutate_legend,'Component role source provenance altered')):
            with self.subTest(mutation=mutate.__name__):
                d=copy.deepcopy(self.scope)
                mutate(d)
                self.rejected(d,reason)

    def test_role_resolution_does_not_approve_application_or_qos(self):
        d=copy.deepcopy(self.scope)
        d['component_candidates'][0]['service_assignment_candidates'][0]['applicability']='applicable'
        self.rejected(d,'prematurely approved Component applicability')
        d=copy.deepcopy(self.scope)
        d['component_candidates'][0]['assignment_reviewed']=True
        self.rejected(d,'prematurely approved Component membership')

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
