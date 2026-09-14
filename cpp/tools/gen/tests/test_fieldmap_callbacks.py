"""fieldmap.py: callbacks (storage inference, captures, generated structs), cycles, outputs and the CLI."""
import contextlib
import io
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import fieldmap  # noqa: E402

CONFIG = '''
[future_apis]
"ThreadPoolManager.schedule" = { reason = "test" }
"ThreadPoolManager.scheduleAtFixedRate" = { reason = "test" }
[stored_callback_apis]
"Cron.schedule" = { owner = "com.x.util.Cron", reason = "external job store" }
'''

FILES = {
    'com/x/util/ThreadPoolManager.java': '''package com.x.util;
import java.util.concurrent.Future;
public class ThreadPoolManager {
    private static final ThreadPoolManager instance = new ThreadPoolManager();
    public static ThreadPoolManager getInstance() { return instance; }
    public Future<?> schedule(Runnable r, long delay) { return null; }
    public Future<?> scheduleAtFixedRate(Runnable r, long delay, long period) { return null; }
}''',
    'com/x/util/Cron.java': '''package com.x.util;
public class Cron {
    private static final Cron instance = new Cron();
    public static Cron getInstance() { return instance; }
    public void schedule(Runnable r, String expr) {}
}''',
    'com/x/obs/Observer.java': '''package com.x.obs;
public abstract class Observer { public void onDeath() {} }''',
    'com/x/obs/ObserveController.java': '''package com.x.obs;
import java.util.*;
public class ObserveController {
    private final List<Observer> observers = new ArrayList<>();
    public void addObserver(Observer o) { observers.add(o); }
    public void attach(Observer o) { addObserver(o); }
}''',
    'com/x/obs/Helpers.java': '''package com.x.obs;
import com.x.world.Player;
public class Helpers {
    public static void watch(Player player, Runnable onDeath) {
        player.getObserveController().attach(new Observer() {
            @Override public void onDeath() { onDeath.run(); }
        });
    }
}''',
    'com/x/world/Player.java': '''package com.x.world;
import java.util.*;
import java.util.concurrent.Future;
import com.x.obs.*;
import com.x.util.*;
public class Player {
    private final ObserveController observeController = new ObserveController();
    private Kisk kisk;
    private Future<?> regenTask;
    private final List<Future<?>> tasks = new ArrayList<>();
    private int hp;
    public ObserveController getObserveController() { return observeController; }
    public void setKisk(Kisk k) { kisk = k; hp = 0; }
    public void startRegen() {
        regenTask = ThreadPoolManager.getInstance().scheduleAtFixedRate(() -> hp++, 1000, 1000);
        tasks.add(ThreadPoolManager.getInstance().schedule(this::heal, 10));
        Future<?> f = ThreadPoolManager.getInstance().schedule(new Runnable() {
            public void run() { heal(); }
        }, 5);
        Cron.getInstance().schedule(() -> heal(), "0 0 * * * ?");
        List<Integer> xs = new ArrayList<>();
        xs.forEach(x -> hp += x);
    }
    void heal() { hp = 100; }
    class Inner { int outerHp() { return hp; } }
    static class Nested { int v; }
}''',
    'com/x/world/Kisk.java': '''package com.x.world;
import com.x.obs.*;
public class Kisk {
    private final Player creator;
    private int uses;
    public Kisk(Player creator) {
        this.creator = creator;
        final String name = "kisk";
        creator.getObserveController().addObserver(new Observer() {
            private int calls;
            @Override public void onDeath() { calls++; uses = 0; System.out.println(name + creator); }
        });
        Helpers.watch(creator, () -> uses = 1);
    }
}''',
}

CYCLES = '''
[resolutions]
"com.x.world.Player.kisk" = "cpp-breaker: LogoutBreakers::run"
"com.x.gone.Old.field" = "accepted: removed"
'''


def _capture_edge_keys(fm):
    """keys of capture edges of the retaining graph, also outside strongly connected components"""
    keys = set()
    for ci in fm.classes.values():
        for cap in ci.captures:
            if cap.cpp and not cap.cpp.startswith('OwnerRef'):
                keys.add(f'{ci.cid}#{cap.name}')
    return keys


class CallbacksTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fm = fieldmap.build_from_sources(FILES, CONFIG, CYCLES)

    def callbacks_of(self, cid, kind=None):
        return [cb for cb in self.fm.callbacks.values() if cb.ci.cid == cid and (kind is None or cb.kind == kind)]

    def test_future_held_by_field(self):
        cb = next(c for c in self.callbacks_of('com.x.world.Player', 'lambda') if c.api == 'scheduleAtFixedRate')
        self.assertEqual(cb.storage, 'task')
        self.assertEqual([o.cid for o in cb.owners], ['com.x.world.Player'])
        self.assertEqual(cb.future_holder, 'field Player.regenTask')
        self.assertEqual([(c.name, c.kind, c.cpp) for c in cb.captures], [('this', 'this', 'const Ref<Player>')])
        self.assertEqual(self.fm.pin_list(cb), ['this'])

    def test_future_added_to_field_collection(self):
        cb = next(c for c in self.callbacks_of('com.x.world.Player', 'method_ref'))
        self.assertEqual(cb.storage, 'task')
        self.assertEqual([o.cid for o in cb.owners], ['com.x.world.Player'])

    def test_anonymous_task_and_local_future(self):
        anon = self.fm.classes['com.x.world.Player$1']
        self.assertEqual(anon.storage.storage, 'task')
        self.assertEqual([c.name for c in anon.captures], ['this'])

    def test_configured_stored_api(self):
        cb = next(c for c in self.callbacks_of('com.x.world.Player', 'lambda') if c.api == 'schedule' and c.storage == 'stored')
        self.assertEqual([o.cid for o in cb.owners], ['com.x.util.Cron'])

    def test_sync_lambdas_are_not_recorded(self):
        self.assertFalse(any(c.context == 'argument 1 of forEach()' for c in self.fm.callbacks.values()))

    def test_inferred_storing_method_and_captures(self):
        anon = self.fm.classes['com.x.world.Kisk$1']
        self.assertEqual(anon.storage.storage, 'stored')
        self.assertEqual([o.cid for o in anon.storage.owners], ['com.x.obs.ObserveController'])
        caps = {c.name: c for c in anon.captures}
        self.assertEqual(set(caps), {'this', 'name', 'creator'})
        self.assertEqual(caps['creator'].cpp, 'const Ref<Player>')
        self.assertEqual(caps['name'].cpp, 'const std::string')
        self.assertEqual(caps['this'].cpp, 'const Ref<Kisk>')
        calls = next(f for f in anon.fields if f.name == 'calls')
        self.assertEqual(calls.cpp, 'Field<int32_t>')

    def test_captured_owner_of_only_one_of_several_lists_retains(self):
        extra = {
            'com/x/obs/ObserveController.java': '''package com.x.obs;
import java.util.*;
public class ObserveController {
    private final List<Observer> observers = new ArrayList<>();
    public void addObserver(Observer o) { observers.add(o); }
    public void removeObserver(Observer o) { observers.remove(o); }
    public void attach(Observer o) { addObserver(o); }
}''',
            'com/x/world/Buff.java': '''package com.x.world;
import java.util.*;
import com.x.obs.*;
public class Buff {
    private final Player target;
    private final List<Observer> own = new ArrayList<>();
    private final List<Runnable> removeTasks = new ArrayList<>();
    public Buff(Player target) { this.target = target; }
    void addObserver(Observer observer) {
        target.getObserveController().addObserver(observer);
        removeTasks.add(() -> target.getObserveController().removeObserver(observer));
    }
    void keep(Observer observer) { own.add(observer); }
    public void start() {
        addObserver(new Observer() { @Override public void onDeath() { end(); } });
        keep(new Observer() { @Override public void onDeath() { end(); } });
    }
    void end() {}
}''',
        }
        fm = fieldmap.build_from_sources({**FILES, **extra}, CONFIG, CYCLES)
        shared = fm.classes['com.x.world.Buff$1']
        self.assertEqual(sorted(o.cid for o in shared.storage.owners), ['com.x.obs.ObserveController', 'com.x.world.Buff'])
        self.assertEqual({c.name: c.cpp for c in shared.captures}['this'], 'const Ref<Buff>')
        own = fm.classes['com.x.world.Buff$2']
        self.assertEqual([o.cid for o in own.storage.owners], ['com.x.world.Buff'])
        self.assertEqual({c.name: c.cpp for c in own.captures}['this'], 'OwnerRef<Buff>')
        self.assertIn('com.x.world.Buff$1#this', {k for u in [fm.cycle_edges] for k in u} | {e for e in _capture_edge_keys(fm)})

    def test_parameter_captured_by_stored_anonymous_class(self):
        # Helpers.watch stores onDeath through the anonymous observer it attaches
        cb = next(c for c in self.callbacks_of('com.x.world.Kisk', 'lambda'))
        self.assertEqual(cb.storage, 'stored')
        self.assertEqual([o.cid for o in cb.owners], ['com.x.obs.ObserveController'])

    def test_inner_class_this0(self):
        inner = self.fm.classes['com.x.world.Player.Inner']
        self.assertEqual([(c.name, c.ci.cid) for c in inner.captures], [('this$0', 'com.x.world.Player')])
        self.assertEqual(self.fm.classes['com.x.world.Player.Nested'].captures, [])

    def test_member_block_and_struct(self):
        text = self.fm.member_block('com.x.world.Kisk')
        self.assertIn('struct Kisk_Observer final : Observer {', text)
        self.assertIn('const Ref<Player> creator;', text)
        self.assertIn('static Ref<Kisk_Observer> create(', text)
        self.assertIn('Field<int32_t> uses;', text)
        text = self.fm.member_block('com.x.world.Player')
        self.assertIn('struct Player_Runnable final : TaskStruct {', text)
        self.assertIn('pin {this}', text)
        with self.assertRaises(fieldmap.FieldmapError):
            self.fm.member_block('com.x.Missing')


class CyclesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fm = fieldmap.build_from_sources(FILES, CONFIG, CYCLES)

    def test_edges_and_resolutions(self):
        edges = self.fm.cycle_edges
        self.assertIn('com.x.world.Player.kisk', edges)
        self.assertEqual(edges['com.x.world.Player.kisk']['resolution'], 'cpp-breaker: LogoutBreakers::run')
        self.assertIn('com.x.world.Kisk.creator', edges)
        self.assertIsNone(edges['com.x.world.Kisk.creator']['resolution'])
        self.assertIn('com.x.world.Kisk$1#creator', edges)  # capture edge through the stored observer
        self.assertEqual(edges['com.x.world.Kisk$1#creator']['kind'], 'capture')
        example = edges['com.x.world.Player.kisk']['example']
        self.assertEqual(example[0][0], 'com.x.world.Player')
        self.assertEqual(example[-1][2], 'com.x.world.Player')
        self.assertEqual(self.fm.stale_resolutions, ['com.x.gone.Old.field'])

    def test_task_suggestion(self):
        key = next(k for k in self.fm.cycle_edges if k.startswith('com.x.world.Player@') and k.endswith('#this'))
        self.assertTrue(self.fm.cycle_edges[key].get('suggestion', '').startswith(('java-hook', 'accepted')))

    def test_report(self):
        text = self.fm.cycles_report_text()
        self.assertIn('## Component 1', text)
        self.assertIn('**UNRESOLVED**', text)
        self.assertIn('## Stale cycles.toml keys', text)
        self.assertIn('# "com.x.world.Kisk.creator" = ""', text)


RETENTION_FILES = {
    'com/y/Roots.java': '''package com.y;
public class Roots {
    public static Owner owner;
    public static Node node;
    public static Node2 node2;
    public static Plain plain;
}''',
    'com/y/Owner.java': '''package com.y;
public class Owner {
    private final Part part = new Part(this);
    private Other other;
    public Part getPart() { return part; }
    public void link() { other = new Other(part); }
}''',
    'com/y/Part.java': '''package com.y;
public class Part {
    private final Owner owner;
    private int hits;
    public Part(Owner owner) { this.owner = owner; }
    public void hit() { hits++; }
}''',
    'com/y/Other.java': '''package com.y;
public class Other {
    private final Part part;
    private int n;
    public Other(Part part) { this.part = part; }
    public void inc() { n++; }
}''',
    'com/y/Node.java': '''package com.y;
public class Node {
    private Node next;
    public void setNext(Node n) { next = n; }
}''',
    'com/y/Node2.java': '''package com.y;
public class Node2 {
    private Node2 next;
    public void setNext(Node2 n) { next = n; }
}''',
    'com/y/Req.java': '''package com.y;
public abstract class Req { public abstract void run(); }''',
    'com/y/Registry.java': '''package com.y;
import java.util.*;
public class Registry {
    private final List<Req> reqs = new ArrayList<>();
    public void add(Req r) { reqs.add(r); }
}''',
    'com/y/Service.java': '''package com.y;
public class Service {
    private static final Service instance = new Service();
    private final Registry registry = new Registry();
    private int count;
    public static Service getInstance() { return instance; }
    public void start() {
        registry.add(new Req() { public void run() { helper(); } });
    }
    void helper() { count++; }
}''',
    'com/y/Plain.java': '''package com.y;
public class Plain {
    private final Registry registry = new Registry();
    private int count;
    public void start() {
        registry.add(new Req() { public void run() { helper(); } });
    }
    void helper() { count++; }
}''',
}

RETENTION_CONFIG = '''
[fields]
"com.y.Node.next" = { cpp = "Field<Node*>", reason = "test: non-retaining spelling" }
'''


