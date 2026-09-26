"""chunks.py: manifest parser, glob language, ownership rules, check-ownership, and agreement with cmake/AionChunks.cmake (fixture trees and the
real tree), plus the targets, unit tests and unity groups that aion_gs_finalize_chunks creates (a configured fixture project)."""
import contextlib
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import chunks  # noqa: E402

VS_CMAKE = Path('C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
CHECK_SCRIPT = chunks.GAME_SERVER / 'cmake' / 'CheckChunks.cmake'
CHUNKS_MODULE = chunks.GAME_SERVER / 'cmake' / 'AionChunks.cmake'
COMPILER_OPTIONS_MODULE = chunks.CPP_ROOT / 'cmake' / 'AionCompilerOptions.cmake'


def find_cmake():
    env = os.environ.get('AION_CMAKE')
    if env and Path(env).is_file():
        return env
    if VS_CMAKE.is_file():
        return str(VS_CMAKE)
    return shutil.which('cmake')


SETTINGS = '''aion_gs_chunks_settings(
	EXTENSIONS cpp h ipp inc
	ROOTS src handlers generated
	JAVA_ROOTS "src/**/*.java" "data/handlers/**/*.java"
)
'''

FIXTURE_MANIFEST = SETTINGS + '''
# a comment with (parentheses) and "quotes"
aion_gs_chunk(LEAF TARGET aion_gs_leaf PHASE 4 GLOBS "aion/gameserver/leaf/**" JAVA "src/com/aionemu/gameserver/leaf/**" DEPENDS aion_commons_core
	TESTS leaf_tests TEST_SUPPORT support)
aion_gs_chunk(MODEL TARGET aion_gs_model PHASE 4
	GLOBS "aion/gameserver/model/*" "aion/gameserver/model/{a,b}/**"   # direct files and two subpackages
	EXCLUDE "aion/gameserver/model/a/Skip.*"
	JAVA "src/com/aionemu/gameserver/model/*" "src/com/aionemu/gameserver/model/{a,b}/**"
	JAVA_EXCLUDE "src/com/aionemu/gameserver/model/a/Skip.java")
aion_gs_chunk(MODEL TARGET aion_gs_model_skip PHASE 4
	GLOBS "aion/gameserver/model/a/Skip.*"
	JAVA "src/com/aionemu/gameserver/model/a/Skip.java")
aion_gs_chunk(MODEL LEASE PHASE 4 TEST_SUPPORT support)
aion_gs_chunk(AK TARGET aion_gs_ak PHASE 5 GLOBS "aion/gameserver/packets/SM_[A-K]*" JAVA "src/com/aionemu/gameserver/packets/SM_[A-K]*.java")
aion_gs_chunk(LZ TARGET aion_gs_lz PHASE 5 GLOBS "aion/gameserver/packets/**" EXCLUDE "aion/gameserver/packets/SM_[A-K]*"
	JAVA "src/com/aionemu/gameserver/packets/**" JAVA_EXCLUDE "src/com/aionemu/gameserver/packets/SM_[A-K]*.java")
aion_gs_chunk(SHARED_A TARGET aion_gs_shared PHASE 5 GLOBS "aion/gameserver/shared/A*" JAVA "src/com/aionemu/gameserver/shared/A*.java")
aion_gs_chunk(SHARED_B TARGET aion_gs_shared PHASE 5 GLOBS "aion/gameserver/shared/**" EXCLUDE "aion/gameserver/shared/A*"
	JAVA "src/com/aionemu/gameserver/shared/**" JAVA_EXCLUDE "src/com/aionemu/gameserver/shared/A*.java")
aion_gs_chunk(SHELLS LEASE PHASE 4 XMLGEN_SHELLS GLOBS "aion/gameserver/model/**")
aion_gs_chunk(GEN TARGET aion_gs_gen PHASE 4 ROOT generated GLOBS "aion/gameserver/**")
aion_gs_chunk(APP TARGET aion_gs_app PHASE 5 GLOBS "aion/gameserver/*" MAIN "main.cpp" JAVA "src/com/aionemu/gameserver/*"
	OTHER_FILES "cmake/Run{Smoke,Check}.cmake")
aion_gs_chunk(H1 TARGET aion_gs_handlers_a PHASE 6 ROOT handlers GLOBS "aion/gameserver/handlers/quest/**/*.cpp" "aion/gameserver/handlers/quest/Prelude.h"
	JAVA "data/handlers/quest/**" PCH "aion/gameserver/handlers/quest/Prelude.h")
aion_gs_chunk(LZ LEASE PHASE 5 OTHER_FILES "cmake/RunCheck.cmake")
'''

FIXTURE_CPP = [
    'src/main.cpp', 'src/aion/gameserver/fwd.h',
    'src/aion/gameserver/leaf/Leaf.h', 'src/aion/gameserver/leaf/Leaf.cpp', 'src/aion/gameserver/leaf/deep/More.h',
    'src/aion/gameserver/model/Race.h', 'src/aion/gameserver/model/fwd.h',
    'src/aion/gameserver/model/a/Item.h', 'src/aion/gameserver/model/a/Item.cpp', 'src/aion/gameserver/model/a/Skip.h',
    'src/aion/gameserver/model/b/x/Deep.h', 'src/aion/gameserver/model/b/Data.h',
    'src/aion/gameserver/packets/SM_ABC.h', 'src/aion/gameserver/packets/SM_KILL.cpp', 'src/aion/gameserver/packets/SM_LOL.h',
    'src/aion/gameserver/packets/fwd.h', 'src/aion/gameserver/packets/AbstractPacket.h',
    'src/aion/gameserver/shared/Alpha.h', 'src/aion/gameserver/shared/Alpha.cpp', 'src/aion/gameserver/shared/Beta.cpp',
    'generated/aion/gameserver/model/a/Item.xml.inc', 'generated/aion/gameserver/model/a/Item.xml.h', 'generated/aion/gameserver/model/a/a.bind.cpp',
    'generated/aion/gameserver/model/b/Data.h',
    'handlers/aion/gameserver/handlers/quest/Prelude.h', 'handlers/aion/gameserver/handlers/quest/x/_1000Q.cpp',
]
FIXTURE_TESTS = [
    'tests/leaf_tests/LeafTest.cpp', 'tests/support/SupportHelpers.h', 'tests/model/RaceTest.cpp', 'tests/model_skip/SkipTest.cpp',
    'tests/shared/SHARED_A/AlphaTest.cpp', 'tests/shared/SHARED_B/BetaTest.cpp', 'tests/handlers_a/QuestTest.cpp',
]
NOT_SOURCES = ['generated/xmlmodel.json', 'src/aion/gameserver/model/notes.md', 'tests/leaf_tests/oracle/gen.py', 'cmake/RunSmoke.cmake']
FIXTURE_JAVA = [
    'src/com/aionemu/gameserver/GameServer.java', 'src/com/aionemu/gameserver/leaf/Leaf.java', 'src/com/aionemu/gameserver/model/Race.java',
    'src/com/aionemu/gameserver/model/a/Item.java', 'src/com/aionemu/gameserver/model/a/Skip.java', 'src/com/aionemu/gameserver/model/b/Data.java',
    'src/com/aionemu/gameserver/packets/SM_ABC.java', 'src/com/aionemu/gameserver/packets/SM_LOL.java',
    'src/com/aionemu/gameserver/shared/Alpha.java', 'src/com/aionemu/gameserver/shared/Beta.java',
    'data/handlers/quest/x/_1000Q.java',
]


def write_fixture(root, manifest=FIXTURE_MANIFEST, cpp=FIXTURE_CPP, java=FIXTURE_JAVA, tests=FIXTURE_TESTS):
    gs = Path(root) / 'cpp' / 'game-server'
    jd = Path(root) / 'game-server'
    for rel in list(cpp) + list(tests) + NOT_SOURCES:
        p = gs / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text('// fixture\n', encoding='utf-8')
    for rel in java:
        p = jd / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text('class X {}\n', encoding='utf-8')
    (gs / 'chunks.cmake').write_text(manifest, encoding='utf-8')
    return gs, jd


def run_cmake_check(gs, jd, json_path):
    cmake = find_cmake()
    out = subprocess.run([cmake, f'-DSOURCE_DIR={gs.as_posix()}', f'-DJAVA_DIR={jd.as_posix()}', f'-DMANIFEST={(gs / "chunks.cmake").as_posix()}',
                          f'-DJSON={Path(json_path).as_posix()}', '-P', str(CHECK_SCRIPT)], capture_output=True, text=True, check=False)
    return out.returncode, out.stdout + out.stderr


class ParserTest(unittest.TestCase):
    def test_commands_arguments_and_comments(self):
        cmds = chunks.parse_cmake_commands('a(x "y z" # c )\n  "q\\"r" w)\n# only a comment\nb ( )\n')
        self.assertEqual(cmds, [('a', ['x', 'y z', 'q"r', 'w'], 1), ('b', [], 4)])

    def test_errors(self):
        for text, message in (('a(x', 'unterminated'), ('a(${X})', 'variable references'), ('a(b(c))', 'nested parentheses'), ('a x', r'expected \(')):
            with self.subTest(text=text), self.assertRaisesRegex(chunks.ManifestError, message):
                chunks.parse_cmake_commands(text)

    def test_keywords(self):
        kw = chunks.parse_keywords(['LEASE', 'TARGET', 't', 'GLOBS', 'a', 'b'], chunks.CHUNK_KEYWORDS, 'x')
        self.assertEqual(kw, {'LEASE': True, 'TARGET': 't', 'GLOBS': ['a', 'b']})
        with self.assertRaisesRegex(chunks.ManifestError, 'one value'):
            chunks.parse_keywords(['TARGET', 'a', 'b'], chunks.CHUNK_KEYWORDS, 'x')
        with self.assertRaisesRegex(chunks.ManifestError, 'unexpected argument'):
            chunks.parse_keywords(['stray'], chunks.CHUNK_KEYWORDS, 'x')


class GlobTest(unittest.TestCase):
    def matches(self, glob, path):
        return chunks.anchored(['src'], [glob]).match('src/' + path) is not None

    def test_translation(self):
        # the same expectations as the CMake replacement sequence (aion_gs_glob_to_regex)
        self.assertEqual(chunks.glob_to_regex('aion/gameserver/model/*'), 'aion/gameserver/model/[^/]*')
        self.assertEqual(chunks.glob_to_regex('a/**/b.h'), 'a/(.*/)?b\\.h')
        self.assertEqual(chunks.glob_to_regex('a/**'), 'a/.*')
        self.assertEqual(chunks.glob_to_regex('x/{a,b}/**'), 'x/(a|b)/.*')
        self.assertEqual(chunks.glob_to_regex('{CM_[A-K],Abstract}*'), '(CM_[A-K]|Abstract)[^/]*')
        self.assertEqual(chunks.glob_to_regex('F?.h'), 'F[^/]\\.h')

    def test_matching(self):
        self.assertTrue(self.matches('a/**/b.h', 'a/b.h'))
        self.assertTrue(self.matches('a/**/b.h', 'a/x/y/b.h'))
        self.assertFalse(self.matches('a/**/b.h', 'a/xb.h'))
        self.assertTrue(self.matches('a/*', 'a/b.h'))
        self.assertFalse(self.matches('a/*', 'a/x/b.h'))
        self.assertFalse(self.matches('a/**', 'a'))
        self.assertTrue(self.matches('p/SM_[A-K]*', 'p/SM_KILL.cpp'))
        self.assertFalse(self.matches('p/SM_[A-K]*', 'p/SM_LOL.cpp'))
        self.assertFalse(self.matches('p/SM_[A-K]*', 'p/sm_abc.cpp'))  # case-sensitive like CMake
        self.assertTrue(self.matches('m/{a,b}/**', 'm/b/c.h'))
        self.assertFalse(self.matches('m/{a,b}/**', 'm/ab/c.h'))
        self.assertFalse(self.matches('X.h', 'aXh'))  # '.' is literal

    def test_invalid_globs(self):
        for glob in ('a,b', 'a{b', 'a{b{c}}', 'a[b', 'a b', 'a}b{', 'a+b', '$x'):
            with self.subTest(glob=glob), self.assertRaises(chunks.ManifestError):
                chunks.glob_to_regex(glob)


# manifest errors that chunks.py and the CMake implementation both reject: (manifest text after SETTINGS, message fragment)
MANIFEST_ERRORS = {
    'aion_gs_chunk(A PHASE 4 GLOBS "x")': 'TARGET is required',
    'aion_gs_chunk(A LEASE TARGET t PHASE 4 GLOBS "x")': 'a LEASE has only',
    'aion_gs_chunk(A LEASE PHASE 4 GLOBS "x" TEST_INCLUDES y)': 'a LEASE has only',
    'aion_gs_chunk(A TARGET t PHASE 4 GLOBS "x" OTHER_FILES "src/x.h")': "OTHER_FILES glob 'src/x.h'",
    'aion_gs_chunk(A TARGET t PHASE 4 GLOBS "x" OTHER_FILES "tests/x/y.cmake")': "OTHER_FILES glob 'tests/x/y.cmake'",
    'aion_gs_chunk(A TARGET t PHASE 4 GLOBS "x" OTHER_FILES "../CMakeLists.txt")': "OTHER_FILES glob '../CMakeLists.txt'",
    'aion_gs_chunk(A TARGET t PHASE 4 GLOBS "x" OTHER_FILES "cmake/a,b")': 'comma outside braces',
    'aion_gs_chunk(A LEASE PHASE 4 ROOT src)': 'GLOBS is required',
    'aion_gs_chunk(A TARGET t GLOBS "x")': 'PHASE is required',
    'aion_gs_chunk(A TARGET t PHASE 4)': 'GLOBS is required',
    'aion_gs_chunk(A TARGET t PHASE 4 TEST_SUPPORT s)': 'GLOBS is required',
    'aion_gs_chunk(A TARGET t PHASE 4 ROOT nowhere GLOBS "x")': 'unknown ROOT',
    'aion_gs_chunk(A TARGET t PHASE 4 GLOBS "a,b")': 'comma outside braces',
    'aion_gs_chunk(A TARGET t PHASE 4 BOGUS GLOBS "x")': 'unexpected argument',
    'aion_gs_chunk(A LEASE PHASE 4 ROOT handlers XMLGEN_SHELLS GLOBS "x")': 'XMLGEN_SHELLS needs ROOT src',
    'aion_gs_chunk(A TARGET t PHASE 4 GLOBS "x" GLOBS "y")': 'GLOBS given twice',
    'aion_gs_chunk(A TARGET t PHASE 4 GLOBS "x" TESTS ../up)': "test directory '../up'",
    'aion_gs_chunk(A TARGET t PHASE 4 GLOBS "x" DEPENDS d)\naion_gs_chunk(B TARGET t PHASE 4 GLOBS "y")':
        'target t: either all parts have DEPENDS (leaf) or none',
    'aion_gs_chunk(A TARGET t PHASE 4 GLOBS "x" TESTS common)\naion_gs_chunk(B TARGET u PHASE 4 GLOBS "y" TESTS common/sub)':
        'test directory tests/common of A (t) overlaps tests/common/sub of B (u)',
    'aion_gs_chunk(A TARGET t PHASE 4 GLOBS "x")\naion_gs_chunk(B TARGET t PHASE 4 GLOBS "y" TESTS t)':
        'test directory tests/t/A of A (t) overlaps tests/t of B (t)',
    'aion_gs_chunk(A TARGET t PHASE 4 GLOBS "x" TEST_SUPPORT s)\naion_gs_chunk(B TARGET u PHASE 4 GLOBS "y" TEST_SUPPORT s)':
        'test directory tests/s of A (t) overlaps tests/s of B (u)',
}


class ManifestTest(unittest.TestCase):
    def load(self, text):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / 'chunks.cmake'
            p.write_text(text, encoding='utf-8')
            return chunks.Manifest.load(p)

    def test_parts_and_defaults(self):
        m = self.load(FIXTURE_MANIFEST)
        self.assertEqual([p.name for p in m.parts],
                         ['LEAF', 'MODEL', 'MODEL', 'MODEL', 'AK', 'LZ', 'SHARED_A', 'SHARED_B', 'SHELLS', 'GEN', 'APP', 'H1', 'LZ'])
        self.assertEqual(m.parts[10].other_files, ['cmake/Run{Smoke,Check}.cmake'])
        self.assertTrue(m.parts[12].lease and m.parts[12].other_files == ['cmake/RunCheck.cmake'])
        self.assertEqual(m.other_file_parts('cmake/RunCheck.cmake'), ([m.parts[10]], [m.parts[12]]))
        self.assertEqual(m.other_file_parts('cmake/RunSmoke.cmake'), ([m.parts[10]], []))
        self.assertEqual(m.other_file_parts('cmake/Other.cmake'), ([], []))
        leaf, model = m.parts[0], m.parts[1]
        self.assertEqual((leaf.roots, leaf.depends, leaf.tests_dir, leaf.test_support), (['src'], ['aion_commons_core'], 'leaf_tests', ['support']))
        self.assertEqual((model.tests_dir, model.exclude), ('model', ['aion/gameserver/model/a/Skip.*']))
        self.assertEqual(m.parts[2].tests_dir, 'model_skip')
        self.assertTrue(m.parts[3].lease and m.parts[3].tests_dir == '' and m.parts[3].test_support == ['support'])
        # two chunks share aion_gs_shared: each gets its own directory below tests/shared
        self.assertEqual([m.parts[6].tests_dir, m.parts[7].tests_dir], ['shared/SHARED_A', 'shared/SHARED_B'])
        self.assertTrue(m.parts[8].lease and m.parts[8].xmlgen_shells)
        self.assertEqual(m.parts[11].pch, ['aion/gameserver/handlers/quest/Prelude.h'])
        self.assertEqual(len(m.chunk_parts('MODEL')), 3)

    def test_validation(self):
        for call, message in MANIFEST_ERRORS.items():
            with self.subTest(call=call), self.assertRaisesRegex(chunks.ManifestError, message.replace('(', r'\(').replace(')', r'\)')):
                self.load(SETTINGS + call + '\n')
        with self.assertRaisesRegex(chunks.ManifestError, 'aion_gs_chunks_settings\\(\\) is missing'):
            self.load('aion_gs_chunk(A TARGET t PHASE 4 GLOBS "x")\n')
        with self.assertRaisesRegex(chunks.ManifestError, "extension 'CPP'"):
            self.load(SETTINGS.replace('EXTENSIONS cpp', 'EXTENSIONS CPP') + 'aion_gs_chunk(A TARGET t PHASE 4 GLOBS "x")\n')
        # a lease of test directories needs no GLOBS
        m = self.load(SETTINGS + 'aion_gs_chunk(A TARGET t PHASE 4 GLOBS "x" TEST_SUPPORT s)\naion_gs_chunk(B LEASE PHASE 4 TEST_SUPPORT s)\n')
        self.assertEqual(m.test_dir_parts('tests/s/x.h')[0][0].name, 'A')
        self.assertEqual([p.name for p in m.test_dir_parts('tests/s/x.h')[1]], ['B'])

    @unittest.skipUnless(find_cmake() and CHECK_SCRIPT.is_file(), 'CMake not found')
    def test_cmake_rejects_the_same_manifests(self):
        with tempfile.TemporaryDirectory() as tmp:
            gs, jd = write_fixture(tmp)
            for call, message in MANIFEST_ERRORS.items():
                with self.subTest(call=call):
                    (gs / 'chunks.cmake').write_text(SETTINGS + call + '\n', encoding='utf-8')
                    status, output = run_cmake_check(gs, jd, Path(tmp) / 'x.json')
                    self.assertNotEqual(status, 0, output)
                    self.assertIn(message, ' '.join(output.split()))


class OwnershipTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix='chunks')
        self.gs, self.jd = write_fixture(self.tmp)

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def ownership(self, manifest=None):
        m = chunks.Manifest.load(self.gs / 'chunks.cmake') if manifest is None else manifest
        return m, chunks.Ownership.of_tree(m, self.gs, self.jd)

    def owner_names(self, own, path):
        return [p.describe() for p in own.owners.get(path, [])]

    def test_fixture_is_consistent(self):
        m, own = self.ownership()
        self.assertEqual(own.problems(), [])
        self.assertEqual(own.cpp_files, sorted(FIXTURE_CPP))  # notes.md and xmlmodel.json are not sources
        self.assertEqual(own.test_files, sorted(FIXTURE_TESTS))  # gen.py is not a source
        self.assertEqual(self.owner_names(own, 'src/main.cpp'), ['APP (aion_gs_app)'])
        self.assertEqual(self.owner_names(own, 'src/aion/gameserver/model/a/Skip.h'), ['MODEL (aion_gs_model_skip)'])
        self.assertEqual(self.owner_names(own, 'src/aion/gameserver/packets/SM_KILL.cpp'), ['AK (aion_gs_ak)'])
        self.assertEqual(self.owner_names(own, 'src/aion/gameserver/packets/fwd.h'), ['LZ (aion_gs_lz)'])
        # the lease covers only shells: Item.h/Item.cpp have generated/.../Item.xml.inc, Race.h and b/Data.h do not
        self.assertEqual(sorted(own.leases), ['src/aion/gameserver/model/a/Item.cpp', 'src/aion/gameserver/model/a/Item.h'])
        self.assertEqual(own.part_java[m.parts[1].index],
                         ['src/com/aionemu/gameserver/model/Race.java', 'src/com/aionemu/gameserver/model/a/Item.java',
                          'src/com/aionemu/gameserver/model/b/Data.java'])

    def test_unowned_and_multiple_owners(self):
        manifest = FIXTURE_MANIFEST.replace('EXCLUDE "aion/gameserver/model/a/Skip.*"\n', '').replace(
            'aion_gs_chunk(GEN TARGET aion_gs_gen PHASE 4 ROOT generated GLOBS "aion/gameserver/**")',
            'aion_gs_chunk(GEN TARGET aion_gs_gen PHASE 4 ROOT generated GLOBS "aion/gameserver/model/a/**")').replace(
            'JAVA_EXCLUDE "src/com/aionemu/gameserver/model/a/Skip.java"', '').replace('JAVA "data/handlers/quest/**"', '')
        (self.gs / 'chunks.cmake').write_text(manifest, encoding='utf-8')
        _, own = self.ownership()
        self.assertEqual(own.problems(), [
            'no chunk owns generated/aion/gameserver/model/b/Data.h',
            'several chunks own src/aion/gameserver/model/a/Skip.h: MODEL (aion_gs_model), MODEL (aion_gs_model_skip)',
            'no chunk claims Java file data/handlers/quest/x/_1000Q.java',
            'several chunks claim Java file src/com/aionemu/gameserver/model/a/Skip.java: MODEL (aion_gs_model), MODEL (aion_gs_model_skip)',
        ])

    def add_files(self, *paths):
        for rel in paths:
            p = self.gs / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text('// fixture\n', encoding='utf-8')

    # files that CMake's case-insensitive glob (Windows) finds and a case-sensitive filter must reject; other C++ extensions; test files outside
    # every test directory (tests/<chunk> is not one: nothing would build it)
    BAD_FILES = ('src/aion/gameserver/model/Upper.H', 'src/aion/gameserver/model/b.CPP', 'handlers/aion/gameserver/handlers/quest/x/Blob.hpp',
                 'src/aion/gameserver/leaf/Impl.cc', 'generated/aion/gameserver/model/a/x.inl', 'tests/leaf_tests/Case.Cpp',
                 'tests/SHELLS/NeverBuilt.cpp', 'tests/Loose.h')

    def test_extension_and_test_directory_problems(self):
        self.add_files(*self.BAD_FILES)
        _, own = self.ownership()
        allowed = '(allowed: .cpp .h .ipp .inc)'
        self.assertEqual(own.problems(), [
            f'unexpected C++ file extension: generated/aion/gameserver/model/a/x.inl {allowed}',
            f'unexpected C++ file extension: handlers/aion/gameserver/handlers/quest/x/Blob.hpp {allowed}',
            f'unexpected C++ file extension: src/aion/gameserver/leaf/Impl.cc {allowed}',
            f'unexpected C++ file extension: src/aion/gameserver/model/Upper.H {allowed}',
            f'unexpected C++ file extension: src/aion/gameserver/model/b.CPP {allowed}',
            f'unexpected C++ file extension: tests/leaf_tests/Case.Cpp {allowed}',
            'no chunk owns test file tests/Loose.h',
            'no chunk owns test file tests/SHELLS/NeverBuilt.cpp',
        ])

    @unittest.skipUnless(find_cmake() and CHECK_SCRIPT.is_file(), 'CMake not found')
    def test_cmake_reports_extension_and_test_directory_problems(self):
        self.add_files(*self.BAD_FILES)
        _, own = self.ownership()
        status, output = run_cmake_check(self.gs, self.jd, Path(self.tmp) / 'broken.json')
        self.assertNotEqual(status, 0)
        flat = ' '.join(output.split())
        for problem in own.problems():
            self.assertIn(problem, flat)

    def test_owner_command_and_paths(self):
        out = io.StringIO()
        args = ['--manifest', str(self.gs / 'chunks.cmake'), '--game-server', str(self.gs), '--java-dir', str(self.jd)]
        with contextlib.redirect_stdout(out):
            status = chunks.main(args + ['owner', str(self.gs / 'src/aion/gameserver/model/a/Item.h'), str(self.gs / 'src/aion/gameserver/New.h'),
                                         str(self.gs / 'tests/support/New.h'), str(self.gs / 'cmake/RunCheck.cmake')])
        self.assertEqual(status, 0)
        self.assertEqual(out.getvalue().splitlines(),
                         ['src/aion/gameserver/model/a/Item.h: MODEL (aion_gs_model); leased to SHELLS', 'src/aion/gameserver/New.h: APP (aion_gs_app)',
                          'tests/support/New.h: LEAF (aion_gs_leaf); leased to MODEL', 'cmake/RunCheck.cmake: APP (aion_gs_app); leased to LZ'])
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            status = chunks.main(args + ['owner', str(self.gs / 'cmake/Unowned.cmake')])
        self.assertEqual(status, 1)
        self.assertEqual(out.getvalue().splitlines(), ['cmake/Unowned.cmake: no owner'])

    def test_check_ownership_rules(self):
        m = chunks.Manifest.load(self.gs / 'chunks.cmake')
        gs = chunks.GAME_SERVER.relative_to(chunks.REPO_ROOT).as_posix()
        changed = [
            f'{gs}/src/aion/gameserver/model/Race.h',          # own file
            f'{gs}/src/aion/gameserver/model/a/Deleted.cpp',   # own glob, file does not exist (deleted in the range)
            f'{gs}/tests/model/RaceTest.cpp',                  # TESTS dir of the part (default: target without aion_gs_)
            f'{gs}/tests/model_skip/New/SkipTest.cpp',         # TESTS dir of the chunk's other part
            f'{gs}/tests/support/SupportHelpers.h',            # TEST_SUPPORT leased to the chunk
            'cpp/docs/deviations/MODEL.md',
            f'{gs}/tests/MODEL/Other.cpp',                     # tests/<chunk> is not built, so not allowed
            f'{gs}/tests/leaf_tests/LeafTest.cpp',             # another chunk's tests
            f'{gs}/src/aion/gameserver/leaf/Leaf.h',           # another chunk
            f'{gs}/chunks.cmake',                              # the manifest
            'cpp/tools/porting/chunks.py',
        ]
        violations = chunks.ownership_violations(m, 'MODEL', changed, self.gs)
        self.assertEqual(violations, [(f'{gs}/tests/MODEL/Other.cpp', 'owned by no chunk'),
                                      (f'{gs}/tests/leaf_tests/LeafTest.cpp', 'owned by LEAF (aion_gs_leaf)'),
                                      (f'{gs}/src/aion/gameserver/leaf/Leaf.h', 'owned by LEAF (aion_gs_leaf)'),
                                      (f'{gs}/chunks.cmake', 'outside the chunk'), ('cpp/tools/porting/chunks.py', 'outside the chunk')])
        # the lease: SHELLS may change the shells, not the other model files
        violations = chunks.ownership_violations(m, 'SHELLS', [f'{gs}/src/aion/gameserver/model/a/Item.h', f'{gs}/src/aion/gameserver/model/Race.h'],
                                                 self.gs)
        self.assertEqual(violations, [(f'{gs}/src/aion/gameserver/model/Race.h', 'owned by MODEL (aion_gs_model)')])
        # OTHER_FILES: the owner and a lease may change them, another chunk may not; a file outside every OTHER_FILES glob stays outside
        other = [f'{gs}/cmake/RunSmoke.cmake', f'{gs}/cmake/RunCheck.cmake', f'{gs}/cmake/AionChunks.cmake']
        self.assertEqual(chunks.ownership_violations(m, 'APP', other, self.gs), [(f'{gs}/cmake/AionChunks.cmake', 'outside the chunk')])
        self.assertEqual(chunks.ownership_violations(m, 'LZ', other, self.gs),
                         [(f'{gs}/cmake/RunSmoke.cmake', 'owned by APP (aion_gs_app)'), (f'{gs}/cmake/AionChunks.cmake', 'outside the chunk')])
        # chunks sharing a target have disjoint test directories
        violations = chunks.ownership_violations(m, 'SHARED_A', [f'{gs}/tests/shared/SHARED_A/AlphaTest.cpp', f'{gs}/tests/shared/SHARED_B/BetaTest.cpp'],
                                                 self.gs)
        self.assertEqual(violations, [(f'{gs}/tests/shared/SHARED_B/BetaTest.cpp', 'owned by SHARED_B (aion_gs_shared)')])

    @unittest.skipUnless(find_cmake() and CHECK_SCRIPT.is_file(), 'CMake not found')
    def test_cmake_agrees_on_the_fixture(self):
        m, own = self.ownership()
        json_path = Path(self.tmp) / 'chunks.json'
        status, output = run_cmake_check(self.gs, self.jd, json_path)
        self.assertEqual(status, 0, output)
        doc = json.loads(json_path.read_text(encoding='utf-8'))
        self.assertEqual(len(doc['parts']), len(m.parts))
        self.assertEqual(doc['testFileCount'], len(FIXTURE_TESTS))
        for entry, p in zip(doc['parts'], m.parts):
            self.assertEqual(entry['name'], p.name)
            self.assertEqual(sorted(entry['files']), sorted(own.part_files[p.index]), p.describe())
            self.assertEqual(sorted(entry['javaFiles']), sorted(own.part_java[p.index]), p.describe())
            self.assertEqual((entry['tests'], entry['testSupport']), (p.tests_dir, p.test_support), p.describe())
            self.assertEqual(entry['otherFiles'], p.other_files, p.describe())
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            result = chunks.main(['--manifest', str(self.gs / 'chunks.cmake'), '--game-server', str(self.gs), '--java-dir', str(self.jd),
                                  'verify-json', str(json_path)])
        self.assertEqual(result, 0, out.getvalue())

    @unittest.skipUnless(find_cmake() and CHECK_SCRIPT.is_file(), 'CMake not found')
    def test_cmake_reports_the_same_problems(self):
        manifest = FIXTURE_MANIFEST.replace('EXCLUDE "aion/gameserver/model/a/Skip.*"\n', '').replace(
            'aion_gs_chunk(GEN TARGET aion_gs_gen PHASE 4 ROOT generated GLOBS "aion/gameserver/**")',
            'aion_gs_chunk(GEN TARGET aion_gs_gen PHASE 4 ROOT generated GLOBS "aion/gameserver/model/a/**")').replace(
            'JAVA "data/handlers/quest/**"', '')
        (self.gs / 'chunks.cmake').write_text(manifest, encoding='utf-8')
        _, own = self.ownership()
        status, output = run_cmake_check(self.gs, self.jd, Path(self.tmp) / 'broken.json')
        self.assertNotEqual(status, 0)
        flat = ' '.join(output.split())
        # C++ and Java problems come in one message
        self.assertTrue(any(p.startswith('no chunk claims Java file') for p in own.problems()))
        for problem in own.problems():
            self.assertIn(problem, flat)


