"""questgen: the phase-6 quest transliterator prototype (G1 of docs/design/phase6-inventory.md §7.1, with G2's mirror-pair report).

Reads every Java quest handler under game-server/data/handlers/quest and either transliterates it into the C++ handler shape of
cpp/game-server/handlers/aion/gameserver/handlers/quest (one .cpp per Java file: the class, its hooks, the AION_QUEST_HANDLER marker) or
refuses it with its reasons: an unsupported construct, a call outside the API table (api.py), or a C++ hook signature that cannot express
what the Java handler does (the read-only bonus reward list). Nothing is compiled.

    python -m tools.gen.questgen --dry-run [--emit DIR] [--only FILE...] [--json OUT] [--markdown OUT]

Modules: paths (repository paths), cppdecl (C++ header declarations), jast (Java statement and expression trees over javasrc tokens),
api (the vocabulary and the ~25-row API table, checked against the C++ headers), emit (the transliterator), mirror (Elyos/Asmodian twins
for G2), cli (the driver and the dry-run report). Design and measured coverage: docs/design/phase6-questgen-prototype.md.
"""
from . import paths  # noqa: F401  (sets sys.path for javasrc, skeleton, dialogaction, census)
