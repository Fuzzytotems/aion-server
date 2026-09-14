"""fieldmap.py: class kinds K1-K5, escape analysis, equals overrides, synchronization counts."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import fieldmap  # noqa: E402

CONFIG = '''
[settings]
packet_bases = ["com.x.net.BasePacket"]
connection_bases = ["com.x.net.Connection"]
[future_apis]
"ThreadPoolManager.schedule" = { reason = "test" }
'''

FILES = {
    # static data (K1 heuristic)
    'com/x/data/ItemData.java': '''package com.x.data;
import java.util.*;
import javax.xml.bind.annotation.*;
@XmlRootElement(name = "items")
@XmlAccessorType(XmlAccessType.FIELD)
public class ItemData {
    @XmlElement(name = "item") private List<ItemTemplate> items;
    @XmlTransient private Map<Integer, com.x.world.Npc> cache = new HashMap<>();
    @XmlElements({ @XmlElement(name = "a", type = ActionA.class) }) private List<Action> actions;
}''',
    'com/x/data/ItemTemplate.java': '''package com.x.data;
import javax.xml.bind.annotation.*;
public class ItemTemplate extends TemplateBase { int id; }''',
    'com/x/data/TemplateBase.java': '''package com.x.data;
public abstract class TemplateBase { String name; }''',
    'com/x/data/Action.java': '''package com.x.data;
public abstract class Action {}''',
    'com/x/data/ActionA.java': '''package com.x.data;
public class ActionA extends Action { int v; }''',
    # packets
    'com/x/net/BasePacket.java': '''package com.x.net;
public abstract class BasePacket { protected int opcode; }''',
    'com/x/net/Connection.java': '''package com.x.net;
public abstract class Connection {}''',
    'com/x/net/SM_INFO.java': '''package com.x.net;
import com.x.world.Npc;
public class SM_INFO extends BasePacket { private final Npc npc; private final int count; public SM_INFO(Npc npc) { this.npc = npc; count = 1; } }''',
    # shared object model
    'com/x/world/VisibleObject.java': '''package com.x.world;
public abstract class VisibleObject {
    private int x;
    public void setX(int x) { this.x = x; }
    public boolean equals(Object o) { return o == this; }
    public int hashCode() { return x; }
}''',
    'com/x/world/Npc.java': '''package com.x.world;
public class Npc extends VisibleObject {
    private Position position;
    public synchronized void move() { synchronized (this) { x(); } lock.lock(); }
    private final java.util.concurrent.locks.ReentrantLock lock = new java.util.concurrent.locks.ReentrantLock();
    void x() {}
}''',
    'com/x/world/Gatherable.java': '''package com.x.world;
public class Gatherable extends VisibleObject { private int count; }''',
    'com/x/world/Position.java': '''package com.x.world;
public class Position { private final float x; private final String map; public Position(float x, String map) { this.x = x; this.map = map; } }''',
    'com/x/world/World.java': '''package com.x.world;
import java.util.*;
public class World {
    private static final World instance = new World();
    private final Map<Integer, VisibleObject> objects = new HashMap<>();
    private final List<Listener> listeners = new ArrayList<>();
    public static World getInstance() { return instance; }
}''',
    'com/x/world/Listener.java': '''package com.x.world;
public interface Listener { void onEvent(); }''',
    'com/x/world/LoggingListener.java': '''package com.x.world;
public class LoggingListener implements Listener { private int calls; public void onEvent() { calls++; } }''',
    'com/x/world/Key.java': '''package com.x.world;
public record Key(int id, String name) {}''',
    # confined
    'com/x/stats/Stat.java': '''package com.x.stats;
public class Stat { private int base; private int bonus; public Stat(int b) { base = b; } public void add(int v) { bonus += v; } public int get() { return base + bonus; } }''',
    'com/x/stats/AddStat.java': '''package com.x.stats;
public class AddStat extends Stat { public AddStat() { super(1); } }''',
    'com/x/stats/Calc.java': '''package com.x.stats;
public class Calc { public static int calc() { Stat s = new AddStat(); return s.get(); } }''',
    # task capture
    'com/x/util/ThreadPoolManager.java': '''package com.x.util;
public class ThreadPoolManager {
    private static final ThreadPoolManager instance = new ThreadPoolManager();
    public static ThreadPoolManager getInstance() { return instance; }
    public java.util.concurrent.Future<?> schedule(Runnable r, long delay) { return null; }
}''',
    'com/x/quest/QuestEnv.java': '''package com.x.quest;
public class QuestEnv { private int step; public void next() { step++; } }''',
    'com/x/quest/Handler.java': '''package com.x.quest;
import com.x.util.ThreadPoolManager;
public class Handler {
    public void onTalk() {
        QuestEnv env = new QuestEnv();
        ThreadPoolManager.getInstance().schedule(() -> System.out.println(env), 100);
    }
}''',
    'com/x/enums/Race.java': '''package com.x.enums;
public enum Race { ELYOS, ASMODIANS; private final int id = 0; }''',
    'com/x/enums/MutableEnum.java': '''package com.x.enums;
public enum MutableEnum { A; private int counter; public void inc() { counter++; } }''',
}


class KindsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fm = fieldmap.build_from_sources(FILES, CONFIG)

    def kind(self, cid):
        return self.fm.classes[cid].kind

    def test_k1_heuristic_closure(self):
        self.assertEqual(self.kind('com.x.data.ItemData'), 'K1')
        self.assertEqual(self.kind('com.x.data.ItemTemplate'), 'K1')  # bound field element type
        self.assertEqual(self.kind('com.x.data.TemplateBase'), 'K1')  # superclass of static data
        self.assertEqual(self.kind('com.x.data.ActionA'), 'K1')  # @XmlElements type=
        self.assertIn('bound field', self.fm.classes['com.x.data.ItemTemplate'].kind_reason)

    def test_k1_from_staticdata_classes(self):
        fm = fieldmap.build_from_sources(FILES, CONFIG, staticdata={'com.x.data.ItemData': {'items'}})
        self.assertEqual(fm.classes['com.x.data.ItemData'].kind, 'K1')
        self.assertNotEqual(fm.classes['com.x.data.ItemTemplate'].kind, 'K1')
        items = next(f for f in fm.classes['com.x.data.ItemData'].fields if f.name == 'items')
        self.assertTrue(items.cpp.startswith('mutable Field<'))

    def test_k1_non_bound_member_escapes(self):
        self.assertEqual(self.kind('com.x.world.Npc'), 'K4')

    def test_k2_packets(self):
        self.assertEqual(self.kind('com.x.net.SM_INFO'), 'K2')
        npc = next(f for f in self.fm.classes['com.x.net.SM_INFO'].fields if f.name == 'npc')
        self.assertEqual(npc.cpp, 'Ref<Npc>')
        self.assertEqual(npc.rule, 'packet member')

    def test_k3_immutable_escaping(self):
        self.assertEqual(self.kind('com.x.world.Position'), 'K3')
        self.assertEqual(self.kind('com.x.world.Key'), 'K5')  # record never stored

    def test_k4_class_tree(self):
        # VisibleObject escapes through World.objects; Gatherable shares the class tree
        self.assertEqual(self.kind('com.x.world.VisibleObject'), 'K4')
        self.assertEqual(self.kind('com.x.world.Gatherable'), 'K4')
        self.assertIn('same class tree', self.fm.classes['com.x.world.Gatherable'].kind_reason)

    def test_interface_escape_marks_implementors(self):
        self.assertEqual(self.kind('com.x.world.LoggingListener'), 'K4')
        self.assertIn('implements com.x.world.Listener', self.fm.classes['com.x.world.LoggingListener'].kind_reason)

    def test_k5_confined(self):
        self.assertEqual(self.kind('com.x.stats.Stat'), 'K5')
        self.assertEqual(self.kind('com.x.stats.AddStat'), 'K5')
        base = next(f for f in self.fm.classes['com.x.stats.Stat'].fields if f.name == 'base')
        self.assertEqual(base.cpp, 'int32_t')
        self.assertEqual(base.rule, 'confined member')

    def test_task_capture_escapes(self):
        self.assertEqual(self.kind('com.x.quest.QuestEnv'), 'K4')
        self.assertIn('captured by lambda', self.fm.classes['com.x.quest.QuestEnv'].kind_reason)

    def test_enums(self):
        self.assertEqual(self.kind('com.x.enums.Race'), 'K3')
        self.assertEqual(self.kind('com.x.enums.MutableEnum'), 'K4')

    def test_singleton(self):
        self.assertTrue(self.fm.classes['com.x.world.World'].singleton)
        inst = next(f for f in self.fm.classes['com.x.world.World'].fields if f.name == 'instance')
        self.assertEqual(inst.cpp, 'static World& getInstance()')
        self.assertEqual(self.fm.base_of(self.fm.classes['com.x.world.World']), 'Immortal')

    def test_singleton_detection_and_named_constants(self):
        extra = {
            'com/x/world/Filter.java': '''package com.x.world;
public class Filter {
    public static final Filter ELYOS = new Filter(1), ASMODIANS = new Filter(2);
    private final int mask;
    private Filter(int mask) { this.mask = mask; }
    public static Filter of(int m) { return new Filter(m); }
}''',
            'com/x/world/Script.java': '''package com.x.world;
public class Script {
    public static final Script EMPTY = new Script("");
    private final String code;
    public Script(String code) { this.code = code; }
}''',
            'com/x/world/Scripts.java': '''package com.x.world;
import java.util.*;
public class Scripts { private final List<Script> scripts = new ArrayList<>(); public void add(String c) { scripts.add(new Script(c)); } }''',
            'com/x/world/Engine.java': '''package com.x.world;
public class Engine {
    private Engine() {}
    public static Engine getInstance() { return SingletonHolder.instance; }
    private static class SingletonHolder { protected static final Engine instance = new Engine(); }
}''',
            'com/x/world/Cron.java': '''package com.x.world;
public class Cron {
    private static Cron instance;
    public static Cron getInstance() { return instance; }
    public static synchronized void initialize() { instance = new Cron(); }
}''',
            'com/x/world/ZoneName.java': '''package com.x.world;
import java.util.*;
public class ZoneName {
    private static final Map<String, ZoneName> names = new HashMap<>();
    public static final ZoneName NONE = get("NONE");
    private final String name;
    private ZoneName(String name) { this.name = name; }
    public static ZoneName get(String n) { return names.computeIfAbsent(n, k -> new ZoneName(k)); }
}''',
            'com/x/world/Tag.java': '''package com.x.world;
import java.util.*;
public class Tag {
    private static final Map<String, Tag> tags = new HashMap<>();
    public static final Tag NONE = new Tag("NONE");
    private final String name;
    private Tag(String name) { this.name = name; }
    public static Tag get(String n) { return tags.computeIfAbsent(n, Tag::new); }
}''',
            'com/x/world/Rift.java': '''package com.x.world;
public class Rift {
    public static Rift getInstance() { return RiftHolder.INSTANCE; }
    private static class RiftHolder { private static final Rift INSTANCE = new Rift(); }
}''',
            'com/x/world/Raid.java': '''package com.x.world;
public class Raid {
    public static Raid getInstance() { return SingletonHolder.instance; }
    private static class SingletonHolder { protected static final Raid instance = new Raid(); }
}''',
            'com/x/world/BaseTask.java': '''package com.x.world;
public abstract class BaseTask { private int runs; public void run() { runs++; } }''',
            'com/x/world/MoveTask.java': '''package com.x.world;
public class MoveTask extends BaseTask {
    private static final MoveTask instance = new MoveTask();
    public static MoveTask getInstance() { return instance; }
    private static final java.util.List<BaseTask> all = new java.util.ArrayList<>();
}''',
            'com/x/world/Zone.java': '''package com.x.world;
public class Zone { private final ZoneName name; private ZoneName last; private final Script script; public Zone(ZoneName n, Script s) { name = n; script = s; } public void set(ZoneName n) { last = n; } }''',
            'com/x/world/Zones.java': '''package com.x.world;
import java.util.*;
public class Zones { private static final List<Zone> zones = new ArrayList<>(); private static final Filter FILTER = Filter.ELYOS; private static Engine engine; }''',
        }
        cfg = CONFIG.replace('connection_bases', 'per_run_services = ["com.x.world.Raid"]\nconnection_bases') + \
            '\n[immortal]\n"com.x.world.ZoneName" = { reason = "interned in a static map" }\n'
        fm = fieldmap.build_from_sources({**FILES, **extra}, cfg)
        cls = fm.classes
        self.assertTrue(cls['com.x.world.Engine'].singleton)                        # SingletonHolder
        self.assertEqual(fm.base_of(cls['com.x.world.Engine']), 'Immortal')
        holder = next(f for f in cls['com.x.world.Engine.SingletonHolder'].fields if f.name == 'instance')
        self.assertEqual(holder.cpp, 'static Engine& getInstance()')
        self.assertTrue(cls['com.x.world.Rift'].singleton)                          # any nested *Holder (RiftServiceHolder, NewSingletonHolder)
        self.assertEqual(fm.base_of(cls['com.x.world.Rift']), 'Immortal')
        self.assertTrue(cls['com.x.world.Cron'].singleton)                          # assigned in initialize()
        self.assertFalse(cls['com.x.world.Filter'].singleton)                       # two constants, of() creates more
        self.assertFalse(cls['com.x.world.Script'].singleton)                       # created by Scripts
        self.assertNotEqual(fm.base_of(cls['com.x.world.Script']), 'Immortal')
        filters = {f.name: f.cpp for f in cls['com.x.world.Filter'].fields}
        self.assertEqual(filters['ELYOS'], 'static inline const Ref<Filter>')
        self.assertEqual(filters['ASMODIANS'], 'static inline const Ref<Filter>')
        self.assertEqual({f.name: f.cpp for f in cls['com.x.world.Script'].fields}['EMPTY'], 'static inline const Ref<Script>')
        self.assertFalse(cls['com.x.world.ZoneName'].singleton)
        self.assertEqual(fm.base_of(cls['com.x.world.ZoneName']), 'Immortal')
        self.assertEqual({f.name: f.cpp for f in cls['com.x.world.ZoneName'].fields}['NONE'], 'static const ZoneName* const')  # hub-headers.md §11.1
        zone = {f.name: f for f in cls['com.x.world.Zone'].fields}
        self.assertEqual((zone['name'].cpp, zone['last'].cpp), ('const ZoneName*', 'Field<const ZoneName*>'))
        self.assertEqual((zone['name'].retains, zone['script'].retains), ([], ['com.x.world.Script']))
        self.assertEqual(fm.class_json(cls['com.x.world.ZoneName'])['immortal'], True)
        self.assertFalse(cls['com.x.world.Tag'].singleton)                          # Tag::new creates more
        self.assertFalse(cls['com.x.world.Raid'].singleton)                         # [settings] per_run_services
        self.assertTrue(cls['com.x.world.MoveTask'].singleton)
        self.assertIsNone(fm.base_of(cls['com.x.world.MoveTask']))                  # BaseTask's base; one base per class tree
        self.assertEqual(fm.class_json(cls['com.x.world.MoveTask'])['singleton'], True)
        with self.assertRaises(fieldmap.FieldmapError):
            fieldmap.build_from_sources(FILES, CONFIG + '\n[immortal]\n"com.x.Nope" = { reason = "x" }\n')

    def test_kind_override(self):
        cfg = CONFIG + '\n[kinds]\n"com.x.stats.Stat" = { kind = "K4", reason = "stored by a later chunk" }\n'
        fm = fieldmap.build_from_sources(FILES, cfg)
        self.assertEqual(fm.classes['com.x.stats.Stat'].kind, 'K4')
        self.assertEqual(fm.classes['com.x.stats.AddStat'].kind, 'K4')
        self.assertIn('fieldmap.toml', fm.classes['com.x.stats.Stat'].kind_reason)

    def test_equals_overrides(self):
        info = self.fm.overrides_info(self.fm.classes['com.x.world.Npc'])
        self.assertEqual(info['equals'], 'com.x.world.VisibleObject')
        self.assertEqual(info['hashCode'], 'com.x.world.VisibleObject')
        self.assertIn('equals', self.fm.overrides_info(self.fm.classes['com.x.world.Key']))
        self.assertNotIn('equals', self.fm.overrides_info(self.fm.classes['com.x.world.Position']))
        self.assertFalse(self.fm.class_json(self.fm.classes['com.x.world.Position'])['hasEquals'])

    def test_sync_counts(self):
        self.assertEqual(self.fm.sync_counts['com.x.world.Npc']['move'], {'synchronized': 2, 'lock': 1})

    def test_escape_report(self):
        text = self.fm.escape_report_text()
        self.assertIn('## K5 CONFINED', text)
        self.assertIn('`com.x.stats.Stat`', text)


class ConfigTest(unittest.TestCase):
    def test_bad_sections_and_entries(self):
        with self.assertRaises(fieldmap.FieldmapError):
            fieldmap.build_from_sources(FILES, '[bogus]\n')
        with self.assertRaises(fieldmap.FieldmapError):
            fieldmap.build_from_sources(FILES, '[kinds]\n"com.x.stats.Stat" = { kind = "K9", reason = "x" }\n')
        with self.assertRaises(fieldmap.FieldmapError):
            fieldmap.build_from_sources(FILES, '[kinds]\n"com.x.Missing" = { kind = "K4", reason = "x" }\n')
        with self.assertRaises(fieldmap.FieldmapError):
            fieldmap.build_from_sources(FILES, '[fields]\n"com.x.stats.Stat.base" = { cpp = "int" }\n')
        with self.assertRaises(fieldmap.FieldmapError):
            fieldmap.build_from_sources(FILES, '[fields]\n"com.x.stats.Stat.nope" = { cpp = "int", reason = "x" }\n')
        with self.assertRaises(fieldmap.FieldmapError):
            fieldmap.build_from_sources(FILES, CONFIG, cycles_text='[resolutions]\n"a.B.c" = "maybe later"\n')

    def test_staticdata_shapes(self):
        import json
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            p = os.path.join(tmp, 's.json')
            for doc in (['a.B'], {'classes': ['a.B']}, {'classes': [{'fqn': 'a.B', 'runtimeMutable': ['z']}]}, {'classes': {'a.B': {'runtime_mutable': ['z']}}}):
                with open(p, 'w', encoding='utf-8') as f:
                    json.dump(doc, f)
                self.assertIn('a.B', fieldmap.load_staticdata_classes(p))
            with open(p, 'w', encoding='utf-8') as f:
                json.dump({'classes': [42]}, f)
            with self.assertRaises(fieldmap.FieldmapError):
                fieldmap.load_staticdata_classes(p)


if __name__ == '__main__':
    unittest.main()
