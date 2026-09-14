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
        self.assertEqual([(x.line, x.rule) for x in f], [(3, 'L1'), (4, 'L1'), (5, 'L1'), (14, 'L1')])

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
}'''
        f = lint(code, rules=['L3'])
        self.assertEqual([x.line for x in f], [5, 6, 7, 8, 9])

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


if __name__ == '__main__':
    unittest.main()
