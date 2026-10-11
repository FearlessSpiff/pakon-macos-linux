"""Work folder and order counter (web/orders.py), in a temporary directory."""

from datetime import datetime
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("orders", ROOT / "web/orders.py")
orders = importlib.util.module_from_spec(spec)
spec.loader.exec_module(orders)


class OrdersTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        tmp = Path(self.tmp.name)
        orders.SETTINGS = tmp / "data" / "settings.json"
        self.work = tmp / "work"
        self.work.mkdir()
        orders.set_work_dir(self.work)

    def tearDown(self):
        self.tmp.cleanup()

    def test_orders_count_up_and_persist(self):
        when = datetime(2026, 10, 11, 14, 30)
        self.assertEqual(orders.next_order(), "000001")
        order, raw = orders.new_order(when)
        self.assertEqual(order, "000001")
        self.assertEqual(raw, self.work / "000001" / "2026-10-11-14-30-scan.raw")
        self.assertTrue(raw.parent.is_dir())
        self.assertEqual(orders.new_order(when)[0], "000002")
        self.assertEqual(orders.next_order(), "000003")

    def test_counter_skips_existing_order_folders(self):
        (self.work / "000041").mkdir()
        self.assertEqual(orders.new_order()[0], "000042")

    def test_work_dir_defaults_to_home_pakon(self):
        orders.SETTINGS.unlink()
        self.assertEqual(orders.work_dir(), Path.home() / "pakon")


if __name__ == "__main__":
    unittest.main()
