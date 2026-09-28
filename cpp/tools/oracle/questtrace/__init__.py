"""Phase-6 golden quest traces (docs/design/phase6-inventory.md §7.6 item 3, phase6-questgen-prototype.md §8.3): every return leaf of
every hook of a Java quest handler as a case (the inputs its guards read) with the expected effects in order, into expected/quest/<id>.json.
Written from the Java handlers and AbstractQuestHandler/QuestService only; nothing here reads the C++ tree or runs the quest generator
(tools/gen/questgen) - it reuses only that package's Java parser (jast) over tools/gen/javasrc's tokens.
"""
