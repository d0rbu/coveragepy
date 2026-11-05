# Licensed under the Apache License: http://www.apache.org/licenses/LICENSE-2.0
# For details: https://github.com/coveragepy/coveragepy/blob/main/NOTICE.txt

"""Tests of coverage/collector.py and other collectors."""

from __future__ import annotations

import os.path

import coverage

from tests.coveragetest import CoverageTest
from tests.helpers import CheckUniqueFilenames


class CollectorTest(CoverageTest):
    """Test specific aspects of the collection process."""

    def test_excluded_generator_calls_measured_functions(self) -> None:
        self.make_file(
            "measured.py",
            """\
            def callback(value):
                if value:
                    return value * 2
                return -1
            """,
        )
        self.make_file(
            "excluded.py",
            """\
            from measured import callback

            def stream():
                for value in [0, 1, 2]:
                    yield callback(value)

            assert list(stream()) == [-1, 2, 4]
            """,
        )
        cov = coverage.Coverage(include=["measured.py"], branch=True)
        self.start_import_stop(cov, "excluded")
        data = cov.get_data()
        assert data.measured_files() == {os.path.abspath("measured.py")}
        arcs = data.arcs(os.path.abspath("measured.py"))
        assert arcs is not None
        assert sorted(arcs) == [
            (-1, 1), (-1, 2), (1, -1), (2, 3), (2, 4), (3, -1), (4, -1),
        ]

    def test_should_trace_cache(self) -> None:
        # The tracers should only invoke should_trace once for each file name.

        # Make some files that invoke each other.
        self.make_file(
            "f1.py",
            """\
            def f1(x, f):
                return f(x)
            """,
        )

        self.make_file(
            "f2.py",
            """\
            import f1

            def func(x):
                return f1.f1(x, otherfunc)

            def otherfunc(x):
                return x*x

            for i in range(10):
                func(i)
            """,
        )

        # Trace one file, but not the other. CheckUniqueFilenames will assert
        # that _should_trace hasn't been called twice for the same file.
        cov = coverage.Coverage(include=["f1.py"])
        should_trace_hook = CheckUniqueFilenames.hook(cov, "_should_trace")

        # Import the Python file, executing it.
        self.start_import_stop(cov, "f2")

        # Double-check that our files were checked.
        abs_files = {os.path.abspath(f) for f in should_trace_hook.filenames}
        assert os.path.abspath("f1.py") in abs_files
        assert os.path.abspath("f2.py") in abs_files
