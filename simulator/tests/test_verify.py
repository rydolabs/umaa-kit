import pathlib
import sys
import unittest
sys.path.insert(0, str(pathlib.Path(__file__).parents[1] / "src"))
from verify import Oracle, VerificationError

class OracleTests(unittest.TestCase):
    def test_valid_trace(self):
        o = Oracle()
        o.feed(["STATUS x ISSUED SUCCEEDED", "ACK x", "STATUS x COMMANDED SUCCEEDED", "STATUS x EXECUTING SUCCEEDED", "STATUS x COMPLETED SUCCEEDED", "DISPOSE_STATUS x", "DISPOSE_ACK x"])
        o.assert_complete()
    def test_reject_skipped_state(self):
        with self.assertRaises(VerificationError):
            Oracle().feed(["STATUS x ISSUED SUCCEEDED", "ACK x", "STATUS x EXECUTING SUCCEEDED"])
    def test_reject_early_dispose(self):
        with self.assertRaises(VerificationError):
            Oracle().feed(["STATUS x ISSUED SUCCEEDED", "DISPOSE_STATUS x"])
    def test_reject_missing_ack(self):
        with self.assertRaises(VerificationError):
            Oracle().feed(["STATUS x ISSUED SUCCEEDED", "STATUS x COMMANDED SUCCEEDED"])
    def test_reject_failure_reason_for_wrong_state(self):
        with self.assertRaises(VerificationError):
            Oracle().feed(["STATUS x ISSUED SUCCEEDED", "STATUS x FAILED OBJECTIVE_FAILED"])
    def test_reject_terminal_update(self):
        with self.assertRaises(VerificationError):
            Oracle().feed(["STATUS x ISSUED SUCCEEDED", "ACK x", "STATUS x COMMANDED SUCCEEDED", "STATUS x EXECUTING SUCCEEDED", "STATUS x COMPLETED SUCCEEDED", "STATUS x ISSUED UPDATED"])

if __name__ == "__main__": unittest.main()
