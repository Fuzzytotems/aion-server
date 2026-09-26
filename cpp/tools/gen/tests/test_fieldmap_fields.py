"""fieldmap.py: the §3.2 field table, effectively final analysis and part detection (patterns 1-4, owner/sibling fields)."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import fieldmap  # noqa: E402

CONFIG = '''
[settings]
connection_bases = ["com.x.net.Connection"]
[fields]
"com.x.world.Creature.ai" = { cpp = "PartSlot<AI>", part = true, retire = "OWNER", reason = "created by a factory with this" }
'''

FILES = {
    'com/x/net/Connection.java': '''package com.x.net;
public abstract class Connection {}''',
    'com/x/net/GameConnection.java': '''package com.x.net;
public class GameConnection extends Connection {}''',
    'com/x/data/Template.java': '''package com.x.data;
import javax.xml.bind.annotation.*;
@XmlRootElement public class Template { @XmlAttribute int id; }''',
    'com/x/Registry.java': '''package com.x;
import java.util.*;
import com.x.world.*;
public class Registry {
    private static final Registry instance = new Registry();
    private static final List<Creature> creatures = new ArrayList<>();
    private static final List<Holder> holders = new ArrayList<>();
    private static Player lastPlayer;
    public static void remember(Player p) { lastPlayer = p; }
    public static final int MAX = 10;
    public static final String NAME = "reg";
    private static final org.slf4j.Logger log = org.slf4j.LoggerFactory.getLogger(Registry.class);
    private static final ThreadLocal<Integer> depth = new ThreadLocal<>();
}''',
    'com/x/configs/GSConfig.java': '''package com.x.configs;
import com.aionemu.commons.configuration.Property;
public class GSConfig {
    @Property(key = "a", defaultValue = "1") public static int LEVEL;
    @Property(key = "b", defaultValue = "x") public static String NAME;
}''',
    'com/x/world/Holder.java': '''package com.x.world;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import java.util.concurrent.locks.*;
import com.x.data.Template;
import com.x.net.GameConnection;
public class Holder {
    private final int id;
    private int hp;
    private volatile boolean dead;
    private Integer nullable;
    private final String name;
    private String title;
    private Race race;
    private final Creature owner;
    private Creature target;
    private final Template template;
    private Template lastTemplate;
    private final Map<Integer, Creature> byId = new ConcurrentHashMap<>();
    private final List<Creature> list;
    private final Set<Integer> ids = ConcurrentHashMap.newKeySet();
    private List<Creature> replaced = new ArrayList<>();
    private final int[] counters = new int[4];
    private Future<?>[] tasks;
    private final AtomicInteger count = new AtomicInteger();
    private final AtomicReference<Creature> ref = new AtomicReference<>();
    private Future<?> task;
    private final ReentrantLock lock = new ReentrantLock();
    private final Object mutex = new Object();
    private GameConnection connection;
    private int effectively;
    private int writtenLater;
    private int writtenInLambda;
    private final java.util.function.Consumer<Creature> callback = c -> {};
    private java.sql.Timestamp stamp;
    public Holder(int id, String name, Creature owner, Template template) {
        this.id = id; this.name = name; this.owner = owner; this.template = template;
        this.list = new CopyOnWriteArrayList<>();
        effectively = 3;
        Runnable r = () -> writtenInLambda = 1;
    }
    public void set() { writtenLater = 2; hp = 1; target = null; }
}''',
    'com/x/world/Race.java': '''package com.x.world;
public enum Race { A, B }''',
    'com/x/world/Creature.java': '''package com.x.world;
public abstract class Creature {
    private final Controller controller;
    private final KnownList knownList;
    private Stats stats;
    private final AI ai;
    private final Equipment equipment = new Equipment(this);
    private MoveController moveController;
    private final Storage[] bags = new Storage[2];
    protected Creature(Controller controller) {
        this.controller = controller;
        knownList = new KnownList(this);
        ai = AIFactory.create(this);
        for (int i = 0; i < bags.length; i++)
            bags[i] = new Storage(this);
    }
    public Controller getController() { return controller; }
    public void setStats(Stats stats) { this.stats = stats; }
    public void setMoveController(MoveController mc) { this.moveController = mc; }
}''',
    'com/x/world/Npc.java': '''package com.x.world;
public class Npc extends Creature {
    public Npc(NpcController controller) {
        super(controller);
        controller.setOwner(this);
        setupStats();
    }
    protected void setupStats() { setStats(new Stats(this)); }
}''',
    'com/x/world/Player.java': '''package com.x.world;
public class Player extends Creature {
    public Player() {
        super(new Controller());
        getController().setOwner(this);
    }
}''',
    'com/x/world/Spawner.java': '''package com.x.world;
public class Spawner {
    public static Npc spawn() {
        Npc npc = new Npc(new NpcController());
        npc.setMoveController(new MoveController(npc));
        World.storeObject(npc);
        npc.setStats(new Stats(npc));
        return npc;
    }
}''',
    'com/x/world/World.java': '''package com.x.world;
public class World { public static void storeObject(Creature c) {} }''',
    'com/x/world/Controller.java': '''package com.x.world;
public class Controller { private Creature owner; public void setOwner(Creature owner) { this.owner = owner; } }''',
    'com/x/world/NpcController.java': '''package com.x.world;
public class NpcController extends Controller {}''',
    'com/x/world/KnownList.java': '''package com.x.world;
public class KnownList { private final Creature owner; private Stats sibling; public KnownList(Creature owner) { this.owner = owner; } }''',
    'com/x/world/Stats.java': '''package com.x.world;
public class Stats { private final Creature owner; private int value; public Stats(Creature owner) { this.owner = owner; } public void inc() { value++; } }''',
    'com/x/world/MoveController.java': '''package com.x.world;
public class MoveController { private final Creature owner; public MoveController(Creature owner) { this.owner = owner; } }''',
    'com/x/world/Equipment.java': '''package com.x.world;
public class Equipment { private final Creature owner; public Equipment(Creature owner) { this.owner = owner; } }''',
    'com/x/world/Storage.java': '''package com.x.world;
public class Storage { private Creature actor; public Storage(Creature actor) { this.actor = actor; } public void setActor(Creature a) { actor = a; } }''',
    'com/x/world/AI.java': '''package com.x.world;
public class AI { private final Creature owner; private int state; public AI(Creature owner) { this.owner = owner; } public void set() { state = 1; } }''',
    'com/x/world/AIFactory.java': '''package com.x.world;
public class AIFactory { public static AI create(Creature c) { return new AI(c); } }''',
    'com/x/world/Confined.java': '''package com.x.world;
public class Confined { private Creature creature; private java.util.List<Creature> list; void f() { creature = null; list = null; } }''',
}


class FieldTableTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fm = fieldmap.build_from_sources(FILES, CONFIG)

    def field(self, cid, name):
        return next(f for f in self.fm.classes[cid].fields if f.name == name)

    def cpp(self, cid, name):
        return self.field(cid, name).cpp

    def test_scalars_and_strings(self):
        h = 'com.x.world.Holder'
        self.assertEqual(self.cpp(h, 'id'), 'const int32_t')
        self.assertEqual(self.cpp(h, 'hp'), 'Field<int32_t>')
        self.assertEqual(self.cpp(h, 'dead'), 'Field<bool>')
        self.assertEqual(self.field(h, 'dead').rule, 'volatile scalar')
        self.assertEqual(self.cpp(h, 'nullable'), 'const std::optional<int32_t>')  # private, never written: effectively final
        self.assertEqual(self.cpp(h, 'name'), 'const std::string')
        self.assertEqual(self.cpp(h, 'race'), 'const Race')

    def test_references(self):
        h = 'com.x.world.Holder'
        self.assertEqual(self.cpp(h, 'owner'), 'const Ref<Creature>')
        self.assertEqual(self.cpp(h, 'target'), 'Field<Ref<Creature>>')
        self.assertEqual(self.cpp(h, 'template'), 'const Template*')
        self.assertEqual(self.cpp(h, 'connection'), 'const std::shared_ptr<GameConnection>')
        self.assertEqual(self.field(h, 'connection').rule, 'connection')

    def test_collections_arrays_atomics(self):
        h = 'com.x.world.Holder'
        self.assertEqual(self.cpp(h, 'byId'), 'ConcurrentHashMap<int32_t, Ref<Creature>>')
        self.assertEqual(self.cpp(h, 'list'), 'CopyOnWriteArrayList<Ref<Creature>>')  # concrete type from the constructor
        self.assertEqual(self.cpp(h, 'ids'), 'ConcurrentKeySet<int32_t>')
        self.assertEqual(self.cpp(h, 'counters'), 'const Ref<Array<int32_t>>')
        self.assertEqual(self.cpp(h, 'tasks'), 'const Ref<Array<FutureRef>>')
        self.assertEqual(self.cpp(h, 'count'), 'AtomicInteger')
        self.assertEqual(self.cpp(h, 'ref'), 'AtomicReference<Ref<Creature>>')
        self.assertEqual(self.cpp(h, 'task'), 'const FutureRef')
        self.assertEqual(self.cpp(h, 'lock'), 'Monitor')
        self.assertEqual(self.cpp(h, 'mutex'), 'Monitor')
        self.assertEqual(self.cpp(h, 'callback'), 'const PinnedCallback<void(Creature&)>')
        self.assertEqual((self.cpp(h, 'stamp'), self.field(h, 'stamp').flags), ('const Timestamp', []))  # EXTERNAL_SPELLING (hub-headers.md §6)

    def test_external_spellings_and_handles(self):
        fm = fieldmap.build_from_sources({**FILES, 'com/x/world/Clock.java': '''package com.x.world;
import java.awt.geom.*;
public class Clock implements java.awt.Shape {
  private java.util.Date next; private final java.time.Instant start = null; private java.time.LocalDate day; private java.time.Duration period;
  private org.quartz.JobDetail job; private final org.quartz.CronExpression cron = null; private java.util.zip.CRC32 crc;
  private Rectangle2D bounds; private GeneralPath path;
  public static Clock current;
  public void reset() { next = null; day = null; period = null; job = null; crc = null; bounds = null; path = null; }
}'''}, CONFIG)
        clock = {f.name: f for f in fm.classes['com.x.world.Clock'].fields}
        self.assertEqual({n: f.cpp for n, f in clock.items() if not f.static}, {
            'next': 'Field<Timestamp>', 'start': 'const Timestamp', 'day': 'Field<Date>', 'period': 'Field<std::chrono::milliseconds>',
            'job': 'Field<Ref<JobDetail>>', 'cron': 'const CronExpression', 'crc': 'Field<CRC32>', 'bounds': 'Field<Rectangle2D>',
            'path': 'Field<Path2D>'})
        self.assertEqual(clock['job'].rule, 'non-final external handle')
        self.assertEqual([n for n, f in clock.items() if 'externalType' in f.flags], ['crc'])

    def test_erasure_rule(self):
        # hub-headers.md §8.1: every type parameter has a project bound -> one non-template class, variables spelled as the bound
        fm = fieldmap.build_from_sources({**FILES, 'com/x/world/Box.java': '''package com.x.world;
public class Box<T extends Creature> { private T item; public void set(T t) { item = t; } }''',
                                          'com/x/world/Bag.java': '''package com.x.world;
public class Bag<E> { private E element; public void set(E e) { element = e; } }''',
                                          'com/x/world/Crate.java': '''package com.x.world;
public class Crate { private Box<Player> box; private Bag<Player> bag; private final java.util.List<Box<Player>> boxes = new java.util.ArrayList<>();
  public void set() { box = null; bag = null; } public static final java.util.List<Crate> ALL = new java.util.ArrayList<>(); }''',
                                          'com/x/world/Player.java': '''package com.x.world;
public class Player extends Creature {}'''}, CONFIG)
        self.assertTrue(fm.erased_generic(fm.classes['com.x.world.Box'].td))
        self.assertFalse(fm.erased_generic(fm.classes['com.x.world.Bag'].td))
        self.assertEqual({f.name: f.cpp for f in fm.classes['com.x.world.Box'].fields}['item'], 'Field<Ref<Creature>>')
        self.assertEqual({f.name: f.cpp for f in fm.classes['com.x.world.Bag'].fields}['element'], 'Field<Ref<E>>')
        crate = {f.name: f.cpp for f in fm.classes['com.x.world.Crate'].fields if not f.static}
        self.assertEqual(crate, {'box': 'Field<Ref<Box>>', 'bag': 'Field<Ref<Bag<Player>>>', 'boxes': 'ArrayList<Ref<Box>>'})

    def test_erasure_tables_match_skeleton(self):
        import skeleton
        self.assertEqual(fieldmap.ERASURE_BOUNDS, skeleton.ERASURE_BOUNDS)
        self.assertEqual(fieldmap.TEMPLATE_GENERICS, frozenset(skeleton.TEMPLATE_GENERICS))

    def test_dropped_fields_and_lock_classes_in_member_blocks(self):
        cfg = CONFIG + '"com.x.world.Holder.mutex" = { drop = true, reason = "no such member in the port" }\n'
        fm = fieldmap.build_from_sources(FILES, cfg)
        holder = {f.name: f for f in fm.classes['com.x.world.Holder'].fields}
        self.assertIsNone(holder['mutex'].cpp)
        self.assertEqual(holder['mutex'].rule, 'fieldmap.toml override: dropped')
        text = fm.member_block('com.x.world.Holder')
        self.assertNotIn(' mutex', text)
        self.assertIn('ConcurrentHashMap<int32_t, Ref<Creature>> byId{AION_LOCK_CLASS(Holder::byId#stripe)};', text)
        self.assertIn('AtomicInteger count{AION_LOCK_CLASS(Holder::count)};', text)
        self.assertIn('Monitor lock{AION_LOCK_CLASS(Holder::lock)};', text)
        self.assertIn('const FutureRef task;', text)
        with self.assertRaises(fieldmap.FieldmapError):
            fieldmap.build_from_sources(FILES, CONFIG + '"com.x.world.Holder.lock" = { drop = true, cpp = "Monitor", reason = "x" }\n')

    def test_non_final_collection(self):
        fm = fieldmap.build_from_sources({**FILES, 'com/x/world/Holder2.java': '''package com.x.world;
import java.util.*;
public class Holder2 { private List<Creature> items = new ArrayList<>(); private Map<Integer, List<Creature>> groups;
  public void reset() { items = new ArrayList<>(); groups = null; } private static Holder2 h; }'''}, CONFIG)
        f = next(x for x in fm.classes['com.x.world.Holder2'].fields if x.name == 'items')
        self.assertEqual(f.cpp, 'Field<Ref<RcArrayList<Ref<Creature>>>>')
        g = next(x for x in fm.classes['com.x.world.Holder2'].fields if x.name == 'groups')
        self.assertEqual(g.cpp, 'Field<Ref<RcHashMap<int32_t, Ref<RcArrayList<Ref<Creature>>>>>>')
        self.assertIn('guessedImpl', g.flags)

    def test_effectively_final(self):
        h = 'com.x.world.Holder'
        self.assertTrue(self.field(h, 'effectively').eff_final)
        self.assertEqual(self.cpp(h, 'effectively'), 'const int32_t')
        self.assertEqual(self.field(h, 'effectively').rule, 'effectively final scalar')
        self.assertFalse(self.field(h, 'writtenLater').eff_final)
        self.assertFalse(self.field(h, 'writtenInLambda').eff_final)

    def test_element_writes_do_not_reassign_arrays(self):
        fm = fieldmap.build_from_sources({**FILES, 'com/x/world/Vars.java': '''package com.x.world;
public class Vars {
    private int[] vars = new int[6];
    private Holder[] slots;
    private int[] replaced = new int[2];
    public Vars() { slots = new Holder[3]; }
    public void set(int i, int v) { vars[i] = v; vars[0]++; slots[i] = null; }
    public void reset() { replaced = new int[2]; }
    public static Vars share() { return new Vars(); }
}''', 'com/x/world/VarsUser.java': '''package com.x.world;
public class VarsUser { private static final Vars shared = Vars.share(); }'''}, CONFIG)
        fields = {f.name: f for f in fm.classes['com.x.world.Vars'].fields}
        self.assertTrue(fields['vars'].eff_final)
        self.assertEqual(fields['vars'].cpp, 'const Ref<Array<int32_t>>')
        self.assertEqual(fields['vars'].rule, 'effectively final array')
        self.assertTrue(fields['slots'].eff_final)
        self.assertFalse(fields['replaced'].eff_final)
        self.assertEqual(fields['replaced'].cpp, 'Field<Ref<Array<int32_t>>>')

    def test_statics(self):
        r = 'com.x.Registry'
        self.assertEqual(self.cpp(r, 'instance'), 'static Registry& getInstance()')
        self.assertEqual(self.cpp(r, 'creatures'), 'static inline ArrayList<Ref<Creature>>')
        self.assertEqual(self.cpp(r, 'lastPlayer'), 'static inline Field<Ref<Player>>')
        self.assertEqual(self.cpp(r, 'MAX'), 'static constexpr int32_t')
        self.assertEqual(self.cpp(r, 'NAME'), 'static constexpr std::string_view')
        self.assertEqual(self.cpp(r, 'log'), 'static const Logger log')
        self.assertEqual(self.cpp(r, 'depth'), 'static thread_local std::optional<int32_t>')
        self.assertEqual(self.cpp('com.x.configs.GSConfig', 'LEVEL'), 'static inline std::atomic<int32_t>')
        self.assertEqual(self.cpp('com.x.configs.GSConfig', 'NAME'), 'static inline ConfigValue<std::string>')

    def test_k1_members(self):
        f = self.field('com.x.data.Template', 'id')
        self.assertIsNone(f.cpp)
        self.assertTrue(f.bound)

    def test_value_types_and_confined_overrides(self):
        files = {**FILES, 'com/x/math/Vec.java': '''package com.x.math;
public class Vec { public float x; public void set(float v) { x = v; } }''',
                 'com/x/net/Crypt.java': '''package com.x.net;
public class Crypt { private boolean enabled; public void enable() { enabled = true; } }''',
                 'com/x/world/Mover.java': '''package com.x.world;
import com.x.math.Vec;
import com.x.net.Crypt;
public class Mover { private static Mover last; private Vec position = new Vec(); private final Vec origin = new Vec(); private Crypt crypt;
  public void move(Vec v) { position = v; crypt = null; last = this; } }'''}
        cfg = CONFIG + '''
[kinds]
"com.x.net.Crypt" = { kind = "K5", reason = "strand-confined" }
'''
        cfg = cfg.replace('[settings]\n', '[settings]\nvalue_types = ["com.x.math.Vec"]\n')
        fm = fieldmap.build_from_sources(files, cfg)
        self.assertEqual(fm.classes['com.x.math.Vec'].kind, 'K5')
        self.assertIn('value type', fm.classes['com.x.math.Vec'].kind_reason)
        mover = {f.name: f for f in fm.classes['com.x.world.Mover'].fields}
        self.assertEqual(fm.classes['com.x.world.Mover'].kind, 'K4')
        self.assertEqual(mover['position'].cpp, 'Field<Vec>')
        self.assertEqual(mover['origin'].cpp, 'const Vec')
        self.assertEqual(mover['position'].retains, [])
        self.assertEqual(mover['crypt'].cpp, 'Crypt')
        self.assertEqual(next(f for f in fm.classes['com.x.math.Vec'].fields if f.name == 'x').cpp, 'float')
        with self.assertRaises(fieldmap.FieldmapError):
            fieldmap.build_from_sources(files, CONFIG.replace('[settings]\n', '[settings]\nvalue_types = ["com.x.Nope"]\n'))

    def test_confined_members(self):
        self.assertEqual(self.fm.classes['com.x.world.Confined'].kind, 'K5')
        self.assertEqual(self.cpp('com.x.world.Confined', 'creature'), 'Ptr<Creature>')
        self.assertEqual(self.cpp('com.x.world.Confined', 'list'), 'std::vector<Ptr<Creature>>')

    def test_confined_elements_are_values(self):
        extra = {
            'com/x/geo/Results.java': '''package com.x.geo;
import java.util.*;
public class Results { private final List<Result> results = new ArrayList<>(); private Result closest; private Result[] sorted;
    void add(Result r) { results.add(r); closest = r; } }''',
            'com/x/geo/Result.java': '''package com.x.geo;
public class Result { private float distance; void set(float d) { distance = d; } }''',
        }
        fm = fieldmap.build_from_sources({**FILES, **extra}, CONFIG)
        fields = {f.name: f.cpp for f in fm.classes['com.x.geo.Results'].fields}
        # elements of a confined class are held by value (CollisionResults.results); a direct member stays a pointer
        self.assertEqual((fields['results'], fields['closest'], fields['sorted']), ('std::vector<Result>', 'Result*', 'std::vector<Result>'))


class PartsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fm = fieldmap.build_from_sources(FILES, CONFIG)

    def part(self, owner, name):
        return self.fm.parts.get((owner, name))

    def cpp(self, cid, name):
        return next(f for f in self.fm.classes[cid].fields if f.name == name).cpp

    def test_pattern1(self):
        p = self.part('com.x.world.Creature', 'knownList')
        self.assertEqual(p.patterns, {1})
        self.assertEqual(self.cpp('com.x.world.Creature', 'knownList'), 'const std::unique_ptr<KnownList>')
        self.assertEqual(self.part('com.x.world.Creature', 'equipment').patterns, {1})  # field initializer
        self.assertTrue(self.part('com.x.world.Creature', 'bags').element)
        self.assertEqual(self.cpp('com.x.world.Creature', 'bags'), 'const std::array<std::unique_ptr<Storage>, 2>')

    def test_pattern2_setter_helper_and_fresh_local(self):
        p = self.part('com.x.world.Creature', 'stats')
        self.assertEqual(p.patterns, {2})
        self.assertIn('com.x.world.Npc', p.owner_classes)  # setupStats() called from the constructor
        self.assertEqual(self.cpp('com.x.world.Creature', 'stats'), 'PartSlot<Stats>')
        mc = self.part('com.x.world.Creature', 'moveController')
        self.assertEqual(mc.patterns, {2})
        self.assertEqual([e['line'] for e in mc.evidence if 'Spawner' in e['file']], [5])  # setStats after storeObject is not evidence
        self.assertFalse(any('npc.setStats' in e['text'] for e in p.evidence))

    def test_pattern3_late_bound_controller(self):
        p = self.part('com.x.world.Creature', 'controller')
        self.assertEqual(p.patterns, {3})
        self.assertEqual(p.owner_classes, {'com.x.world.Npc', 'com.x.world.Player'})  # parameter trace and getter
        self.assertEqual(self.cpp('com.x.world.Controller', 'owner'), 'Final<Creature*>')

    def test_pattern4_toml(self):
        p = self.part('com.x.world.Creature', 'ai')
        self.assertEqual(p.patterns, {4})
        self.assertEqual(self.cpp('com.x.world.Creature', 'ai'), 'PartSlot<AI>')
        self.assertEqual(self.cpp('com.x.world.AI', 'owner'), 'OwnerRef<Creature>')

    def test_owner_and_sibling_fields(self):
        self.assertEqual(self.cpp('com.x.world.KnownList', 'owner'), 'OwnerRef<Creature>')
        self.assertEqual(self.cpp('com.x.world.Storage', 'actor'), 'SelfOrRef<Creature>')
        self.assertEqual(self.cpp('com.x.world.KnownList', 'sibling'), 'Field<Stats*>')
        self.assertEqual(self.fm.base_of(self.fm.classes['com.x.world.KnownList']), 'OwnedPart')
        self.assertEqual(self.fm.classes['com.x.world.KnownList'].part_of, ['com.x.world.Creature.knownList'])

    def test_non_private_owner_fields_use_the_write_scan(self):
        extra = {
            'com/x/world/Mover.java': '''package com.x.world;
public class Mover { protected Creature owner; public Mover(Creature owner) { this.owner = owner; } }''',
            'com/x/world/FastMover.java': '''package com.x.world;
public class FastMover extends Mover { public FastMover(Creature c) { super(c); owner = c; } }''',
            'com/x/world/Bag.java': '''package com.x.world;
public class Bag { Creature actor; public Bag(Creature actor) { this.actor = actor; } }''',
            'com/x/world/BagService.java': '''package com.x.world;
public class BagService { static void give(Bag bag, Creature other) { bag.actor = other; } }''',
            'com/x/world/Walker.java': '''package com.x.world;
public class Walker extends Creature {
    private final Mover mover = new Mover(this);
    private final Bag bag = new Bag(this);
    public Walker() { super(new Controller()); }
}''',
        }
        fm = fieldmap.build_from_sources({**FILES, **extra}, CONFIG)
        owner = next(f for f in fm.classes['com.x.world.Mover'].fields if f.name == 'owner')
        self.assertEqual((owner.cpp, owner.rule), ('OwnerRef<Creature>', 'part owner (assigned only in constructors)'))
        actor = next(f for f in fm.classes['com.x.world.Bag'].fields if f.name == 'actor')
        self.assertEqual(actor.cpp, 'SelfOrRef<Creature>')
        self.assertEqual(self.cpp('com.x.world.Storage', 'actor'), 'SelfOrRef<Creature>')   # private, setActor

    def test_owner_fields_do_not_retain(self):
        self.assertEqual(next(f for f in self.fm.classes['com.x.world.KnownList'].fields if f.name == 'owner').retains, [])
        self.assertEqual(next(f for f in self.fm.classes['com.x.world.Storage'].fields if f.name == 'actor').retains,
                         ['com.x.world.Creature'])  # SelfOrRef may hold a foreign object

    def test_shared_class_trees_are_no_parts(self):
        extra = {
            'com/x/world/Observer.java': '''package com.x.world;
public abstract class Observer { private static final java.util.List<Observer> ALL = new java.util.ArrayList<>(); public void attacked() {} }''',
            'com/x/world/Charge.java': '''package com.x.world;
public class Charge extends Observer { private final Weapon weapon; public Charge(Weapon weapon) { this.weapon = weapon; } }''',
            'com/x/world/Stone.java': '''package com.x.world;
public abstract class Stone { private static final java.util.List<Stone> ALL = new java.util.ArrayList<>(); protected int slot; }''',
            'com/x/world/ManaStone.java': '''package com.x.world;
public class ManaStone extends Stone { public ManaStone(int slot) { this.slot = slot; } }''',
            'com/x/world/IdianStone.java': '''package com.x.world;
public class IdianStone extends Stone { private final Weapon weapon; public IdianStone(Weapon weapon) { this.weapon = weapon; } }''',
            'com/x/world/Weapon.java': '''package com.x.world;
public class Weapon {
    private static final java.util.List<Weapon> ALL = new java.util.ArrayList<>();
    private Charge charge;
    private IdianStone idian;
    public Weapon() { charge = new Charge(this); idian = new IdianStone(this); }
}''',
        }
        fm = fieldmap.build_from_sources({**FILES, **extra}, CONFIG)
        cls = fm.classes
        # Charge derives the shared Observer (RefCounted): no part, a retaining owner reference, and a warning
        self.assertIsNone(fm.parts.get(('com.x.world.Weapon', 'charge')))
        self.assertEqual({f.name: f.cpp for f in cls['com.x.world.Weapon'].fields}['charge'], 'const Ref<Charge>')
        self.assertEqual({f.name: f.cpp for f in cls['com.x.world.Charge'].fields}['weapon'], 'const Ref<Weapon>')
        self.assertEqual(cls['com.x.world.Charge'].part_of, [])
        self.assertIsNone(fm.base_of(cls['com.x.world.Charge']))
        self.assertTrue(any('com.x.world.Charge is no part' in w for w in fm.warnings))
        # without a [bases] entry the abstract Stone gives IdianStone a RefCounted tree too
        self.assertIsNone(fm.parts.get(('com.x.world.Weapon', 'idian')))
        # [bases] "none": each subclass picks its base, so IdianStone is a part and ManaStone RefCounted
        cfg = CONFIG.replace('[fields]', '[bases]\n"com.x.world.Stone" = { base = "none", reason = "subclass bases differ" }\n[fields]')
        fm = fieldmap.build_from_sources({**FILES, **extra}, cfg)
        cls = fm.classes
        self.assertEqual(fm.parts[('com.x.world.Weapon', 'idian')].part_types, {'com.x.world.IdianStone'})
        self.assertEqual((fm.base_of(cls['com.x.world.Stone']), fm.base_of(cls['com.x.world.ManaStone']), fm.base_of(cls['com.x.world.IdianStone'])),
                         (None, 'RefCounted', 'OwnedPart'))
        self.assertEqual({f.name: f.cpp for f in cls['com.x.world.IdianStone'].fields}['weapon'], 'OwnerRef<Weapon>')
        with self.assertRaises(fieldmap.FieldmapError):
            fieldmap.build_from_sources(FILES, CONFIG.replace('[fields]', '[bases]\n"com.x.world.Stats" = { base = "Weird", reason = "x" }\n[fields]'))

    def test_parts_json(self):
        import json
        d = json.loads(self.fm.parts_json_text())
        self.assertEqual(d['format'], 'aion-parts')
        owners = {(p['owner'], p['field']) for p in d['parts']}
        self.assertIn(('com.x.world.Creature', 'controller'), owners)
        self.assertTrue(all(p['evidence'] for p in d['parts']))


if __name__ == '__main__':
    unittest.main()
