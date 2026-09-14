"""skeleton.py over the real game server tree: forward headers for every package (the drift check of the committed fwd.h files under
cpp/game-server/src, a compile check of each of them alone and with the generated definitions of its types, and of all of them together with
the existing C++ headers), drafts for every Java file, and a compile check of a draft set (the DAOs with their dependencies)."""
from __future__ import annotations

import io
import os
import re
import shutil
import unittest
from contextlib import redirect_stderr, redirect_stdout

from tests import test_skeleton_support as ss

import skeleton

P = 'com.aionemu.gameserver.'
HAVE_TREE = ss.REAL_JAVA_ROOT.is_dir() and ss.REAL_CPP_SRC.is_dir()
CAN_COMPILE = ss.find_cmake() is not None and os.name == 'nt' and os.environ.get('AION_SKELETON_SKIP_COMPILE') != '1'


def real_fieldmap():
    return skeleton.Fieldmap.load(skeleton.DEFAULT_FIELDMAP) if skeleton.DEFAULT_FIELDMAP.is_file() else skeleton.Fieldmap()


@unittest.skipUnless(HAVE_TREE, 'game-server sources not found')
class RealTreeForwardHeadersTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.project = skeleton.Project(ss.REAL_JAVA_ROOT, None, ss.REAL_CPP_SRC, ss.REAL_COMMONS_SRC, generated_root=ss.REAL_GENERATED)
        cls.files = skeleton.generate_fwd(cls.project)

    def test_every_type_declared_once(self):
        packages = {cu.package for cu in self.project.core_units if cu.types}
        self.assertEqual(len(self.files), len(packages))
        declared = 0
        for text in self.files.values():
            declared += sum(1 for line in text.splitlines() if line.endswith(';') and not line.startswith('//'))
        tops = [td for cu in self.project.core_units for td in cu.types if td.kind != 'annotation']
        namespaces = [td for td in tops if self.project.unit_ns(td.cu) + (td.name,) in self.project.cpp.namespaces]   # model::DialogAction
        aliases = [td for td in tops if (self.project.unit_ns(td.cu), td.name) in self.project.cpp.aliases
                   and self.project.cpp.declaration(self.project.unit_ns(td.cu), td.name) is None and td not in namespaces]
        self.assertEqual(declared, len(tops) - len(namespaces) - len(aliases))
        self.assertGreater(len(tops), 2300)

    def test_known_declarations(self):
        f = self.files
        self.assertIn('enum class TribeClass : std::uint16_t;', f['aion/gameserver/model/fwd.h'])                 # 724 constants
        self.assertIn('enum class Race : std::uint8_t;', f['aion/gameserver/model/fwd.h'])
        self.assertIn('struct GSConfig;', f['aion/gameserver/configs/main/fwd.h'])                               # existing C++ struct
        self.assertIn('class Vector3f;', f['aion/gameserver/geoEngine/math/fwd.h'])
        self.assertIn('struct FastMath;', f['aion/gameserver/geoEngine/math/fwd.h'])
        self.assertIn('\nclass CreatureController;', f['aion/gameserver/controllers/fwd.h'])            # erased generic (hub-headers.md §8.1)
        self.assertIn('template <class Type> class SplitList;', f['aion/gameserver/utils/collections/fwd.h'])   # unbounded: template
        self.assertIn('class Player;', f['aion/gameserver/model/gameobjects/player/fwd.h'])
        self.assertIn('namespace aion::gameserver::questEngine::handlers::template_ {', f['aion/gameserver/questEngine/handlers/template/fwd.h'])
        self.assertIn('//   SecurityConfig::MultiClientingRestrictionMode', f['aion/gameserver/configs/main/fwd.h'])
        self.assertIn('//   @AIName', f['aion/gameserver/ai/fwd.h'])
        self.assertIn('\nclass AbstractAI;', f['aion/gameserver/ai/fwd.h'])                                  # non-template (HandlerRegistry.h)
        self.assertIn('struct RouteVersion;', f['aion/gameserver/model/templates/walker/fwd.h'])            # xmlgen data struct
        self.assertIn('struct EnchantList;', f['aion/gameserver/model/enchants/fwd.h'])
        self.assertIn('// ServerPacketsOpcodes: a C++ namespace', f['aion/gameserver/network/aion/fwd.h'])

    def test_class_keys_match_the_generated_tree(self):
        """Every type xmlgen defines as a struct/class/enum is forward declared with the same key (C4099 and V/U name mangling)."""
        defined = re.compile(r'^(struct|class|enum class) (\w+)\b(?![^\n]*;$)', re.M)
        namespace = re.compile(r'^namespace ([\w:]+) \{', re.M)
        fwd_keys = {}
        for rel, text in self.files.items():
            ns = namespace.search(text).group(1)
            for key, name in re.findall(r'^(struct|class|enum class) (\w+)\b[^\n]*;$', text, re.M):
                fwd_keys[(ns, name)] = key
        checked = 0
        for path in sorted(ss.REAL_GENERATED.rglob('*.h')):
            text = path.read_text(encoding='utf-8')
            m = namespace.search(text)
            if m is None:
                continue
            for key, name in defined.findall(text):
                if (m.group(1), name) in fwd_keys:
                    checked += 1
                    self.assertEqual(fwd_keys[(m.group(1), name)], key, f'{m.group(1)}::{name} ({path.name})')
        self.assertGreater(checked, 250)

    def test_deterministic_and_check(self):
        self.assertEqual(skeleton.generate_fwd(self.project), self.files)
        root = ss.short_temp_dir('skfwdchk')
        try:
            self.assertEqual(skeleton.write_files(root, self.files), (len(self.files), 0))
            self.assertEqual(skeleton.check_files(root, self.files, skeleton.FWD_MARK), [])
            self.assertEqual(skeleton.write_files(root, self.files), (0, len(self.files)))
        finally:
            shutil.rmtree(root, ignore_errors=True)

    def test_committed_forward_headers_are_current(self):
        """Drift check of the committed forward headers (S0a decision 5): `skeleton.py --fwd --check --out cpp/game-server/src`."""
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            code = skeleton.main(['--fwd', '--check', '--out', str(ss.REAL_CPP_SRC), '--java-root', str(ss.REAL_JAVA_ROOT),
                                  '--cpp-src', str(ss.REAL_CPP_SRC), '--commons-src', str(ss.REAL_COMMONS_SRC),
                                  '--generated-root', str(ss.REAL_GENERATED)])
        self.assertEqual(code, 0, 'the committed fwd.h files are out of date; regenerate with: python cpp/tools/gen/skeleton.py --fwd --out '
                                  'cpp/game-server/src\n' + out.getvalue()[:4000] + err.getvalue()[-2000:])
        self.assertEqual(skeleton.check_files(ss.REAL_CPP_SRC, self.files, skeleton.FWD_MARK), [])

    @unittest.skipUnless(CAN_COMPILE, 'needs CMake and MSVC (AION_SKELETON_SKIP_COMPILE=1 skips)')
    def test_compile_each_forward_header(self):
        """Every fwd.h alone in a TU, and every fwd.h followed by the generated headers that define the types it declares (xmlgen enums
        with their underlying type, data structs with their class key), with /W4 /WX: a differing enum base is an error, a differing class
        key is C4099."""
        namespace = re.compile(r'^namespace ([\w:]+) \{', re.M)
        declared = re.compile(r'^(?:template <[^>]*> )?(?:struct|class|enum class) (\w+)\b[^\n]*;$', re.M)
        resolvable = set(_resolvable_generated_headers())
        work = ss.short_temp_dir('skfwd1')
        try:
            ss.write_tree(work / 'inc', self.files)
            sources = []
            with_definitions = 0
            for i, rel in enumerate(sorted(self.files)):
                text = self.files[rel]
                alone = work / f'alone_{i}.cpp'
                alone.write_text(f'#include "{rel}"\n', encoding='utf-8')
                sources.append(alone)
                ns = tuple(namespace.search(text).group(1).split('::'))
                definitions = set()
                for name in declared.findall(text):
                    d = self.project.cpp.lookup(ns, name)
                    if (d is not None and d.key in ('enum', 'struct', 'class') and d.path in resolvable and not d.path.endswith(('.xml.h', '.bind.h'))
                            and (ss.REAL_GENERATED / d.path).is_file() and not (ss.REAL_CPP_SRC / d.path).is_file()):
                        definitions.add(d.path)
                if definitions:
                    with_definitions += 1
                    tu = work / f'defs_{i}.cpp'
                    tu.write_text(f'#include "{rel}"\n' + ''.join(f'#include "{h}"\n' for h in sorted(definitions)), encoding='utf-8')
                    sources.append(tu)
            self.assertGreater(with_definitions, 40)
            ok, output = ss.compile_check(work, [work / 'inc', ss.REAL_CPP_SRC, ss.REAL_GENERATED, ss.REAL_COMMONS_SRC, ss.VCPKG_INCLUDE],
                                          sources)
            problems = [line for line in ss.warnings_in(output) if 'MSB80' not in line]
            self.assertTrue(ok and not problems, '\n'.join(problems[:40]) or output[-4000:])
        finally:
            shutil.rmtree(work, ignore_errors=True)

    @unittest.skipUnless(CAN_COMPILE, 'needs CMake and MSVC (AION_SKELETON_SKIP_COMPILE=1 skips)')
    def test_compile_with_existing_headers(self):
        """All forward headers in one TU, plus one TU per existing C++ header of a Java class (class-key and enum consistency)."""
        work = ss.short_temp_dir('skfwd')
        try:
            ss.write_tree(work / 'inc', self.files)
            includes = ''.join(f'#include "{f}"\n' for f in sorted(self.files))
            (work / 'all_fwd.cpp').write_text(includes, encoding='utf-8')
            sources = [work / 'all_fwd.cpp']
            existing = sorted({d.path for d in self.project.cpp.decls.values() if (ss.REAL_CPP_SRC / d.path).is_file()
                               and self.project.core_type(P + '.'.join(d.path.split('/')[2:-1] + [d.path.rsplit('/', 1)[1][:-2]])) is not None})
            self.assertGreater(len(existing), 40)
            for i, header in enumerate(existing):
                tu = work / f'existing_{i}.cpp'
                tu.write_text(includes + f'#include "{header}"\n', encoding='utf-8')
                sources.append(tu)
            # one TU with every forward header, every generated header whose includes exist (data structs and enums: behaviour classes
            # need hand-written headers) and HandlerRegistry.h, which forward declares core classes itself
            generated = [h for h in _resolvable_generated_headers() if not h.endswith(('.xml.h', '.bind.h'))]
            self.assertGreater(len(generated), 200)
            together = work / 'fwd_generated_registry.cpp'
            together.write_text(includes + ''.join(f'#include "{h}"\n' for h in generated)
                                + '#include "aion/gameserver/handlers/HandlerRegistry.h"\n', encoding='utf-8')
            sources.append(together)
            ok, output = ss.compile_check(work, [work / 'inc', ss.REAL_CPP_SRC, ss.REAL_GENERATED, ss.REAL_COMMONS_SRC, ss.VCPKG_INCLUDE],
                                          sources)
            problems = [line for line in ss.warnings_in(output) if 'MSB80' not in line]
            self.assertTrue(ok and not problems, '\n'.join(problems[:40]) or output[-4000:])
        finally:
            shutil.rmtree(work, ignore_errors=True)


