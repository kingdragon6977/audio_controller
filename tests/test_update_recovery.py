import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('update', Path(__file__).resolve().parents[1] / 'tools/stage_stm32_update.py')
update = importlib.util.module_from_spec(spec)
spec.loader.exec_module(update)

class RecoveryTests(unittest.TestCase):
    def test_pending_then_ready(self):
        with patch.object(update, 'transact', side_effect=[(6,26,26,0),(0,26,26,0)]) as tx, patch.object(update.time, 'sleep'):
            update.wait_application(None, ('127.0.0.1',5004), 123)
        self.assertEqual(tx.call_count, 2)
        self.assertEqual(tx.call_args.args[2:4], (update.PKT_APPLICATION_STATUS,123))

    def test_wrong_session_fails(self):
        with patch.object(update, 'transact', return_value=(3,0,0,0)):
            with self.assertRaisesRegex(RuntimeError, 'BAD_SESSION'):
                update.wait_application(None, ('127.0.0.1',5004), 123)

    def test_recovery_timeout_does_not_report_flash_success(self):
        with patch.object(update.time, 'monotonic', side_effect=[0, 61]):
            with self.assertRaisesRegex(RuntimeError, 'image verified.*not confirmed'):
                update.wait_application(None, ('127.0.0.1',5004), 123)

    def test_lost_status_reply_retries(self):
        with patch.object(update, 'transact', side_effect=[RuntimeError('no ACK'),(0,26,26,0)]):
            update.wait_application(None, ('127.0.0.1',5004), 123)

if __name__ == '__main__':
    unittest.main()
