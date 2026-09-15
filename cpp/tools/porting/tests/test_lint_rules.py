"""lint_concurrency.py: scanner and rules L1-L20 on C++ fixtures."""
import contextlib
import io
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import lint_concurrency as lc  # noqa: E402

G = 'com.aionemu.gameserver.model.'
FIELDMAP = {
    'format': 'aion-fieldmap',
    'classes': {
        G + 'Npc': {'kind': 'K4', 'cppName': 'Npc', 'fields': [
            {'name': 'hp', 'cpp': 'Field<int32_t>', 'java': 'int', 'line': 3, 'rule': 'non-final scalar'},
            {'name': 'target', 'cpp': 'Field<Ref<Creature>>', 'java': 'Creature', 'line': 4, 'rule': 'non-final object reference'},
            {'name': 'knownList', 'cpp': 'const std::unique_ptr<KnownList>', 'java': 'KnownList', 'line': 5, 'rule': 'part (pattern 1)'},
            {'name': 'MAX', 'cpp': 'static constexpr int32_t', 'java': 'int', 'line': 6, 'rule': 'static constant', 'modifiers': ['static', 'final']},
            {'name': 'missing', 'cpp': 'Field<bool>', 'java': 'boolean', 'line': 7, 'rule': 'non-final scalar'},
        ], 'sync': {'move': {'synchronized': 1, 'lock': 0}}},
        G + 'KnownList': {'kind': 'K4', 'cppName': 'KnownList', 'partOf': [G + 'Npc.knownList'], 'fields': [
            {'name': 'owner', 'cpp': 'OwnerRef<Npc>', 'java': 'Npc', 'line': 2, 'rule': 'part owner'}]},
        G + 'Stat': {'kind': 'K5', 'cppName': 'Stat', 'fields': []},
        G + 'Template': {'kind': 'K1', 'cppName': 'Template', 'fields': []},
        G + 'Npc$1': {'kind': 'K4', 'cppName': 'Npc_ActionObserver', 'fields': [], 'captures': [
            {'name': 'player', 'kind': 'param', 'cpp': 'const Ref<Player>', 'type': G + 'Player'}]},
    },
    'cycleEdges': {G + 'Npc.target': {'scc': 1, 'resolution': None}, G + 'Npc.knownList': {'scc': 1, 'resolution': 'part'}},
    'staleResolutions': [G + 'Old.field'],
}


def lint(code, path='game-server/src/aion/gameserver/model/Test.h', fieldmap=FIELDMAP, rules=None, cycles=False, extra=()):
    linter = lc.Linter(fieldmap, rules, cycles)
    linter.add_source(code, path)
    for p, c in extra:
        linter.add_source(c, p)
    return linter.run()


def rules_of(findings, severity=None):
    return sorted({f.rule for f in findings if severity is None or f.severity == severity})


class ScannerTest(unittest.TestCase):
    def test_tokens_comments_preprocessor(self):
        src = lc.Source('x.h', '#define X(a) \\\n  a + 1 // macro\nint /* c */ y = R"(a ) " b)"; // tail\n')
        self.assertEqual(src.tok, ['int', 'y', '=', 'R"(a ) " b)"', ';'])
        self.assertEqual(src.line[0], 3)
        self.assertIn('tail', src.comments[3])

    def test_classes_members_functions(self):
        code = '''namespace aion::gameserver::model {
template <class T> class Box final : public RefCounted, private Base<T, int> {
	AION_MAKE_REF_FRIEND
public:
	int get() const { return v_; }
	std::function<void(int)> cb;
	static inline Field<int32_t> counter{0};
private:
	Field<int32_t> v_{1}, w_;
	struct Inner { int a; };
	enum class E : uint8_t { A, B };
};
void Box::run() noexcept {}
Model* model = nullptr;
}'''
        p = lc.Parser(lc.Source('x.h', code)).parse()
        box = next(c for c in p.classes if c.name == 'Box')
        self.assertEqual(box.namespace, ['aion', 'gameserver', 'model'])
        self.assertEqual(box.bases, ['RefCounted', 'Base<T, int>'])
        self.assertEqual([(m.name, m.type) for m in box.members], [('cb', 'std::function<void(int)>'), ('counter', 'Field<int32_t>'),
                                                                   ('v_', 'Field<int32_t>'), ('w_', 'Field<int32_t>')])
        self.assertIn('static', box.members[1].specifiers)
        self.assertEqual([f.name for f in p.functions], ['get', 'run'])
        self.assertEqual(p.functions[1].qual, ['Box'])
        self.assertEqual([c.qualname for c in p.classes], ['Box', 'Box::Inner'])
        self.assertEqual([g.name for g in p.globals], ['model'])

    def test_nested_class_defined_outside_its_class(self):
        code = 'namespace aion::gameserver::services {\nclass LifeStatsRestoreService::HpRestoreTask {\n\truntime::Ptr<X> lifeStats{};\n};\n}'
        p = lc.Parser(lc.Source('x.cpp', code)).parse()
        self.assertEqual([(c.name, c.qualname) for c in p.classes], [('HpRestoreTask', 'LifeStatsRestoreService::HpRestoreTask')])
        fm = {'format': 'aion-fieldmap', 'classes': {'com.aionemu.gameserver.services.LifeStatsRestoreService.HpRestoreTask': {
            'kind': 'K4', 'cppName': 'LifeStatsRestoreService::HpRestoreTask', 'base': 'RefCounted', 'fields': [
                {'name': 'lifeStats', 'cpp': 'Field<Ref<X>>', 'rule': 'non-final object reference'}]}}}
        f = lint(code, path='game-server/src/aion/gameserver/services/LifeStatsRestoreService.cpp', fieldmap=fm, rules=['L2', 'L3'])
        self.assertEqual(sorted(x.rule for x in f), ['L2', 'L3'])  # the fieldmap class is found through the class qualifier

    def test_constructor_init_list_and_lambdas(self):
        code = '''struct S { S(int a) : x(a), y{a} { auto f = [this, &a](int b) mutable -> int { return b; }; arr[0] = 1; } int x; int y; int arr[2]; };'''
        src = lc.Source('x.h', code)
        p = lc.Parser(src).parse()
        fn = p.functions[0]
        self.assertEqual(fn.name, 'S')
        lams = lc.find_lambdas(src, fn.body[0] + 1, fn.body[1])
        self.assertEqual(len(lams), 1)
        self.assertEqual(src.text_of(*lams[0].cap), 'this, &a')

    def test_norm_type(self):
        self.assertEqual(lc.norm_type('runtime::Field< runtime::Ref<model::Npc> >'), 'Field<Ref<Npc>>')
        self.assertEqual(lc.norm_type('Ref<Npc> const'), 'const Ref<Npc>')
        self.assertEqual(lc.norm_type('SpawnPoint const* const'), 'SpawnPoint const*const')
        self.assertEqual(lc.norm_type('std::int32_t'), 'int32_t')