@unittest.skipUnless(HAVE_TREE, 'game-server sources not found')
class RealTreeDraftsTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.project = skeleton.Project(ss.REAL_JAVA_ROOT, ss.REAL_HANDLERS_ROOT, ss.REAL_CPP_SRC, ss.REAL_COMMONS_SRC, real_fieldmap(),
                                       ss.REAL_GENERATED)

    def test_drafts_for_every_java_file(self):
        selected = self.project.select(['@all'])
        files = skeleton.generate_drafts(self.project, selected)
        units = {id(td.cu) for td in selected}
        self.assertEqual(len(files), 2 * len(units))
        self.assertGreater(len(units), 500)           # S0a: enums generated, behaviour classes scaffolded; S0c: declaration headers exist
        skipped_units = {id(td.cu) for cu in self.project.core_units for td in cu.types if td.fqn in self.project.skipped_existing}
        self.assertGreater(len(units) + len(skipped_units), 2200, 'every Java file is drafted or skipped as ported/generator-owned')
        # no draft defines an enum xmlgen generates (nested ones become aliases, secondary top-level ones includes): 141 core + JAXB enums
        self.assertGreater(len(self.project.xmlgen_enums), 250)
        redefined = []
        for fqn in self.project.xmlgen_enums:
            td = self.project.index.types.get(fqn)
            text = files.get(self.project.unit_header(td.cu), '') if td is not None else ''
            if re.search(r'(?m)^\s*enum class ' + re.escape(skeleton.cpp_ident(td.name)) + r'\b', text):
                redefined.append(fqn)
        self.assertEqual(redefined, [])
        for rel, text in files.items():
            self.assertTrue(text.startswith(skeleton.DRAFT_MARK), rel)
            if rel.endswith('.h'):
                self.assertIn('\n#pragma once\n', text, rel)
                self.assertTrue(text.rstrip().endswith('} // namespace ' + '::'.join(self.project.unit_ns(
                    next(td for td in selected if self.project.unit_header(td.cu) == rel).cu))), rel)

    def test_groups(self):
        # group selectors skip classes whose header exists: after S0c (declaration headers of every DAO, service and server packet) the
        # selected groups shrink towards empty, so the sizes are checked on the unfiltered groups and explicit names still draft a class
        tops = [td for cu in self.project.core_units for td in cu.types]
        for group, minimum, example in (('@daos', 55, 'dao.PlayerDAO'), ('@services', 121, 'services.player.PlayerService'),
                                        ('@serverpackets', 231, 'network.aion.serverpackets.SM_ATTACK_STATUS')):
            with self.subTest(group=group):
                unfiltered = {td.fqn for td in self.project._group(group, tops)}
                self.assertGreaterEqual(len(unfiltered), minimum)
                selected = self.project.select([group])
                self.assertLessEqual({td.fqn for td in selected}, unfiltered)
                self.assertFalse([td.fqn for td in selected if self.project.already_ported(td)])
                self.assertIn(P + example, unfiltered)
                self.assertEqual([td.fqn for td in self.project.select([P + example])], [P + example])
        packets = self.project.select(['@serverpackets'])
        # @hubs skips hub files whose header exists: after S0b every hub is written, so the group is empty (explicit names still draft them)
        hubs = {td.fqn for td in self.project.select(['@hubs'])}
        self.assertLessEqual(hubs, {P + rel for rel in skeleton.HUBS})
        self.assertNotIn(P + 'model.gameobjects.player.Player', hubs)
        self.assertTrue((ss.REAL_CPP_SRC / 'aion/gameserver/model/gameobjects/player/Player.h').is_file())
        self.assertEqual([td.fqn for td in self.project.select([P + 'model.gameobjects.player.Player'])], [P + 'model.gameobjects.player.Player'])
        self.assertNotIn(P + 'utils.ThreadPoolManager', hubs)
        self.assertNotIn(P + 'skillengine.model.SkillTemplate', hubs)                  # xmlgen behaviour class: xmlgen.py scaffold
        packets = {td.fqn for td in packets}
        self.assertNotIn(P + 'network.aion.serverpackets.SM_SYSTEM_MESSAGE', packets)  # sysmsg.py member block
        everything = {td.fqn for td in self.project.select(['@all'])}
        self.assertNotIn(P + 'network.aion.ServerPacketsOpcodes', everything)          # opcodes.py namespace
        self.assertNotIn(P + 'model.templates.walker.RouteVersion', everything)       # xmlgen data struct
        self.assertNotIn(P + 'model.DialogAction', everything)                        # dialogaction.py
        with self.assertRaisesRegex(skeleton.SkeletonError, 'static data class'):
            self.project.select([P + 'model.templates.walker.RouteVersion'])

    def test_header_check_lists_every_hub(self):
        """game-server/CMakeLists.txt gs_hub_headers (aion_gs_header_check, docs/design/hub-headers.md §3.4) equals skeleton.HUBS without the
        enums xmlgen generates, plus the kernel ports ThreadPoolManager and IDFactory."""
        text = (ss.CPP_ROOT / 'game-server' / 'CMakeLists.txt').read_text(encoding='utf-8')
        block = re.search(r'set\(gs_hub_headers\n(.*?)\n\)', text, re.S)
        self.assertIsNotNone(block, 'set(gs_hub_headers ...) not found')
        listed = block.group(1).split()
        self.assertEqual(listed, sorted(listed), 'keep gs_hub_headers sorted')
        expected = {'aion/gameserver/utils/ThreadPoolManager.h', 'aion/gameserver/utils/idfactory/IDFactory.h'}
        for rel in skeleton.HUBS:
            td = self.project.core_type(P + rel)
            if td is not None and self.project.generated_enum(td) is None:
                expected.add(self.project.unit_header(td.cu))
        self.assertEqual(set(listed), expected)
        spine = re.search(r'set\(gs_spine_headers\n(.*?)\n\)', text, re.S)
        self.assertIsNotNone(spine, 'set(gs_spine_headers ...) not found')
        self.assertEqual(spine.group(1).split(), sorted(skeleton.SPINE_HEADERS), 'gs_spine_headers equals skeleton.SPINE_HEADERS, sorted')
        for rel in skeleton.SPINE_HEADERS:
            self.assertTrue((ss.REAL_CPP_SRC / rel).is_file(), rel)

    def test_hub_rules_on_the_real_tree(self):
        """The generics decisions of docs/design/hub-headers.md §8.1 hold for the real hub classes."""
        erased = ['controllers.VisibleObjectController', 'controllers.CreatureController', 'controllers.movement.CreatureMoveController',
                  'model.stats.container.CreatureGameStats', 'model.stats.container.CreatureLifeStats', 'model.team.GeneralTeam',
                  'model.team.TemporaryPlayerTeam', 'model.team.TeamMember', 'ai.AbstractAI']
        for rel in erased:
            self.assertTrue(self.project.erased_generic(self.project.index.types[P + rel]), rel)
        self.assertFalse(self.project.erased_generic(self.project.index.types[P + 'ai.AITemplate']))
        self.assertIn(P + 'instance.handlers.InstanceHandler', self.project.retainable_interfaces())
        self.assertIn(P + 'model.stats.calc.StatOwner', self.project.retainable_interfaces())
        npc = self.project.index.types[P + 'model.gameobjects.Npc']
        self.assertTrue(skeleton.Project.is_cast_override(next(m for m in npc.methods if m.name == 'getController')))

    def test_member_block_and_non_template_drafts(self):
        files = skeleton.generate_drafts(self.project, self.project.select([P + 'network.aion.serverpackets.SM_SYSTEM_MESSAGE',
                                                                             P + 'ai.AbstractAI']))
        sysmsg = files['aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h']
        self.assertIn('#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.gen.h"', sysmsg)
        self.assertLess(sysmsg.count('STR_'), 40, 'only the hand-ported factories are drafted')
        self.assertIn('STR_SKILL_ABYSS_SKILL_IS_FIRED', sysmsg)
        self.assertNotIn('_STR_MSG_Heal_TO_ME', sysmsg)
        ai = files['aion/gameserver/ai/AbstractAI.h']
        self.assertRegex(ai, r'\nclass AbstractAI\b')
        self.assertNotIn('template <class T>\nclass AbstractAI', ai)

    @unittest.skipUnless(CAN_COMPILE, 'needs CMake and MSVC (AION_SKELETON_SKIP_COMPILE=1 skips)')
    def test_compile_drafts(self):
        """The DAO drafts plus every draft their headers need compile with /W4 /WX against the real runtime and commons headers
        (AION_SKELETON_FULL_COMPILE=1 compiles the drafts of every Java file instead, several minutes). After S0c every DAO has a header,
        so the group selector @daos drafts nothing: the DAOs are selected by their FQNs, which draft them anyway."""
        if os.environ.get('AION_SKELETON_FULL_COMPILE') == '1':
            selected = self.project.select(['@all'])
        else:
            tops = [td for cu in self.project.core_units for td in cu.types]
            selected = self.project.select(sorted(td.fqn for td in self.project._group('@daos', tops)))
        self.assertGreaterEqual(len(selected), 55)
        drafts = skeleton.generate_drafts(self.project, selected, with_dependencies=True)
        fwd = skeleton.generate_fwd(self.project)
        work = ss.short_temp_dir('skdr')
        try:
            ss.write_tree(work / 'inc', {**fwd, **drafts})
            sources = [work / 'inc' / f for f in sorted(drafts) if f.endswith('.cpp')]
            ok, output = ss.compile_check(work, [work / 'inc', ss.REAL_CPP_SRC, ss.REAL_GENERATED, ss.REAL_COMMONS_SRC, ss.VCPKG_INCLUDE],
                                          sources)
            problems = [line for line in ss.warnings_in(output) if 'MSB80' not in line]
            self.assertTrue(ok and not problems, '\n'.join(problems[:40]) or output[-4000:])
        finally:
            shutil.rmtree(work, ignore_errors=True)


