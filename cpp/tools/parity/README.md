# tools/parity: structural parity of a Java handler and its C++

The per-file check of phase6-inventory.md §7.6 items 1 and 4 (and phase6-questgen-prototype.md §8.1, §8.4): a generated or hand-ported
C++ handler must keep the literals, calls, operators and constants of its Java source. Python 3.12, standard library only. It reads text
only and imports neither the quest generator (`tools/gen/questgen`) nor anything of the C++ tree, so it checks the generator from outside.
The seed is the prototype review's three scratch checks (ordered literals, operators and called names over the 910 emitted files).

## What is compared

Both sides are tokenized; the Java side from `class` to the end of the file, the C++ side from `class` to the `AION_*_HANDLER` marker.

| Check | What |
|---|---|
| `literals-multiset`, `literals-order` | integer, floating (`float` and `double` kept apart), string, char, boolean and null literals. The ordered check is the one G2's twin tool needs (§7.2): a multiset cannot see two literals swapped between positions |
| `calls-multiset`, `calls-order` | called (and declared) names |
| `companions` | the enum methods whose position moves in C++ (`WorldMapType.X.getId()` is `getId(WorldMapType::X)`), as a multiset |
| `operators-order` | comparison, logical, arithmetic, bitwise, increment and `?` operators |
| `constants-order` | ALL_CAPS names: DialogAction and enum constants, `STR_*` messages, `SM_*` packets |
| `spawn-ids` | the int literal that starts the arguments of every `spawn*` call in the code (comments dropped), as a multiset |
| `spawn-analyzer` | the npc id set `QuestSpawnAnalyzer.loadNpcIdsSpawnedByHandlers` finds: its own regular expression (`QuestSpawnAnalyzer.java:101`) over the whole raw text of each file, so `sp(`, both ids of a ternary and a commented-out `// spawn(216239, ...)` count (the npcIdsSpawnedByHandlers parity of handlers-and-porting-plan.md:291-297) |

Only these documented renames are applied (each undoes one emitter idiom of phase6-questgen-prototype.md §2, or a C++ spelling):

- template argument lists (casts, `runtime::Ptr<T>`, `std::array<T, N>` sizes, Java generics), comments, preprocessor lines and Java
  annotations are dropped;
- `register_`, `delete_`, `NULL_` lose the keyword/macro underscore; `nullptr` and `std::nullopt` are null; `u'c'` is `'c'`; `901.0f` is `901f`;
- `x instanceof T` is `!=` and null (`runtime::as<T>(x) != nullptr`); `a.equals(b)` is `==`; `x op= y` counts as `op` (Java narrows it
  implicitly, the C++ spells out the cast); plain `=` is not compared (a C++ array initializer has none);
- C++ unary `*` and `&` and `T*`/`T&` declarators are not operators (the declarator rule applies to both sides);
- casts and unboxing (`cast`, `as`, `static_cast`, `value`, `intValue`) are not calls; `QuestEnv::create` is `new QuestEnv`;
  `front`/`back`/`empty` are `getFirst`/`getLast`/`isEmpty`; `super(...)` is the `AbstractQuestHandler` constructor; `arr.length` is
  `size()`; `get`, `at` and a Java index that is not an int literal are one name;
- the class of a Java static import is dropped from the C++ qualified name (`SM_SYSTEM_MESSAGE::STR_X`);
- `workItems.getFirst()`/`getLast()` are compared as `workItems.get(0)` / `workItems.get(workItems.size() - 1)`, the C++ spelling of
  questgen's work-items rule;
- hand-port spellings (P6-Q ascension route, integration of 2026-09-29): `push_back` is `add` (java.util.List on a local `std::vector`);
  a Java `new ArrayList<>()` with no argument is not a call (the default-constructed local; with an argument it stays a call); a Java
  anonymous `new Runnable() { ... run() ... }` is a C++ lambda, so its `Runnable` and the `run` it declares are not calls.

Not caught by design: plain identifiers are not compared, so a swapped or substituted operand, local or receiver is at parity (Java
`return var > targetId;` against C++ `return targetId > var;`, `qs.setQuestVarById(0, var + 1)` against
`qs->setQuestVarById(0, targetId + 1)`); a dropped plain assignment without a literal (`x = y;`); and anything a rename folds (a
`T* name` declarator and a `a * b` followed by `)` look alike on both sides). Behaviour is the golden trace's job
(`tools/oracle/questtrace`).

Not implemented yet: the per-line waiver `// parity: <reason>` of handlers-and-porting-plan.md §3.1 item 2. Comments are dropped, so a
hand-ported handler with an owner-approved deviation cannot pass until it exists (phase6-inventory.md, the P6-T row).

## Commands

```
python parity.py pair JAVA CPP [--json]
python parity.py tree --java-dir DIR --cpp-dir DIR [--only REL ...] [--json OUT] [--require-all]
python parity.py spawn-analyzer [--suffix SUFFIX ...] DIR...
```

`tree` compares every `.java` below `--java-dir` with the `.cpp` at the same relative path below `--cpp-dir`; a Java file without its C++
is counted, and is a failure only with `--require-all`. Exit 0 at parity, 1 on a mismatch, 2 on an input error.

`spawn-analyzer` prints the npc id set `QuestSpawnAnalyzer.loadNpcIdsSpawnedByHandlers` finds below the directories, with the same
regular expression as the `spawn-analyzer` check: a first line `# N files, M npc ids`, then the ids in ascending order. It reads the files
whose names end with a `--suffix` (default `.java`, the analyzer's own filter); a missing directory exits 2. It is the oracle of the C++
analyzer's set over the ported handlers (P5-06a's `QuestSpawnAnalyzerTest`, m5d-plan.md E-08: the build-time table of `aion_gs_regscan`
against the pattern over `handlers/aion/gameserver/handlers/{ai,instance,quest}`, `.cpp` and `.h`):

```
python tools/parity/parity.py spawn-analyzer --suffix .cpp --suffix .h game-server/handlers/aion/gameserver/handlers/ai game-server/handlers/aion/gameserver/handlers/instance game-server/handlers/aion/gameserver/handlers/quest
```

The generator's output, from `cpp/`:

```
python -m tools.gen.questgen --dry-run --emit <dir outside the repo>
python tools/parity/parity.py tree --java-dir ../game-server/data/handlers/quest --cpp-dir <dir>/aion/gameserver/handlers/quest
```

On 2026-09-27 (questgen with the P6-T rules): 929 pairs compared, 0 mismatches on every check, `spawn-analyzer` included; 106 Java
files have no C++ (refused).
`tools/gen/tests/test_questgen_p6t.py` runs the same check over every transliterated file.

Tests: `python -m unittest discover -s tests -t .` from this directory (CTest `tools.parity`, registered by `tools/CMakeLists.txt`).
