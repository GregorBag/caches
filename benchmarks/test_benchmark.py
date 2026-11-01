import contextlib
import io
import unittest
from unittest.mock import patch

from benchmark import arguments, budgets_for


class BudgetTests(unittest.TestCase):
    def test_default_range_reaches_full_universe(self):
        budgets = budgets_for(1024, 0.05)
        self.assertEqual(len(budgets), 20)
        self.assertEqual((budgets[0], budgets[-1]), (51, 1024))

    def test_limit_on_step_boundary(self):
        self.assertEqual(budgets_for(100, 0.05, 0.3), [5, 10, 15, 20, 25, 30])

    def test_limit_between_steps_is_included(self):
        self.assertEqual(budgets_for(100, 0.2, 0.3), [20, 30])

    def test_step_above_limit_gives_one_point(self):
        self.assertEqual(budgets_for(100, 0.5, 0.3), [30])

    def test_rounding_cannot_exceed_limit(self):
        self.assertEqual(budgets_for(32, 0.1, 0.3), [3, 6, 9])

    def test_duplicate_budgets_are_merged(self):
        self.assertEqual(budgets_for(10, 0.05, 0.3), [1, 2, 3])

    def test_invalid_limits_are_rejected(self):
        for value in ['0', '-0.1', '1.1', 'nan', 'inf', '-inf']:
            with self.subTest(value=value), patch('sys.argv', ['benchmark.py', f'--max-ratio={value}']):
                with contextlib.redirect_stderr(io.StringIO()) as error, self.assertRaises(SystemExit) as exit:
                    arguments()
                self.assertEqual(exit.exception.code, 2)
                self.assertIn('--max-ratio must be greater than 0 and at most 1', error.getvalue())


if __name__ == '__main__':
    unittest.main()
