"""The handler category preludes under cpp/game-server/handlers (handlers-and-porting-plan.md §1.2, S0a): one per Java handler category,
valid under the handler file rules (unity-safe) and self-contained (each compiles alone, twice in one TU and together with all others, with the
project's MSVC flags and /WX)."""
from __future__ import annotations

import os
import re
import shutil
import unittest

from tests import test_skeleton_support as ss

HANDLERS_ROOT = ss.CPP_ROOT / 'game-server' / 'handlers'
PRELUDE_DIR = HANDLERS_ROOT / 'aion' / 'gameserver' / 'handlers'
# Java category (data/handlers/<category>) -> prelude path below aion/gameserver/handlers
PRELUDES = {
    'ai': 'ai/AiPrelude.h',
    'instance': 'instance/InstancePrelude.h',
    'quest': 'quest/QuestPrelude.h',
    'zone': 'zone/ZonePrelude.h',
    'admincommands': 'admincommands/AdminCommandsPrelude.h',
    'playercommands': 'playercommands/PlayerCommandsPrelude.h',
    'consolecommands': 'consolecommands/ConsoleCommandsPrelude.h',
}
# The PCH of the one command library (chunks C1/C2): includes the three command preludes, declares nothing
COMMAND_PCH = 'CommandPrelude.h'
QUEST_DIRECTIVE = 'using namespace aion::gameserver::model::DialogAction;'
CAN_COMPILE = ss.find_cmake() is not None and os.name == 'nt' and os.environ.get('AION_SKELETON_SKIP_COMPILE') != '1'


def prelude_files():
    return sorted(set(PRELUDES.values()))


def using_declaration_names(text):
    """Simple names that `using a::b::Name;` declarations (not directives or aliases) of a header introduce."""
    return re.findall(r'\busing\s+(?!namespace\b)(?:typename\s+)?[\w:]*::(\w+)\s*;', strip_comments(text))


def strip_comments(text):
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    text = re.sub(r'"(?:[^"\\\n]|\\.)*"', '""', text)
    return re.sub(r'//[^\n]*', '', text)


