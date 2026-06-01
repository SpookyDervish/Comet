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


class CometTest(unittest.TestCase):
    pass

def make_test(case: str):
    def test(self):
        obj_name = f"{TESTS_OBJ_FOLDER_NAME}/{case}.out"
        program_name = f"{TESTS_OBJ_FOLDER_NAME}/{case.rstrip(".comet")}"
        
        result = subprocess.run(["./cometc", f"{TESTS_FOLDER_NAME}/{case}", "-O0", "-o", obj_name])
        self.assertEqual(result.returncode, 0, f"{case} did not compile successfully (cometc)!")
        
        gcc_result = subprocess.run(["./comet", obj_name])
        self.assertEqual(gcc_result.returncode, 0, f"{case} did not run successfully (comet)!")
        
        program_result = subprocess.run([program_name], stdout=subprocess.DEVNULL)
        self.assertEqual(program_result.returncode, 0, f"{case} did not run successfully!")
    return test

for i, (test_name) in enumerate(os.listdir("tests")):
    setattr(
        CometTest,
        f"test_{test_name}",
        make_test(test_name)
    )       

if __name__ == "__main__":
    unittest.main()