class ClassRulesTest(unittest.TestCase):
    def test_l1_unwrapped_members(self):
        code = '''namespace aion::gameserver::model {
class Shared final : public RefCounted {
	int32_t hp;
	std::vector<Ref<Npc>> list;
	Ref<Npc> ref;
	Field<int32_t> ok;
	const int32_t id;
	ConcurrentHashMap<int32_t, Ref<Npc>> map;
	PartSlot<KnownList> part;
	const std::unique_ptr<KnownList> owned;
	Monitor lock;
	int32_t waived; // confined: only touched by the constructor thread
};
class Sub : public Shared { bool flag; };
class Value { int32_t plain; };
}'''
        f = lint(code, rules=['L1'])
        self.assertEqual([(x.line, x.rule) for x in f if x.severity == 'error'], [(3, 'L1'), (4, 'L1'), (5, 'L1'), (14, 'L1')])
        # the shim and the Monitor have no static lock class (RR-16 warning)
        self.assertEqual([(x.line, x.severity) for x in f if x.severity != 'error'], [(8, 'warning'), (11, 'warning')])

    def test_l1_lock_classes(self):
        code = '''namespace aion::gameserver::model {
class Shared final : public RefCounted {
	ConcurrentHashMap<int32_t, Ref<Npc>> tagged{AION_LOCK_CLASS(Shared::tagged#stripe)};
	mutable Monitor lock{AION_LOCK_CLASS(Shared::lock)};
	Semaphore permits{AION_LOCK_CLASS(Shared::permits), 1};
	AtomicBoolean flag = {AION_LOCK_CLASS(Shared::flag)};
	ArrayList<int32_t> inConstructor;
	static inline HashMap<int32_t, int32_t> cache;
	ArrayDeque<int32_t> untagged{};
	Field<Ref<RcArrayList<int32_t>>> created;
	Shared() : inConstructor(AION_LOCK_CLASS(Shared::inConstructor)) {}
};
}'''
        f = lint(code, rules=['L1'])
        self.assertEqual([(x.line, x.severity) for x in f], [(8, 'warning'), (9, 'warning')])
        self.assertIn('AION_LOCK_CLASS(JavaClass::untagged)', f[1].message)
        runtime = lint(code, path='game-server/src/aion/gameserver/runtime/x/T.h', rules=['L1'])
        self.assertEqual(runtime, [])

    def test_l1_lock_classes_outside_the_declaration(self):
        header = '''namespace aion::gameserver::model {
class ProbeA final : public RefCounted {
	runtime::Monitor lock;
	static runtime::ConcurrentHashMap<int32_t, int32_t> perWorld;
	static runtime::ArrayList<int32_t> untaggedStatic;
	ProbeA();
};
class ProbeB final : public RefCounted {
	runtime::Monitor lock;
	runtime::Monitor waived; // fieldmap: a layout waiver does not waive RR-16
};
}'''
        source = '''namespace aion::gameserver::model {
runtime::ConcurrentHashMap<int32_t, int32_t> ProbeA::perWorld{
	AION_LOCK_CLASS(ProbeA::perWorld#stripe)};
runtime::ArrayList<int32_t> ProbeA::untaggedStatic{};
ProbeA::ProbeA() : lock{AION_LOCK_CLASS(ProbeA::lock)} {}
}'''
        f = lint(header, rules=['L1'], extra=[('game-server/src/aion/gameserver/model/Probe.cpp', source)])
        # the constructor tag of ProbeA::lock does not cover ProbeB::lock; the out-of-line definition tags ProbeA::perWorld only
        self.assertEqual([(x.line, x.message.split(' ')[2]) for x in f], [(5, 'ProbeA::untaggedStatic'), (9, 'ProbeB::lock'), (10, 'ProbeB::waived')])
        inline = header.replace('\tProbeA();', '\tProbeA() : lock(AION_LOCK_CLASS(ProbeA::lock)) {}')
        self.assertEqual([x.line for x in lint(inline, rules=['L1'])], [4, 5, 9, 10])

    def test_l1_l19_accept_fieldmap_decisions(self):
        fm = {'format': 'aion-fieldmap', 'classes': {
            'com.aionemu.gameserver.network.Conn': {'kind': 'K4', 'cppName': 'Conn', 'fields': [
                {'name': 'queue', 'cpp': 'std::deque<SerializedBody>', 'rule': 'fieldmap.toml override'},
                {'name': 'crypt', 'cpp': 'Crypt', 'rule': 'member of a class confined by fieldmap.toml'},
                {'name': 'other', 'cpp': 'Field<int32_t>', 'rule': 'non-final scalar'}]},
            'com.aionemu.gameserver.network.Crypt': {'kind': 'K5', 'cppName': 'Crypt', 'reason': 'fieldmap.toml [kinds]', 'fields': []},
            'com.aionemu.gameserver.geo.Vector3f': {'kind': 'K5', 'cppName': 'Vector3f', 'reason': 'value type (fieldmap.toml settings.value_types)',
                                                    'fields': []},
            'com.aionemu.gameserver.network.Service': {'kind': 'K5', 'cppName': 'Service', 'fields': []}}}
        code = '''namespace aion::gameserver::network {
class Conn : public RefCounted {
	std::deque<SerializedBody> queue;
	Crypt crypt;
	std::deque<int> other;
	Field<Vector3f> position;
	ArrayList<Ref<Service::Identifiers>> ids{AION_LOCK_CLASS(Conn::ids)};
};
}'''
        f = lint(code, fieldmap=fm, rules=['L1', 'L19'])
        self.assertEqual([(x.line, x.rule) for x in f], [(5, 'L1')])

    def test_l2_fieldmap_comparison(self):
        code = '''namespace aion::gameserver::model {
class Npc : public VisibleObject {
	runtime::Field<int32_t> hp;
	Field<Ref<VisibleObject>> target;
	const std::unique_ptr<KnownList> knownList;
	static constexpr int32_t MAX = 3;
};
struct Npc_ActionObserver final : ActionObserver { const Ref<Player> player; };
class Template { int32_t id; };
}'''
        f = [x for x in lint(code, rules=['L2'])]
        msgs = [(x.line, x.message.split(' ')[0]) for x in f]
        self.assertIn((4, 'Npc::target'), msgs)
        self.assertIn((2, 'Npc'), msgs)  # lacks member missing
        self.assertEqual(len(f), 2)
        code2 = code.replace('Field<Ref<VisibleObject>> target;', 'Field<Ref<VisibleObject>> target; // fieldmap: widened for the port')
        self.assertEqual(len(lint(code2, rules=['L2'])), 1)

    def test_l2_cpp_only_members(self):
        fm = {'format': 'aion-fieldmap', 'classes': {G + 'Player': {'kind': 'K4', 'cppName': 'Player', 'fields': [
            {'name': 'hp', 'cpp': 'Field<int32_t>', 'rule': 'non-final scalar'}], 'cppMembers': [
            {'name': 'proxySlot', 'cpp': 'PartSlot<Proxy, RetireTo::RECLAIMER>', 'reason': 'x', 'partType': G + 'Proxy'},
            {'name': 'monitor', 'cpp': 'Monitor', 'reason': 'x'},
            {'name': 'declaredOnly', 'cpp': 'const int32_t', 'reason': 'x'}]}}}
        code = '''namespace aion::gameserver::model {
class Player final : public RefCounted {
	runtime::Field<int32_t> hp;
	runtime::PartSlot<Proxy, runtime::RetireTo::RECLAIMER> proxySlot{*this};
	mutable runtime::Monitor monitor_{AION_LOCK_CLASS(Player::monitor)};
	const int32_t copiedScalar;
	runtime::Field<runtime::Ref<Kisk>> undeclared; // fieldmap: a waiver cannot hide a retaining member
	static runtime::Field<runtime::Ref<Kisk>> staticOne;
	const std::unique_ptr<Part> ownedPart; // lint: L2 not waivable either
};
}'''
        f = lint(code, fieldmap=fm, rules=['L2'])
        self.assertEqual([(x.line, x.message.split(' ')[0] if x.line != 2 else x.message) for x in f],
                         [(2, 'Player lacks the C++-only member declaredOnly `const int32_t` of fieldmap.toml [cpp_members]'), (7, 'C++-only'), (9, 'C++-only')])
        wrong = code.replace('runtime::PartSlot<Proxy, runtime::RetireTo::RECLAIMER> proxySlot', 'runtime::PartSlot<Proxy> proxySlot')
        self.assertIn((4, 'L2'), [(x.line, x.rule) for x in lint(wrong, fieldmap=fm, rules=['L2'])])

    def test_l2_holders_nested_names_and_loggers(self):
        fm = {'format': 'aion-fieldmap', 'classes': {
            'com.aionemu.gameserver.dataholders.DataManager': {'kind': 'K4', 'cppName': 'DataManager', 'base': 'Immortal', 'fields': [
                {'name': 'ITEM_DATA', 'cpp': 'static inline Field<const ItemData*>', 'modifiers': ['public', 'static'], 'rule': 'static non-final template reference'},
                {'name': 'SPAWNS_DATA', 'cpp': 'static inline Field<const SpawnsData*>', 'modifiers': ['public', 'static'], 'rule': 'static non-final template reference'},
                {'name': 'NPC_DATA', 'cpp': 'static inline Field<const NpcData*>', 'modifiers': ['public', 'static'], 'rule': 'static non-final template reference'}]},
            'com.aionemu.gameserver.model.Passport': {'kind': 'K4', 'cppName': 'Passport', 'fields': [
                {'name': 'state', 'cpp': 'Field<Persistable::PersistentState>', 'rule': 'non-final scalar'},
                {'name': 'force', 'cpp': 'Field<const Effect_ForceType*>', 'rule': 'non-final immortal reference'},
                {'name': 'log', 'cpp': 'const Logger', 'rule': 'logger'}]}}}
        code = '''namespace aion::gameserver::dataholders {
class DataManager final : public runtime::Immortal {
public:
	static inline xml::HolderRef<ItemData> ITEM_DATA;
	static inline xml::MutableHolderRef<SpawnsData> SPAWNS_DATA;
	static inline xml::HolderRef<ItemData> NPC_DATA;
};
}
namespace aion::gameserver::model {
class Passport : public RefCounted {
	runtime::Field<PersistentState> state;
	runtime::Field<const Effect::ForceType*> force;
};
}'''
        f = lint(code, fieldmap=fm, rules=['L2', 'L4'])
        self.assertEqual([(x.line, x.rule, x.message.split(' ')[0]) for x in f], [(6, 'L2', 'DataManager::NPC_DATA'), (12, 'L2', 'Passport::force')])

    def test_l2_explicit_mapping_comment(self):
        code = '''namespace other {
// fieldmap-class: com.aionemu.gameserver.model.KnownList
class KL : public OwnedPart { Npc& owner; };
}'''
        f = lint(code, rules=['L2'])
        self.assertEqual([x.rule for x in f], ['L2'])

    def test_l3_borrows_in_shared(self):
        code = '''namespace aion::gameserver::x {
class Player final : public RefCounted {};
class MapRegion final : public OwnedPart {};
class S final : public RefCounted {
	std::string_view name;
	Ptr<Player> borrowed;
	Player* raw;
	Player& refd;
	const std::string& str;
	Field<MapRegion*> sibling;
	MapRegion* part;
	OwnerRef<Player> owner;
};
class Part final : public OwnedPart { OwnerRef<Player> owner; };
}'''
        f = lint(code, rules=['L3'])
        # OwnerRef is a plain reference: only parts hold one (line 12 is in a RefCounted class)
        self.assertEqual([x.line for x in f], [5, 6, 7, 8, 9, 12])
        waived = code.replace('OwnerRef<Player> owner;\n};', 'OwnerRef<Player> owner; // fieldmap: not waivable\n};')
        self.assertNotEqual(waived, code)
        self.assertEqual([x.line for x in lint(waived, rules=['L3'])], [5, 6, 7, 8, 9, 12])
        fm = {'format': 'aion-fieldmap', 'classes': {'com.aionemu.gameserver.x.S': {'kind': 'K4', 'cppName': 'S', 'base': 'RefCounted', 'fields': [
            {'name': 'owner', 'cpp': 'OwnerRef<Player>', 'rule': 'fieldmap.toml override'}]}}}
        self.assertEqual([x.line for x in lint(code, fieldmap=fm, rules=['L3'])], [5, 6, 7, 8, 9])

    def test_l3_back_reference_holders_and_accessor(self):
        fm = {'format': 'aion-fieldmap', 'classes': {
            'com.aionemu.gameserver.x.Item': {'kind': 'K4', 'cppName': 'Item', 'base': 'RefCounted', 'fields': [
                {'name': 'conditioningInfo', 'cpp': 'Field<Ref<ChargeInfo>>', 'rule': 'non-final object reference'}]},
            'com.aionemu.gameserver.x.ChargeInfo': {'kind': 'K4', 'cppName': 'ChargeInfo', 'fields': [
                {'name': 'item', 'cpp': 'OwnerRef<Item>', 'rule': 'fieldmap.toml override', 'holders': ['com.aionemu.gameserver.x.Item.conditioningInfo'],
                 'accessor': 'getItem'}]}}}
        code = '''namespace aion::gameserver::x {
class Item final : public RefCounted { Field<Ref<ChargeInfo>> conditioningInfo{}; };
class ChargeInfo final : public ActionObserver { OwnerRef<Item> item; Item& getItem() const; };
class Holder final : public RefCounted {
	Field<Ref<ChargeInfo>> copy{}; // lint: L3 not waivable
	ArrayList<Ref<model::items::ChargeInfo>> list{AION_LOCK_CLASS(Holder::list)};
	Field<Ref<ChargeInfoTemplate>> other{};
};
ChargeInfo::ChargeInfo(Item& itemValue) : item(itemValue) {}
Item& ChargeInfo::getItem() const { AION_CHECK("C4", item.isManaged(), "gone"); return item; }
bool ChargeInfo::update(int32_t points) {
	return item.isEquipped() && getItem().isEquipped();
}
void ChargeInfo::shadowed(Item& item) { item.touch(); }
void Other::f(Ref<ChargeInfo> info, Player& player) {
	ThreadPoolManager::getInstance().schedule({info}, [info] { info->attack(); }, 10);
	ThreadPoolManager::getInstance().schedule(&player, [&player] { go(player); }, 10);
}
}'''
        f = lint(code, fieldmap=fm, rules=['L3'])
        self.assertEqual([x.line for x in f], [5, 6, 12, 16])
        self.assertIn('only Item::conditioningInfo may hold it', f[0].message)
        self.assertIn('use getItem()', f[2].message)
        self.assertIn('is captured or pinned', f[3].message)
        self.assertEqual(lint(code, rules=['L3']), [])  # without the fieldmap decision nothing is restricted

    def test_l3_l10_immortal_names_by_identity(self):
        fm = {'format': 'aion-fieldmap', 'classes': {
            'com.aionemu.gameserver.model.templates.goods.GoodsList.Item': {'kind': 'K1', 'cppName': 'GoodsList::Item', 'base': 'StaticTemplate',
                                                                             'fields': []},
            'com.aionemu.gameserver.model.gameobjects.Item': {'kind': 'K4', 'cppName': 'Item', 'base': 'RefCounted', 'fields': []},
            'com.aionemu.gameserver.world.zone.ZoneName': {'kind': 'K4', 'cppName': 'ZoneName', 'base': 'Immortal', 'immortal': True, 'fields': []},
            'com.aionemu.gameserver.model.templates.QuestNpc': {'kind': 'K1', 'cppName': 'QuestNpc', 'base': 'StaticTemplate', 'fields': []},
            'com.aionemu.gameserver.questEngine.model.QuestNpc': {'kind': 'K4', 'cppName': 'QuestNpc', 'base': 'RefCounted', 'fields': []}}}
        code = '''namespace aion::gameserver::model {
class CreatureTemplate : public runtime::StaticTemplate {};
class PlayerCommonData final : public runtime::RefCounted, public CreatureTemplate {};
class NpcTemplate final : public CreatureTemplate {};
class Task final : public runtime::RefCounted {
	Item& itemRef;
	PlayerCommonData& pcdRef;
	QuestNpc& qnpc;
	const ZoneName* zone;
	const NpcTemplate& npcTemplate;
};
class SM_PROBE final : public AionServerPacket {
	Item* item;
	PlayerCommonData* pcd;
	const ZoneName* zone;
};
}'''
        f = lint(code, fieldmap=fm, rules=['L3', 'L10'])
        self.assertEqual([(x.line, x.rule) for x in f], [(6, 'L3'), (7, 'L3'), (8, 'L3'), (13, 'L10'), (14, 'L10')])

    def test_l3_immortals_and_templates_are_no_borrows(self):
        fm = {'format': 'aion-fieldmap', 'classes': {
            'com.aionemu.gameserver.world.zone.ZoneName': {'kind': 'K4', 'cppName': 'ZoneName', 'base': 'Immortal', 'fields': []},
            'com.aionemu.gameserver.questEngine.QuestEngine': {'kind': 'K4', 'cppName': 'QuestEngine', 'base': 'Immortal', 'singleton': True, 'fields': []},
            'com.aionemu.gameserver.model.Player': {'kind': 'K4', 'cppName': 'Player', 'fields': []}}}
        code = '''namespace aion::gameserver::x {
class AbstractQuestHandler : public runtime::Immortal {};
class S final : public RefCounted {
	HashMap<const zone::ZoneName*, Ref<RcArrayList<int32_t>>> zones{AION_LOCK_CLASS(S::zones)};
	HashMap<int32_t, handlers::AbstractQuestHandler*> handlers{AION_LOCK_CLASS(S::handlers)};
	QuestEngine& qe;
	const QuestEngine& constQe;
	Player* player;
	Player& playerRef;
};
}'''
        f = lint(code, fieldmap=fm, rules=['L3'])
        self.assertEqual([x.line for x in f], [8, 9])

    def test_l4_static_and_global_state(self):
        code = '''namespace aion::gameserver::x {
int counter = 0;
constexpr int LIMIT = 3;
const std::string NAME = "a";
std::atomic<int> atomicCounter{0};
std::array<std::atomic<const int*>, 4> table;
Model* model = nullptr;
class C { static inline std::vector<int> cache; static inline Field<int32_t> ok{0}; static constexpr int K = 1; };
}'''
        f = lint(code, path='game-server/src/aion/gameserver/runtime/x/T.cpp', rules=['L4'])
        self.assertEqual([x.line for x in f], [2, 7, 8])

    def test_l4_opaque_enum_declarations_are_not_variables(self):
        code = '''namespace aion::gameserver::x {
enum class WorldMapType : std::uint8_t;
enum class Race : uint16_t;
enum Plain : int;
class Forward;
int counter = 0;
}'''
        f = lint(code, path='game-server/src/aion/gameserver/x/fwd.h', rules=['L4'])
        self.assertEqual([x.line for x in f], [6])

    def test_l10_packets(self):
        code = '''namespace aion::gameserver::network::aion::serverpackets {
class SM_PLAYER_INFO final : public AionServerPacket { Ref<Player> player; };
class SM_MOVE final : public AionServerPacket {
	Ptr<Npc> npc;
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }
	void writeImpl(AionConnection* con) override { sendPacket(*npc, SM_MOVE()); }
};
}'''
        f = lint(code, rules=['L10'])
        self.assertEqual([x.line for x in f], [2, 3, 4, 6])

    def test_l13_immortal(self):
        code = '''namespace aion::gameserver::services {
class GoodService final : public Immortal { public: static GoodService& getInstance(); };
class Siege final : public Immortal {};
class Random final : public Immortal {};
class Q : public QuestHandler, public Immortal {};
}'''
        f = lint(code, rules=['L13'])
        self.assertEqual([x.line for x in f], [3, 4])
        # fieldmap.toml [immortal] (interned classes such as ZoneName) marks the entry 'immortal'
        interned = {'format': 'aion-fieldmap', 'classes': {G + 'ZoneName': {'kind': 'K4', 'cppName': 'ZoneName', 'immortal': True, 'fields': []},
                                                            G + 'Other': {'kind': 'K4', 'cppName': 'Other', 'fields': []}}}
        code = '''namespace aion::gameserver::model {
class ZoneName final : public Immortal {};
class Other final : public Immortal {};
}'''
        self.assertEqual([x.line for x in lint(code, fieldmap=interned, rules=['L13'])], [3])
        # the handler and command root classes themselves (IMMORTAL_BASES)
        code = '''namespace aion::gameserver::questEngine::handlers {
class AbstractQuestHandler : public runtime::Immortal {};
class ChatCommand : public runtime::Immortal {};
class NotARoot : public runtime::Immortal {};
}'''
        self.assertEqual([x.line for x in lint(code, rules=['L13'])], [4])

    def test_l15_thread_local(self):
        code = '''namespace aion::gameserver::x {
class Player final : public RefCounted {};
thread_local Ref<Player> current;
thread_local std::vector<uint8_t> scratch;
thread_local Player* raw = nullptr;
thread_local Schedule* kernel = nullptr;
}'''
        f = lint(code, rules=['L15'])
        self.assertEqual([x.line for x in f], [3, 5])

    def test_l16_cycles_and_part_owner(self):
        code = '''namespace aion::gameserver::model {
class KnownList : public OwnedPart { Field<Ref<Npc>> owner; };
}'''
        f = lint(code, rules=['L16'], cycles=True)
        self.assertEqual(sorted((x.severity, x.message.split(' ')[0]) for x in f),
                         [('error', 'part'), ('error', 'unresolved'), ('warning', 'stale')])

    def test_l16_core_scope_skips_handler_edges(self):
        fm = json.loads(json.dumps(FIELDMAP))
        fm['classes'][G + 'Npc']['file'] = 'game-server/src/com/aionemu/gameserver/model/Npc.java'
        fm['classes']['ai.HandlerAI'] = {'kind': 'K4', 'cppName': 'HandlerAI', 'file': 'game-server/data/handlers/ai/HandlerAI.java', 'fields': []}
        fm['callbacks'] = {'ai.HandlerAI@L3:4': {'file': 'game-server/data/handlers/ai/HandlerAI.java'}}
        fm['cycleEdges'] = {G + 'Npc.target': {'from': G + 'Npc', 'scc': 1, 'resolution': None},
                            'ai.HandlerAI.owner': {'from': 'ai.HandlerAI', 'scc': 1, 'resolution': None},
                            'ai.HandlerAI@L3:4#this': {'from': 'cb:ai.HandlerAI@L3:4', 'scc': 1, 'resolution': None}}
        edges = lambda scope: sorted(x.message.split(' ')[3] for x in lint('', fieldmap=fm, rules=['L16'], cycles=scope) if x.severity == 'error')
        self.assertEqual(edges(True), ['ai.HandlerAI.owner', 'ai.HandlerAI@L3:4#this', G + 'Npc.target'])
        self.assertEqual(edges('all'), edges(True))
        self.assertEqual(edges('core'), [G + 'Npc.target'])
        with self.assertRaises(lc.LintError):
            lc.Linter(fm, None, 'handlers')

    def test_l19_confined_in_shared(self):
        code = '''namespace aion::gameserver::model {
class Holder final : public RefCounted { Field<Ref<Stat>> stat; };
void f(Stat stat) { ThreadPoolManager::getInstance().schedule(this, [this, stat] {}, 1); }
}'''
        f = lint(code, rules=['L19'])
        self.assertEqual([x.line for x in f], [2, 3])