def run_unity_groups(tmp, java, sources, batch):
    script = Path(tmp) / 'unity_groups.cmake'
    out = Path(tmp) / 'groups.txt'
    script.write_text(f'include("{CHUNKS_MODULE.as_posix()}")\n'
                      f'aion_gs_handler_unity_groups(groups BATCH {batch} JAVA ${{JAVA}} SOURCES ${{SOURCES}})\n'
                      f'file(WRITE "{out.as_posix()}" "${{groups}}")\n', encoding='utf-8')
    result = subprocess.run([find_cmake(), f'-DJAVA={";".join(java)}', f'-DSOURCES={";".join(sources)}', '-P', str(script)],
                            capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise AssertionError(result.stdout + result.stderr)
    return dict(entry.split('|') for entry in out.read_text(encoding='utf-8').split(';') if entry)


@unittest.skipUnless(find_cmake() and CHUNKS_MODULE.is_file(), 'CMake not found')
class UnityGroupsTest(unittest.TestCase):
    JAVA = ['data/handlers/quest/x/_1000A.java', 'data/handlers/quest/x/_1003D.java', 'data/handlers/quest/x/_1001B.java',
            'data/handlers/quest/x/_1004E.java', 'data/handlers/quest/x/_1002C.java', 'data/handlers/Root.java']
    H = 'handlers/aion/gameserver/handlers/'

    def test_groups_follow_the_java_directory_and_do_not_move_when_files_are_added(self):
        with tempfile.TemporaryDirectory() as tmp:
            before = run_unity_groups(tmp, self.JAVA, [self.H + 'quest/x/_1000A.cpp', self.H + 'quest/x/_1002C.cpp', self.H + 'quest/x/_1004E.cpp',
                                                       self.H + 'quest/x/Helper.cpp', self.H + 'quest/y/Z.cpp', self.H + 'Root.cpp', 'src/x.cpp'], 2)
            self.assertEqual(before, {
                self.H + 'quest/x/_1000A.cpp': 'handlers_quest_x_0',   # Java index 0
                self.H + 'quest/x/_1002C.cpp': 'handlers_quest_x_1',   # Java index 2
                self.H + 'quest/x/_1004E.cpp': 'handlers_quest_x_2',   # Java index 4
                self.H + 'quest/x/Helper.cpp': 'handlers_quest_x_0',   # no Java file: sorts before _1000A ('H' < '_')
                self.H + 'quest/y/Z.cpp': 'handlers_quest_y_0',        # a directory without Java files is one group
                self.H + 'Root.cpp': 'handlers__0',
            })
            after = run_unity_groups(tmp, self.JAVA, list(before) + [self.H + 'quest/x/_1001B.cpp', self.H + 'quest/x/_1003D.cpp'], 2)
            self.assertEqual({k: after[k] for k in before}, before)
            self.assertEqual((after[self.H + 'quest/x/_1001B.cpp'], after[self.H + 'quest/x/_1003D.cpp']), ('handlers_quest_x_0', 'handlers_quest_x_1'))


@unittest.skipUnless(find_cmake() and CHUNKS_MODULE.is_file() and COMPILER_OPTIONS_MODULE.is_file(), 'CMake not found')
class FinalizeTest(unittest.TestCase):
    """configures a project with the fixture manifest (stand-ins for GoogleTest, commons and the registries) and inspects the targets"""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix='chunksfinalize')
        gs, jd = write_fixture(cls.tmp)
        project = Path(cls.tmp) / 'project'
        project.mkdir()
        quest = (gs / 'handlers/aion/gameserver/handlers/quest/x/_1000Q.cpp').as_posix()
        (project / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.28)