class RetainingGraphTest(unittest.TestCase):
    """Cycle graph corrections of the S0b cycle review: references to parts retain the owner, captured singletons do not retain,
    non-retaining fieldmap.toml spellings are not edges."""

    @classmethod
    def setUpClass(cls):
        cls.fm = fieldmap.build_from_sources(RETENTION_FILES, RETENTION_CONFIG, '')

    def test_reference_to_a_part_retains_its_owner(self):
        edges = self.fm.cycle_edges
        self.assertEqual(self.fm.classes['com.y.Owner'].fields[0].cpp, 'const std::unique_ptr<Part>')
        self.assertIn('com.y.Other.part', edges)  # Owner.other -> Other.part -> Part, and Part's owner is Owner
        self.assertEqual(edges['com.y.Other.part']['targets'], ['com.y.Owner'])
        self.assertIn('com.y.Owner.other', edges)

    def test_captured_singleton_is_not_retained(self):
        edges = self.fm.cycle_edges
        self.assertNotIn('com.y.Service$1#this', edges)
        self.assertNotIn('com.y.Service.registry', edges)
        self.assertIn('com.y.Plain$1#this', edges)  # the same shape with a multi-instance class stays a cycle
        self.assertEqual(edges['com.y.Registry.reqs']['targets'], ['com.y.Plain$1'])

    def test_non_retaining_override_spelling(self):
        node = next(f for f in self.fm.classes['com.y.Node'].fields if f.name == 'next')
        self.assertEqual((node.cpp, node.retains), ('Field<Node*>', []))
        self.assertNotIn('com.y.Node.next', self.fm.cycle_edges)
        self.assertIn('com.y.Node2.next', self.fm.cycle_edges)

    def test_spelling_retains(self):
        class C:
            cpp_name = 'MapRegion'
        spelling = fieldmap.FieldMap._spelling_retains
        self.assertFalse(spelling('Field<Ref<Array<MapRegion*>>>', C))
        self.assertFalse(spelling('Final<world::MapRegion*>', C))
        self.assertFalse(spelling('OwnerRef<MapRegion>', C))
        self.assertFalse(spelling('std::weak_ptr<const MapRegion>', C))
        self.assertTrue(spelling('Field<Ref<MapRegion>>', C))
        self.assertTrue(spelling('PartMap<int32_t, MapRegion>', C))
        self.assertTrue(spelling('Field<Ref<InstanceHandler>>', C))  # does not name the class: the Java type keeps the edge
        self.assertTrue(spelling('HashMap<int32_t, Ref<MapRegionData>>', C))  # a longer name is not the class


class OutputsTest(unittest.TestCase):
    def test_json_deterministic(self):
        a = fieldmap.build_from_sources(FILES, CONFIG, CYCLES)
        b = fieldmap.build_from_sources(dict(reversed(list(FILES.items()))), CONFIG, CYCLES)
        ta = a.fieldmap_json_text({'roots': []})
        tb = b.fieldmap_json_text({'roots': []})
        import re
        self.assertEqual(re.sub(r'tmp\w+', 'tmp', ta), re.sub(r'tmp\w+', 'tmp', tb))  # temporary root names differ
        d = json.loads(ta)
        self.assertEqual(d['format'], 'aion-fieldmap')
        self.assertIn('com.x.world.Player', d['classes'])
        self.assertTrue(any(k.startswith('com.x.world.Player@L') for k in d['callbacks']))
        self.assertEqual(d['summary']['unresolvedCycleEdges'], sum(1 for e in d['cycleEdges'].values() if e['resolution'] is None))

    def test_cli(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = os.path.join(tmp, 'src')
            for rel, text in FILES.items():
                p = os.path.join(root, rel)
                os.makedirs(os.path.dirname(p), exist_ok=True)
                with open(p, 'w', encoding='utf-8') as f:
                    f.write(text)
            out = os.path.join(tmp, 'out')
            os.makedirs(out)
            with open(os.path.join(out, 'fieldmap.toml'), 'w', encoding='utf-8') as f:
                f.write(CONFIG)
            args = ['--roots', root, '--support-roots', '--out', out, '--staticdata-classes', os.path.join(tmp, 'none.json')]
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                self.assertEqual(fieldmap.main(args), 0)
            self.assertEqual(sorted(os.listdir(out)), ['cycles_report.md', 'escape_report.md', 'fieldmap.json', 'fieldmap.toml', 'parts.json'])
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(fieldmap.main(args + ['--check']), 0)
            with open(os.path.join(out, 'parts.json'), 'a', encoding='utf-8') as f:
                f.write(' ')
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(fieldmap.main(args + ['--check']), 1)
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                self.assertEqual(fieldmap.main(args + ['--class', 'com.x.world.Kisk']), 0)
            self.assertIn('struct Kisk_Observer', buf.getvalue())
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(fieldmap.main(args + ['--class', 'com.x.Nope']), 2)


if __name__ == '__main__':
    unittest.main()