class FunctionRulesTest(unittest.TestCase):
    def test_l5_captures(self):
        code = '''namespace aion::gameserver::x {
void Npc::f(Player& player, Ptr<Npc> npc, int32_t count) {
	Ref<Player> ref = player;
	ThreadPoolManager::getInstance().schedule(this, [this] { go(); }, 10);
	ThreadPoolManager::getInstance().schedule({this, &player}, [this, &player] { go(); }, 10);
	ThreadPoolManager::getInstance().schedule(this, [&] { go(); }, 10);
	ThreadPoolManager::getInstance().schedule([this] { go(); }, 10);
	ThreadPoolManager::getInstance().schedule([] { tick(); }, 10);
	ThreadPoolManager::getInstance().schedule(this, [this, &player] { go(); }, 10);
	ThreadPoolManager::getInstance().schedule(this, [this, npc] { go(); }, 10);
	ThreadPoolManager::getInstance().schedule(this, [this, count, ref] { go(); }, 10);
	ThreadPoolManager::getInstance().schedule(&player, [&player] { go(); }, 10);
	player.getObserveController().addObserver(PinnedCallback<void()>(this, [this, n = &count] {}));
	std::for_each(v.begin(), v.end(), [&](int x) { use(x); });
}
}'''
        f = lint(code, rules=['L5'])
        self.assertEqual([x.line for x in f], [6, 7, 9, 10, 13])

    def test_l5_player_templates_are_not_captured(self):
        code = '''namespace aion::gameserver::x {
void Service::f(Player& player, Npc& npc, Creature& creature) {
	auto playerTemplate = player.getObjectTemplate();
	auto npcTemplate = npc.getObjectTemplate();
	ThreadPoolManager::getInstance().schedule(&npc, [&npc, playerTemplate] { use(playerTemplate); }, 10);
	ThreadPoolManager::getInstance().schedule(&npc, [&npc, npcTemplate] { use(npcTemplate); }, 10);
	ThreadPoolManager::getInstance().schedule(&npc, [&npc, t = creature.getObjectTemplate()] { use(t); }, 10);
	ThreadPoolManager::getInstance().schedule(bindTask(&use, player.getObjectTemplate()), 10);
	ThreadPoolManager::getInstance().schedule({&npc, creature.getObjectTemplate()}, [&npc] { go(); }, 10);
	ThreadPoolManager::getInstance().schedule(bindTask(&use, npc.getObjectTemplate()), 10);
}
void Player::g() {
	auto own = this->getObjectTemplate();
	ThreadPoolManager::getInstance().schedule(this, [this, own] { use(own); }, 10);
}
}'''
        f = lint(code, rules=['L5'])
        self.assertEqual([x.line for x in f], [3, 7, 8, 9, 13])
        self.assertIn('capture `playerTemplate`: getObjectTemplate() of a Player', f[0].message)
        # a template id derived from the template is fine; VisibleObject receivers and call chains are checked
        code = '''namespace aion::gameserver::x {
class PetCommonData final : public RefCounted { public: Player& getOwner() const; };
class Spawn final : public RefCounted { public: const SpawnTemplate& getSpot() const; };
void Service::f(Player& player, VisibleObject& vo, PetCommonData& pc, Spawn& spawn) {
	ThreadPoolManager::getInstance().schedule(this, [this, id = player.getObjectTemplate()->getTemplateId()] { use(id); }, 10);
	int32_t tid = player.getObjectTemplate()->getTemplateId();
	ThreadPoolManager::getInstance().schedule(this, [this, tid] { use(tid); }, 10);
	ThreadPoolManager::getInstance().schedule(this, [this, t = vo.getObjectTemplate()] { use(t); }, 10);
	ThreadPoolManager::getInstance().schedule(this, [this, t = pc.getOwner().getObjectTemplate()] { use(t); }, 10);
	ThreadPoolManager::getInstance().schedule(this, [this, t = spawn.getSpot().getObjectTemplate()] { use(t); }, 10);
	ThreadPoolManager::getInstance().schedule(this, [this, t = unknown().getObjectTemplate()] { use(t); }, 10);
}
}'''
        f = lint(code, rules=['L5'])
        self.assertEqual([x.line for x in f], [8, 9, 11])
        self.assertIn('getObjectTemplate() of a Player (getOwner())', f[1].message)

    def test_l5b_quiescent(self):
        code = '''namespace aion::gameserver::x {
void PeriodicSaveService::onShutdown(Player& admin) {
	Ptr<Player> borrowed = admin.self();
	runtime::QuiescentScope quiescent;
	for (Ptr<Player> p : players) { runtime::quiescentPoint(); save(*p); }
}
void Service::nested() {
	if (x) { runtime::QuiescentScope quiescent; runtime::quiescentPoint(); }
}
void Service::stray() { runtime::quiescentPoint(); }
void Service::ok(Ref<Player> admin) {
	// quiescent-safe: admin is a captured Ref
	runtime::QuiescentScope quiescent;
	for (Ref<Player> p : snapshot) runtime::quiescentPoint();
}
}'''
        f = lint(code, rules=['L5'])
        self.assertEqual([(x.line, x.severity) for x in f], [(2, 'error'), (3, 'error'), (5, 'error'), (8, 'error'), (10, 'warning')])

    def test_l6_threads(self):
        code = 'void f() { std::thread t([] {}); t.detach(); auto n = std::thread::hardware_concurrency(); auto a = std::async([] {}); }'
        self.assertEqual([x.col for x in lint(code, rules=['L6'])], [code.find('thread') + 1, code.find('detach') + 1, code.find('async') + 1])
        self.assertEqual(lint(code, path='game-server/src/aion/gameserver/runtime/a.cpp', rules=['L6']), [])

    def test_l7_sync_parity(self):
        code = '''namespace aion::gameserver::model {
class Npc : public VisibleObject { void move() { SYNCHRONIZED(*this) { hp = 1; } } void idle(); };
void Npc::idle() { SYNCHRONIZED(*this) {} }
}'''
        f = lint(code, rules=['L7'])
        self.assertEqual([(x.line, x.message.split(': ')[0]) for x in f], [(3, 'Npc::idle')])
        # an unported stub is not compared (the port adds the lock); a stub with more code is
        stub = code.replace('void Npc::idle() { SYNCHRONIZED(*this) {} }', 'void Npc::idle() {\n\tAION_UNPORTED();\n}\nvoid Npc::move() { AION_UNPORTED(); hp = 2; }')
        stub = stub.replace('void move() { SYNCHRONIZED(*this) { hp = 1; } } void idle();', 'void idle();')
        self.assertEqual([(x.line, x.message.split(': ')[0]) for x in lint(stub, rules=['L7'])], [(6, 'Npc::move')])

    def test_l8_banned_and_statics(self):
        code = '''void f() {
	char* p = strtok(s, ",");
	auto t = std::localtime(&now);
	int r = rand();
	static int counter = 0;
	static const auto* logger = new Logger("x");
	static auto* state = new State();
	static constexpr int K = 3;
	static std::atomic<int> hits{0};
	obj.ctime();
}
Service& Service::getInstance() { static Service instance; return instance; }'''
        f = lint(code, rules=['L8'])
        self.assertEqual([x.line for x in f], [2, 3, 4, 5])

    def test_l9_destructor(self):
        code = '''namespace aion::gameserver::x {
class Item final : public RefCounted { ~Item() override; Field<int64_t> time; Field<Ref<Npc>> npc; };
Item::~Item() {
	int64_t t = time.get();
	npc->release();
	SYNCHRONIZED(*this) {}
	World::getInstance().remove(this);
	CleanerQueue::push(id);
}
class Plain { ~Plain() { other->close(); } };
}'''
        f = lint(code, rules=['L9'])
        self.assertEqual([x.line for x in f], [5, 6, 7, 7])

    def test_l11_iterators(self):
        code = '''void f(Player& p) {
	std::vector<int> local;
	auto a = local.begin();
	auto b = p.getItems().begin();
	auto it = list.iterator();
}'''
        self.assertEqual([x.line for x in lint(code, rules=['L11'])], [4])

    def test_l12_blocking_under_monitor(self):
        code = '''void f() {
	SYNCHRONIZED(*this) { PlayerDAO::storePlayer(p); task->get(); }
	SYNCHRONIZED(*this) { PlayerDAO::storePlayer(p); } // lockdep: startup only
	map.compute(1, [&](Ptr<Future> old) { LegionDAO::load(1); return old; });
	PlayerDAO::storePlayer(p);
}'''
        f = lint(code, rules=['L12'])
        self.assertEqual([(x.line, x.severity) for x in f], [(2, 'warning'), (2, 'warning'), (4, 'warning')])

    def test_l14_atomic_warning(self):
        code = 'namespace aion::gameserver::x { void f() { std::atomic<int> a{0}; } }'
        self.assertEqual(rules_of(lint(code, rules=['L14']), 'warning'), ['L14'])
        self.assertEqual(lint(code, path='game-server/src/aion/gameserver/runtime/a.cpp', rules=['L14']), [])

    def test_l17_knownlist(self):
        code = '''bool KnownList::addPair(VisibleObject& a, VisibleObject& b) { return b.getKnownList().add(a) && a.getKnownList().add(b); }
void Other::f(VisibleObject& a, VisibleObject& b) { a.getKnownList().add(b); knownList->add(b); list.add(b); }'''
        self.assertEqual([x.line for x in lint(code, rules=['L17'])], [2, 2])

    def test_l18_ranked_mutex(self):
        code = '''namespace aion::gameserver::runtime {
class Heap { RankedMutex<LockRank::SCHEDULER> mutex_; void each(std::function<void(int)> fn); void safe(std::function<void(int)> fn); };
void Heap::each(std::function<void(int)> fn) { std::scoped_lock lock(mutex_); fn(1); }
void Heap::safe(std::function<void(int)> fn) { { std::scoped_lock lock(mutex_); } fn(1); }
}'''
        f = lint(code, path='game-server/src/aion/gameserver/runtime/Heap.cpp', rules=['L18'])
        self.assertEqual([x.line for x in f], [3])
        f = lint('class A { LeafMutex m{LockRank::STATS}; };', path='game-server/src/aion/gameserver/services/A.h', rules=['L18'])
        self.assertEqual([x.rule for x in f], ['L18'])

    def test_l18_blocking_under_leaf_mutex(self):
        code = '''namespace aion::gameserver::runtime {
class Q { LeafMutex mutex_{LockRank::CONNECTION_QUEUE}; FutureRef task; void f(); };
void Q::f() {
	{
		std::scoped_lock lock(mutex_);
		task->get();
		SYNCHRONIZED(*this) {}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	task->get();
}
}'''
        f = lint(code, path='game-server/src/aion/gameserver/runtime/Q.cpp', rules=['L18'])
        self.assertEqual([x.line for x in f], [6, 7, 8])

    def test_l20_nested_same_map_writes(self):
        code = '''void PlayerContainer::updateCachedPlayerName(std::string_view oldName, Player& player) {
	playersByName.compute(std::string(oldName), [&](Ptr<Player> old) {
		playersByName.put(player.getName(), Ref<Player>(player));
		otherMap.put(1, 2);
		return nullptr;
	});
	this->tasks.computeIfAbsent(1, [this](int32_t) { tasks.remove(2); return FutureRef(); });
}'''
        f = lint(code, rules=['L20'])
        self.assertEqual([x.line for x in f], [3, 7])

    def test_waiver_without_reason(self):
        code = 'namespace aion::gameserver::x { class S : public RefCounted { int32_t v; // confined:\n}; }'
        f = lint(code, rules=['L1'])
        self.assertEqual(sorted(x.rule for x in f), ['L1', 'W0'])
        code = 'void f() { int r = rand(); // lint: L8 seeded test helper\n}'
        self.assertEqual(lint(code, rules=['L8']), [])