project(chunk_fixture LANGUAGES CXX)
set(AION_BUILD_TESTS ON)
add_library(fixture_gtest INTERFACE)
add_library(GTest::gtest ALIAS fixture_gtest)
add_library(GTest::gtest_main ALIAS fixture_gtest)
function(gtest_discover_tests)
endfunction()
include("{COMPILER_OPTIONS_MODULE.as_posix()}")
foreach(stand_in IN ITEMS aion_gs_build_options aion_commons aion_commons_core fixture_registry_empty)
	add_library(${{stand_in}} INTERFACE)
endforeach()
add_library(aion::commons ALIAS aion_commons)
include("{CHUNKS_MODULE.as_posix()}")
include("{(gs / 'chunks.cmake').as_posix()}")
aion_gs_finalize_chunks(SOURCE_DIR "{gs.as_posix()}" JAVA_DIR "{jd.as_posix()}" JSON "${{CMAKE_BINARY_DIR}}/chunks.json"
	REGISTRY_EMPTY fixture_registry_empty)
get_source_file_property(quest_group "{quest}" UNITY_GROUP)
file(GENERATE OUTPUT "${{CMAKE_BINARY_DIR}}/targets.txt" CONTENT "\\
handler_tests_links=$<TARGET_PROPERTY:aion_gs_handlers_a_tests,LINK_LIBRARIES>
handler_tests_includes=$<TARGET_PROPERTY:aion_gs_handlers_a_tests,INCLUDE_DIRECTORIES>
handler_unity=$<TARGET_PROPERTY:aion_gs_handlers_a,UNITY_BUILD>|$<TARGET_PROPERTY:aion_gs_handlers_a,UNITY_BUILD_MODE>
quest_group=${{quest_group}}
leaf_tests_links=$<TARGET_PROPERTY:aion_gs_leaf_tests,LINK_LIBRARIES>
model_tests_links=$<TARGET_PROPERTY:aion_gs_model_tests,LINK_LIBRARIES>
shared_tests_sources=$<TARGET_PROPERTY:aion_gs_shared_tests,SOURCES>
test_targets=$<TARGET_EXISTS:aion_gs_model_skip_tests>|$<TARGET_EXISTS:aion_gs_ak_tests>
")
''', encoding='utf-8')
        build = Path(cls.tmp) / 'build'
        cls.result = subprocess.run([find_cmake(), '-S', str(project), '-B', str(build)], capture_output=True, text=True, check=False)
        cls.values = {}
        targets = build / 'targets.txt'
        if targets.is_file():
            for line in targets.read_text(encoding='utf-8').splitlines():
                key, _, value = line.partition('=')
                cls.values[key] = value.split(';') if key != 'handler_unity' and key != 'quest_group' else value
        cls.gs = gs

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def setUp(self):
        self.assertEqual(self.result.returncode, 0, self.result.stdout + self.result.stderr)

    def test_handler_tests_link_their_library_and_get_the_handlers_include_root(self):
        links = self.values['handler_tests_links']
        self.assertEqual(links[:3], ['aion_gs_core', 'aion_gs_handlers_a', 'fixture_registry_empty'])
        self.assertIn((self.gs / 'handlers').as_posix(), self.values['handler_tests_includes'])

    def test_handler_unity_groups(self):
        self.assertEqual(self.values['handler_unity'], 'ON|GROUP')
        self.assertEqual(self.values['quest_group'], 'handlers_quest_x_0')

    def test_leaf_and_core_tests(self):
        self.assertEqual(self.values['leaf_tests_links'][:2], ['aion_gs_leaf', 'aion_gs_build_options'])
        self.assertEqual(self.values['model_tests_links'][:2], ['aion_gs_core', 'fixture_registry_empty'])
        self.assertEqual(self.values['test_targets'], ['1|0'])  # model_skip has tests/model_skip, ak has no test directory

    def test_shared_target_tests_come_from_both_chunks_directories(self):
        sources = sorted(Path(s).name for s in self.values['shared_tests_sources'])
        self.assertEqual(sources, ['AlphaTest.cpp', 'BetaTest.cpp'])


@unittest.skipUnless(chunks.DEFAULT_MANIFEST.is_file() and (chunks.JAVA_GAME_SERVER / 'src').is_dir(), 'real tree not available')
class RealTreeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = chunks.Manifest.load()
        cls.own = chunks.Ownership.of_tree(cls.manifest)

    def test_every_file_has_exactly_one_owner(self):
        self.assertEqual(self.own.problems(), [])
        self.assertGreater(len(self.own.cpp_files), 4000)
        self.assertGreater(len(self.own.test_files), 100)
        self.assertEqual(len(self.own.java_files), 2310 + 1729)

    def test_design_chunks_and_generated_file_assignments(self):
        names = {p.name for p in self.manifest.parts}
        design = {f'P4-{n:02d}' for n in range(1, 18)} - {'P4-02', 'P4-07', 'P4-11'} | {'P4-02a', 'P4-02b', 'P4-07a', 'P4-07b', 'P4-11a', 'P4-11b',
                                                                                        'P4-15a'}
        design |= {f'P5-{n:02d}' for n in range(0, 17)} - {'P5-02', 'P5-09', 'P5-12'} | {'P5-02a', 'P5-02b', 'P5-09a', 'P5-09b', 'P5-09c', 'P5-12a',
                                                                                     'P5-12b', 'P5-SC'}
        design |= {f'Q{n:02d}' for n in range(1, 15)} | {'A1', 'Z1', 'C1', 'C2'} | {f'I{n}' for n in range(1, 7)} | {'T2', 'T2-gen'}
        self.assertEqual(names, design)

        def owner(path):
            return [p.name for p in self.own.owners.get(path, [])]
        self.assertEqual(owner('src/aion/gameserver/model/DialogAction.h'), ['P4-05'])
        self.assertEqual(owner('src/aion/gameserver/model/DialogAction.gen.cpp'), ['P4-05'])
        self.assertEqual(owner('src/aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h'), ['P4-15'])
        self.assertEqual(owner('src/aion/gameserver/network/aion/ClientPacketInfo.gen.inc'), ['P4-15'])
        for i in range(8):
            self.assertEqual(owner(f'src/aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.gen{i}.cpp'), ['P4-06'])
        self.assertEqual(owner('src/aion/gameserver/configs/schedule/SiegeSchedules.h'), ['P4-09'])
        self.assertEqual(owner('handlers/aion/gameserver/handlers/quest/QuestPrelude.h'), ['Q01'])
        self.assertEqual(owner('handlers/aion/gameserver/handlers/ai/AiPrelude.h'), ['P5-05'])
        self.assertEqual(owner('handlers/aion/gameserver/handlers/CommandPrelude.h'), ['C1'])
        self.assertTrue(all(owner(f) == ['T2-gen'] for f in self.own.cpp_files if f.startswith('generated/')))

    def test_test_directories(self):
        def test_owner(path):
            owners, leases = self.manifest.test_dir_parts(path)
            return [p.name for p in owners], [p.name for p in leases]
        self.assertEqual(test_owner('tests/runtime/support/PctSupport.h'), (['P4-02a'], ['P4-02b']))
        self.assertEqual(test_owner('tests/runtime/stress/StressHarness.h'), (['P4-02a'], ['P4-02b']))
        self.assertEqual(test_owner('tests/runtime/sched/X.cpp'), (['P4-02b'], []))
        # chunks that share a target test in their own directories
        by_name = {(p.name, p.target): p.tests_dir for p in self.manifest.parts if p.target}
        self.assertEqual(by_name[('P4-07a', 'aion_gs_templates')], 'templates/P4-07a')
        self.assertEqual(by_name[('P4-07b', 'aion_gs_templates')], 'templates/P4-07b')
        self.assertEqual(by_name[('C1', 'aion_gs_handlers_commands')], 'handlers_commands/C1')
        self.assertEqual(by_name[('C2', 'aion_gs_handlers_commands')], 'handlers_commands/C2')
        # wave 5a pre-stage (m5a-plan.md I-01): the scenario harness chunk, the app's CMake scripts, the C-01 leases of stage 2
        self.assertEqual(by_name[('P5-SC', 'aion_gs_scenario')], 'scenario')
        self.assertEqual(test_owner('tests/scenario/ScenarioTests.cmake'), (['P5-SC'], []))
        owners, leases = self.manifest.other_file_parts('cmake/RunStartupSmoke.cmake')
        self.assertEqual(([p.name for p in owners], leases), (['P5-14'], []))
        self.assertEqual(self.manifest.other_file_parts('cmake/AionChunks.cmake'), ([], []))
        self.assertEqual(test_owner('tests/cm_ak/CM_CHAT_AUTHTest.cpp'), (['P5-15'], ['P4-16']))
        self.assertEqual(test_owner('tests/cm_lz/CM_MAY_QUITTest.cpp'), (['P5-16'], ['P4-17']))

    def test_c01_leases(self):
        cm = 'src/aion/gameserver/network/aion/clientpackets/'
        for cls, owner, lessee in (('CM_CHECK_MAIL_UNK', 'P5-15', 'P4-16'), ('CM_CUSTOM_SETTINGS', 'P5-15', 'P4-16'), ('CM_CHAT_AUTH', 'P5-15', 'P4-16'),
                                   ('CM_MAY_QUIT', 'P5-16', 'P4-17'), ('CM_PING_REQUEST', 'P5-16', 'P4-17'), ('CM_SHOW_FRIENDLIST', 'P5-16', 'P4-17'),
                                   ('CM_SUBZONE_CHANGE', 'P5-16', 'P4-17')):
            for ext in ('h', 'cpp'):
                owners, leases = chunks.owners_of(self.manifest, f'{cm}{cls}.{ext}')
                self.assertEqual(([p.name for p in owners], [p.name for p in leases]), ([owner], [lessee]), cls)
        owners, leases = chunks.owners_of(self.manifest, f'{cm}CM_MOVE.h')
        self.assertEqual(([p.name for p in owners], leases), (['P5-00'], []))

    @unittest.skipUnless(find_cmake() and CHECK_SCRIPT.is_file(), 'CMake not found')
    def test_cmake_agrees_on_the_real_tree(self):
        with tempfile.TemporaryDirectory() as tmp:
            json_path = Path(tmp) / 'chunks.json'
            status, output = run_cmake_check(chunks.GAME_SERVER, chunks.JAVA_GAME_SERVER, json_path)
            self.assertEqual(status, 0, output)
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                result = chunks.main(['verify-json', str(json_path)])
            self.assertEqual(result, 0, out.getvalue())


if __name__ == '__main__':
    unittest.main()