class SpineGuardsTest(unittest.TestCase):
    """`__has_include` guards (docs/design/hub-headers.md §3.3): an open guard is an error (MSBuild does not rebuild a guarded .cpp when the
    header appears), S0b transition guards on hub and spine headers are tolerated until the freeze, every guard fails the freeze gate."""

    def test_rules(self):
        root = ss.short_temp_dir('skguards')
        try:
            src, tests = root / 'src', root / 'tests'
            ss.write_tree(src, {
                'aion/gameserver/model/X.h': '#pragma once\n',
                'aion/gameserver/model/gameobjects/Creature.h': '#pragma once\n',
                'aion/gameserver/model/Open.cpp': '#if __has_include("aion/gameserver/model/X.h")\n#endif\n',
                'aion/gameserver/model/Hub.cpp': '// S0b transition\n#if __has_include("aion/gameserver/model/gameobjects/Creature.h")\n#endif\n',
                'aion/gameserver/model/Waiting.cpp': ('#include <x>\n#if __has_include("aion/gameserver/model/X.h") && \\\n'
                                                      '\t__has_include("aion/gameserver/model/Missing.h")\n#else\n#endif\n'),
                'aion/gameserver/runtime/Kernel.cpp': '#if __has_include(<sanitizer/asan_interface.h>)\n#endif\n',
            })
            ss.write_tree(tests, {'objects/T.cpp': '#  elif __has_include(<aion/gameserver/model/Missing.h>)\n'})
            guards = skeleton.spine_guards([src, tests], [src])
            by_path = {g.path: g for g in guards}
            self.assertEqual(sorted(by_path), ['src/aion/gameserver/model/Hub.cpp', 'src/aion/gameserver/model/Open.cpp',
                                               'src/aion/gameserver/model/Waiting.cpp', 'tests/objects/T.cpp'])
            waiting = by_path['src/aion/gameserver/model/Waiting.cpp']
            self.assertEqual((waiting.line, waiting.missing), (2, ['aion/gameserver/model/Missing.h']))
            self.assertEqual(len(waiting.headers), 2)
            problems = skeleton.guard_problems(guards, frozen=False)
            self.assertEqual(len(problems), 1)
            self.assertIn('src/aion/gameserver/model/Open.cpp:1: open guard', problems[0])
            self.assertEqual(len(skeleton.guard_problems(guards, frozen=True)), 4)
        finally:
            shutil.rmtree(root, ignore_errors=True)

    @unittest.skipUnless(HAVE_TREE, 'game-server sources not found')
    def test_real_tree(self):
        """No open guard outside the tolerated S0b transition guards; SPINE_FROZEN (set at the freeze) turns every guard into a failure."""
        guards = skeleton.spine_guards([ss.REAL_CPP_SRC, ss.REAL_CPP_SRC.parent / 'tests', ss.REAL_CPP_SRC.parent / 'handlers'],
                                       [ss.REAL_CPP_SRC, ss.REAL_CPP_SRC.parent / 'handlers', ss.REAL_GENERATED, ss.REAL_COMMONS_SRC])
        self.assertEqual(skeleton.guard_problems(guards), [])


