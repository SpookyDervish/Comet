"""
# run_tests.py

A helper script to test all of comet's features. This script reads the "tests" folder
and executes each comet file, making sure it doesn't fail to compile.

**Note:** This script requires that the cometc executable is in the same folder as the script, same with the tests folder.
"""
import unittest
import os
import subprocess


TESTS_FOLDER_NAME = "tests"
TESTS_OBJ_FOLDER_NAME = "test_objs"


class CometTester(unittest.TestCase):
    def test_comet(self):
        cases = os.listdir(TESTS_FOLDER_NAME)
        
        self.assertTrue(os.path.isfile("./cometc"), "The comet compiler was not found in the same path as run_tests.py! Is there a \"cometc\" executable in the same folder?")
        
        for case in cases:
            with self.subTest():
                result = subprocess.run(["./cometc", f"{TESTS_FOLDER_NAME}/{case}", "-o", f"{TESTS_OBJ_FOLDER_NAME}/{case}.obj"])
                self.assertEqual(result.returncode, 0, f"{case} did not compile successfully!")
        

if __name__ == "__main__":
    unittest.main()