class CliTest(unittest.TestCase):
    def test_cli(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, 'model'))
            with open(os.path.join(tmp, 'model', 'A.h'), 'w', encoding='utf-8') as f:
                f.write('namespace aion::gameserver::model { class A : public RefCounted { int32_t v; }; }\n')
            with open(os.path.join(tmp, 'fm.json'), 'w', encoding='utf-8') as f:
                json.dump(FIELDMAP, f)
            out = io.StringIO()
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(lc.main(['--fieldmap', os.path.join(tmp, 'fm.json'), tmp]), 1)
            self.assertIn(': error: L1: ', out.getvalue())
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(lc.main(['--no-fieldmap', '--rules', 'L6', tmp]), 0)
                self.assertEqual(lc.main(['--no-fieldmap', '--rules', 'L99', tmp]), 2)
                self.assertEqual(lc.main(['--no-fieldmap', os.path.join(tmp, 'missing')]), 2)
            out = io.StringIO()
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
                lc.main(['--no-fieldmap', '--json', tmp])
            self.assertEqual(json.loads(out.getvalue())[0]['rule'], 'L1')
            with open(os.path.join(tmp, 'model', 'A.h'), 'w', encoding='utf-8') as f:
                f.write('namespace aion::gameserver::model { class A : public RefCounted { Field<int32_t> v; Monitor lock; }; }\n')
            out = io.StringIO()
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(lc.main(['--no-fieldmap', tmp]), 0)  # a warning fails only with --werror
                self.assertIn(': warning: L1: lockable member A::lock', out.getvalue())
                self.assertEqual(lc.main(['--no-fieldmap', '--werror', tmp]), 1)
                self.assertEqual(lc.main(['--no-fieldmap', '--werror', '--strict-lock-classes', tmp]), 1)
                self.assertEqual(lc.main(['--no-fieldmap', '--cycles', tmp]), 0)
                self.assertEqual(lc.main(['--no-fieldmap', '--cycles=core', tmp]), 0)
            lc.LOCK_CLASS_SEVERITY = 'warning'


if __name__ == '__main__':
    unittest.main()