class HubDefinitionsTest(unittest.TestCase):
    """S0B-120: every member function a hub or spine header declares without a body has a definition (skeleton.py --definitions), so no hub
    constructor, destructor or stub is left undefined (a link error at the first use)."""

    HEADER = (
        '#pragma once\n#include <string>\nnamespace a::b {\n'
        'class X : public Base {\n\tAION_MAKE_REF_FRIEND\npublic:\n'
        '\tstatic constexpr int32_t N = 5;\n\truntime::Field<int32_t> f{AION_LOCK_CLASS(X::f)};\n'
        '\tX(int32_t id);\n\t~X() override;\n\tvirtual void pure() = 0;\n\tX(const X&) = delete;\n'
        '\tint32_t inlineGetter() const { return f.get(); }\n\tvoid defined(std::string_view s, const std::function<void(int)>& fn);\n'
        '\tbool operator==(const X& other) const;\n\tvoid missing(int32_t a = 1) const noexcept override;\n'
        '\t// void commented();\n\tusing Base::defined;\n\tfriend class Y;\n'
        '\ttemplate <class T> void generic(T t) { AION_UNPORTED(); }\n'
        '\tenum class E : int8_t { A, B };\n\tstruct Inner {\n\t\tvoid innerMissing();\n\t\tint32_t v{0};\n\t};\n'
        '\tvoid afterInner();\n};\n'
        'template <class T> class Tpl {\npublic:\n\tvoid templated();\n};\n}\n')

    def test_declarations_and_definitions(self):
        decls = skeleton.declared_member_functions(self.HEADER)
        self.assertEqual([('::'.join(c), n) for c, n, _ in decls],
                         [('X', 'X'), ('X', '~X'), ('X', 'defined'), ('X', 'operator=='), ('X', 'missing'), ('X::Inner', 'innerMissing'),
                          ('X', 'afterInner')])
        root = ss.short_temp_dir('skdefs')
        try:
            ss.write_tree(root, {
                'aion/X.h': self.HEADER,
                'aion/X.cpp': ('#include "aion/X.h"\nnamespace a::b {\nX::X(int32_t id) : f(id) {}\nX::~X() = default;\n'
                               'void X::defined(std::string_view s, const std::function<void(int)>& fn) {}\n'
                               'bool X::operator==(const X& other) const { return true; }\nvoid X::Inner::innerMissing() {}\n}\n'),
                'aion/Other.cpp': 'namespace a::b {\nvoid X::afterInner() {}\n// void X::missing(int32_t a) const noexcept {}\n}\n',
            })
            missing = skeleton.undefined_member_functions(['aion/X.h'], root, [root])
            self.assertEqual([(h, q) for h, q, _ in missing], [('aion/X.h', 'X::missing')])
        finally:
            shutil.rmtree(root, ignore_errors=True)

    @unittest.skipUnless(HAVE_TREE, 'game-server sources not found')
    def test_real_tree(self):
        headers = skeleton.spine_header_set()
        declared = sum(len(skeleton.declared_member_functions((ss.REAL_CPP_SRC / h).read_text(encoding='utf-8')))
                       for h in headers if (ss.REAL_CPP_SRC / h).is_file())
        self.assertGreater(declared, 1500)
        self.assertEqual(skeleton.undefined_member_functions(headers, ss.REAL_CPP_SRC, [ss.REAL_CPP_SRC]), [])


def _resolvable_generated_headers():
    """Generated headers whose quoted includes all exist (transitively) under src, generated and commons."""
    roots = (ss.REAL_CPP_SRC, ss.REAL_GENERATED, ss.REAL_COMMONS_SRC)
    include = re.compile(r'^#include "([^"]+)"', re.M)
    memo = {}

    def resolvable(rel):
        if rel not in memo:
            path = next((r / rel for r in roots if (r / rel).is_file()), None)
            memo[rel] = path is not None          # cycle guard
            if path is not None:
                memo[rel] = all(resolvable(i) for i in include.findall(path.read_text(encoding='utf-8')))
        return memo[rel]

    headers = sorted(p.relative_to(ss.REAL_GENERATED).as_posix() for p in ss.REAL_GENERATED.rglob('*.h'))
    return [h for h in headers if resolvable(h)]


if __name__ == '__main__':
    unittest.main()