@unittest.skipUnless(PRELUDE_DIR.is_dir(), 'cpp/game-server/handlers not found')
class HandlerPreludeTest(unittest.TestCase):

    @unittest.skipUnless(ss.REAL_HANDLERS_ROOT.is_dir(), 'game-server/data/handlers not found')
    def test_one_prelude_per_java_category(self):
        categories = sorted(p.name for p in ss.REAL_HANDLERS_ROOT.iterdir() if p.is_dir())
        self.assertEqual(categories, sorted(PRELUDES))
        for rel in prelude_files():
            self.assertTrue((PRELUDE_DIR / rel).is_file(), rel)

    def test_only_known_preludes(self):
        found = sorted(p.relative_to(PRELUDE_DIR).as_posix() for p in PRELUDE_DIR.rglob('*Prelude.h'))
        self.assertEqual(found, sorted(prelude_files() + [COMMAND_PCH]))

    def test_command_pch_only_includes_the_command_preludes(self):
        """Each command package declares into its own namespace; the shared PCH adds no declarations of its own (review S0a: using-declarations
        in the common parent namespace resolve differently depending on the unity batch)."""
        text = (PRELUDE_DIR / COMMAND_PCH).read_text(encoding='utf-8')
        code = [line for line in re.sub(r'//[^\n]*', '', text).splitlines() if line.strip()]
        self.assertEqual(code, ['#pragma once'] + sorted(f'#include "aion/gameserver/handlers/{PRELUDES[c]}"'
                                                         for c in ('admincommands', 'consolecommands', 'playercommands')))

    @unittest.skipUnless(ss.REAL_HANDLERS_ROOT.is_dir(), 'game-server/data/handlers not found')
    def test_using_declarations_do_not_name_handler_classes(self):
        """A name a prelude re-exports must not be the simple name of a Java type of its category (any subpackage): an earlier file of a unity
        batch that declares the handler class would otherwise change what the name means in later files of the same namespace."""
        for category, rel in sorted(PRELUDES.items()):
            java = {p.stem for p in (ss.REAL_HANDLERS_ROOT / category).rglob('*.java')}
            for name in using_declaration_names((PRELUDE_DIR / rel).read_text(encoding='utf-8')):
                self.assertNotIn(name, java, f'{rel} re-exports {name}, the name of a handler class of data/handlers/{category}')
        self.assertEqual(using_declaration_names('using gameserver::model::gameobjects::Pet;\nusing namespace x::Y;\n// using a::B;\n'
                                                 'using Alias = a::C;\n'), ['Pet'])

    def test_handler_file_rules(self):
        """The regscan file rules as they apply to a header without markers: LF, #pragma once, exactly one block in the namespace of the
        directory, nothing at namespace scope outside it, no `static`, no using-directive except the quest prelude's DialogAction one."""
        for rel in prelude_files():
            raw = (PRELUDE_DIR / rel).read_bytes()
            self.assertNotIn(b'\r', raw, rel)
            text = raw.decode('utf-8')
            self.assertTrue(text.startswith('#pragma once\n'), rel)
            directory = rel.rsplit('/', 1)[0] if '/' in rel else ''
            expected = '::'.join(['aion', 'gameserver', 'handlers'] + ([directory] if directory else []))
            code = strip_comments(text)
            namespaces = re.findall(r'\bnamespace\s+([\w:]+)\s*\{', code)
            self.assertEqual(namespaces, [expected], rel)
            outside = re.sub(r'\bnamespace\s+[\w:]+\s*\{.*\}', '', code, flags=re.S)
            outside = '\n'.join(line for line in outside.splitlines() if line.strip() and not line.lstrip().startswith('#'))
            self.assertEqual(outside, '', f'{rel}: code outside the package namespace')
            self.assertNotRegex(code, r'\bstatic\b', rel)
            self.assertNotRegex(code, r'#\s*(?:if|ifdef|ifndef|define)\b', rel)
            directives = re.findall(r'\busing\s+namespace\s+[\w:]+\s*;', code)
            self.assertEqual(directives, [QUEST_DIRECTIVE] if rel == PRELUDES['quest'] else [], rel)
        self.assertIn('#include "aion/gameserver/model/DialogAction.h"', (PRELUDE_DIR / PRELUDES['quest']).read_text(encoding='utf-8'))

    @unittest.skipUnless(CAN_COMPILE, 'needs CMake and MSVC (AION_SKELETON_SKIP_COMPILE=1 skips)')
    def test_compile(self):
        work = ss.short_temp_dir('skpre')
        try:
            includes = [f'aion/gameserver/handlers/{rel}' for rel in prelude_files() + [COMMAND_PCH]]
            sources = []
            for i, inc in enumerate(includes):
                alone = work / f'alone_{i}.cpp'
                alone.write_text(f'#include "{inc}"\n', encoding='utf-8')
                twice = work / f'twice_{i}.cpp'                                 # a unity batch includes the prelude once per file
                twice.write_text(f'#include "{inc}"\n#include "{inc}"\n', encoding='utf-8')
                sources += [alone, twice]
            together = work / 'together.cpp'
            together.write_text(''.join(f'#include "{inc}"\n' for inc in includes), encoding='utf-8')
            sources.append(together)
            use = work / 'quest_use.cpp'                                        # the quest prelude's directive reaches nested packages
            use.write_text(f'#include "aion/gameserver/handlers/{PRELUDES["quest"]}"\n'
                           'namespace aion::gameserver::handlers::quest::heiron {\n'
                           'inline constexpr int32_t SELECT = SELECT_QUEST_REWARD;\n'
                           '} // namespace aion::gameserver::handlers::quest::heiron\n', encoding='utf-8')
            sources.append(use)
            ok, output = ss.compile_check(work, [HANDLERS_ROOT, ss.REAL_CPP_SRC, ss.REAL_COMMONS_SRC, ss.VCPKG_INCLUDE], sources)
            problems = [line for line in ss.warnings_in(output) if 'MSB80' not in line]
            self.assertTrue(ok and not problems, '\n'.join(problems[:40]) or output[-4000:])
        finally:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    unittest.main()
