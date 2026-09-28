"""Repository paths of the quest transliterator, and the sys.path entries of the sibling tools it reuses (tools/gen: javasrc, skeleton,
dialogaction; tools/porting: census)."""
from __future__ import annotations

import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
GEN_DIR = HERE.parent                      # cpp/tools/gen
CPP_ROOT = GEN_DIR.parents[1]              # cpp
REPO_ROOT = CPP_ROOT.parent                # the Java tree root (game-server/, cpp/, ...)
PORTING_DIR = CPP_ROOT / 'tools' / 'porting'

JAVA_GAME_SERVER = Path(os.environ.get('AION_JAVA_GAME_SERVER', REPO_ROOT / 'game-server'))
JAVA_QUEST_DIR = JAVA_GAME_SERVER / 'data' / 'handlers' / 'quest'
JAVA_CORE = JAVA_GAME_SERVER / 'src' / 'com' / 'aionemu' / 'gameserver'
QUEST_DATA_XML = JAVA_GAME_SERVER / 'data' / 'static_data' / 'quest_data' / 'quest_data.xml'

CPP_GAME_SERVER = CPP_ROOT / 'game-server'
CPP_INCLUDE_ROOTS = (CPP_GAME_SERVER / 'src', CPP_GAME_SERVER / 'generated', CPP_GAME_SERVER / 'handlers')
QUEST_PRELUDE = 'aion/gameserver/handlers/quest/QuestPrelude.h'
QUEST_HANDLER_NAMESPACE = ('aion', 'gameserver', 'handlers', 'quest')

for _p in (str(GEN_DIR), str(PORTING_DIR)):
    if _p not in sys.path:
        sys.path.insert(0, _p)
sys.dont_write_bytecode = True
