"""api: the vocabulary the transliterator accepts, and what the C++ side declares for it today.

Two layers, as docs/design/phase6-inventory.md §7.1 describes them:

- CORE: the quest vocabulary of tier A. The AbstractQuestHandler helpers (without the spawn and follow helpers), QuestEnv, QuestState,
  QuestVars and the quest state list, QuestService start/finish/collect/abandon, the `qe.register*` / `addOn*` registration calls, the
  simple getters (object and npc ids, level, race, class, inventory counts, item ids), HandlerResult, ZoneName.get, and
  PacketSendUtility.sendPacket with SM_DIALOG_WINDOW (the close-dialog idiom). Keyed by the C++ class that declares the member (a call on
  a Player to getObjectId is AionObject.getObjectId).
- API_TABLE: the tier-B rows, ported or declared non-quest APIs with a receiver kind: teleports, the spawn and follow helpers, timers,
  kinah and item counts, zone checks, crafting checks, instance creation, flight state, skills, system messages, enum companions.

Everything else is refused as "api-missing: Class.method". A member the table allows but the C++ headers do not declare is still
transliterated when PLANNED gives its C++ spelling (HandlerResult.fromBoolean: m5d-plan H-06); the report lists such files as blocked on a
declaration. The C++ side is read from the headers (cppdecl) and the body status of each declaration from tools/porting/census.py.
"""
from __future__ import annotations

import re
from dataclasses import dataclass
from functools import cached_property

from . import cppdecl, paths

import dialogaction  # noqa: E402

G = 'aion/gameserver/'

# Every header whose classes, enums or companion functions a quest handler may reach. Base classes are listed too (a call on Player finds
# AionObject::getObjectId through them).
HEADERS = [G + h for h in '''
questEngine/handlers/AbstractQuestHandler.h questEngine/handlers/HandlerResult.h questEngine/QuestEngine.h
questEngine/model/QuestEnv.h questEngine/model/QuestState.h questEngine/model/QuestVars.h questEngine/model/QuestStatus.h
questEngine/model/QuestActionType.h
model/templates/quest/QuestNpc.h model/templates/quest/QuestItems.h
model/gameobjects/AionObject.h model/gameobjects/VisibleObject.h model/gameobjects/Creature.h model/gameobjects/Npc.h
model/gameobjects/player/Player.h model/gameobjects/player/QuestStateList.h model/gameobjects/player/PlayerCommonData.h
model/gameobjects/player/Equipment.h model/gameobjects/Item.h
model/items/storage/IStorage.h model/items/storage/Storage.h model/templates/item/ItemTemplate.h model/templates/VisibleObjectTemplate.h
model/templates/npc/NpcTemplate.h model/animations/TeleportAnimation.h
model/gameobjects/player/RecipeList.h model/skill/SkillList.h model/skill/PlayerSkillList.h controllers/movement/CreatureMoveController.h
controllers/movement/PlayableMoveController.h controllers/movement/PlayerMoveController.h controllers/effect/EffectController.h
controllers/effect/PlayerEffectController.h
model/Race.h model/PlayerClass.h model/PlayerClassInfo.h model/DialogPage.h model/DialogPageInfo.h model/EmotionId.h model/EmotionType.h
model/Gender.h model/gameobjects/state/CreatureState.h
services/QuestService.h services/teleport/TeleportService.h services/craft/CraftSkillUpdateService.h services/instance/InstanceService.h
services/item/ItemService.h services/item/ItemPacketService_ItemAddType.h services/item/ItemPacketService_ItemUpdateType.h
services/event/EventService.h
controllers/VisibleObjectController.h controllers/CreatureController.h controllers/NpcController.h controllers/PlayerController.h
skillengine/SkillEngine.h
utils/PacketSendUtility.h utils/PositionUtil.h
network/aion/AionServerPacket.h network/aion/serverpackets/SM_DIALOG_WINDOW.h network/aion/serverpackets/SM_SYSTEM_MESSAGE.h
network/aion/serverpackets/SM_PLAY_MOVIE.h network/aion/serverpackets/SM_EMOTION.h network/aion/serverpackets/SM_QUEST_ACTION.h
network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h network/aion/serverpackets/SM_USE_OBJECT.h
world/zone/ZoneName.h world/WorldMapType.h world/WorldMapTypeInfo.h world/WorldMapInstance.h world/WorldPosition.h
utils/stats/AbyssRankEnum.h model/templates/rewards/BonusType.h
model/house/House.h model/templates/spawns/SpawnSearchResult.h instance/handlers/InstanceHandler.h
'''.split()] + ['aion/commons/utils/Rnd.h']
# (the last line: types QuestPrelude.h re-exports that handlers declare locals of; without them such a local is refused as `type`, which
# hides the API gap behind it: DataManager.SPAWNS_DATA, WorldMapInstance.getInstanceHandler, Player.getActiveHouse)

# AbstractQuestHandler members that are not tier A (S2's classifier, phase6-inventory.md §7.1): they are API_TABLE rows
SPAWN_HELPERS = frozenset('spawn spawnInFrontOf spawnForFiveMinutesInFrontOf spawnForFiveMinutesInFront spawnForFiveMinutes '
                          'spawnTemporarily'.split())
FOLLOW_HELPERS = frozenset(['defaultStartFollowEvent', 'defaultFollowEndEvent'])

CORE = {
    'QuestEnv': {'getPlayer', 'getDialogActionId', 'getTargetId', 'getVisibleObject', 'getQuestId', 'setQuestId', 'getExtendedRewardIndex',
                 'setDialogActionId', 'isDialogContinuationFromPreQuest', 'setExtendedRewardIndex'},
    'QuestState': {'getQuestVarById', 'setQuestVarById', 'setQuestVar', 'getStatus', 'setStatus', 'isStartable', 'getQuestVars',
                   'getRewardGroup', 'setRewardGroup', 'getCompleteCount', 'canRepeat', 'getFlags', 'setFlags', 'getQuestId'},
    'QuestVars': {'getVarById', 'setVarById', 'getQuestVars', 'setVar'},
    'QuestStateList': {'getQuestState', 'hasQuest'},
    'QuestEngine': set('''addHandlerSideQuestDrop registerQuestNpc registerQuestItem registerQuestHouseItem registerOnGetItem
registerOnLevelChanged registerOnQuestCompleted registerOnEnterWorld registerOnDie registerOnLogOut registerOnEnterZone registerOnKillInZone
registerOnLeaveZone registerOnKillRanked registerOnKillInWorld registerOnPassFlyingRings registerOnQuestTimerEnd registerOnInvisibleTimerEnd
registerQuestSkill registerOnFailCraft registerOnEquipItem registerCanAct registerOnDredgionReward registerOnBonusApply
registerAddOnReachTargetEvent registerAddOnLostTargetEvent registerOnEnterWindStream registerOnRide'''.split()),
    'QuestNpc': {'addOnQuestStart', 'addOnAttackEvent', 'addOnKillEvent', 'addOnTalkEvent', 'addOnAddAggroListEvent', 'addOnAtDistanceEvent'},
    'QuestService': {'startQuest', 'finishQuest', 'collectItemCheck', 'abandonQuest'},
    'Player': {'getQuestStateList', 'getInventory', 'getLevel', 'getRace', 'getCommonData', 'getPlayerClass', 'isMentor', 'getGender',
               'getName'},
    'PlayerCommonData': {'getLevel', 'getRace', 'getPlayerClass', 'getGender', 'getName'},
    'Creature': {'getLevel', 'getRace'},
    'VisibleObject': {'getWorldId', 'getName'},
    'AionObject': {'getObjectId', 'getName'},
    'Npc': {'getNpcId'},
    'Item': {'getItemId', 'getItemTemplate', 'getItemCount', 'getObjectId'},
    'ItemTemplate': {'getTemplateId'},
    'VisibleObjectTemplate': {'getTemplateId'},
    'Storage': {'getItemCountByItemId'},
    'HandlerResult': {'fromBoolean'},
    'ZoneName': {'get'},
    'PacketSendUtility': {'sendPacket'},
    'SM_DIALOG_WINDOW': {'<init>'},
}


@dataclass(frozen=True)
class Row:
    id: str
    title: str
    members: tuple      # ((class, member), ...) - class: the C++ class declaring it; '<init>' for a constructor; '*' for any
    gate: str           # which milestone brings the bodies (phase6-inventory.md §4, §8)


API_TABLE = (
    Row('B01', 'teleports: TeleportService.teleportTo (worldId, instance and position overloads)',
        (('TeleportService', 'teleportTo'),), 'ported worldId overloads; delegations M5f (R5)'),
    Row('B02', 'spawn helpers of AbstractQuestHandler', tuple(('AbstractQuestHandler', n) for n in sorted(SPAWN_HELPERS)), 'M5d H-05'),
    Row('B03', 'follow helpers of AbstractQuestHandler (escorts)', tuple(('AbstractQuestHandler', n) for n in sorted(FOLLOW_HELPERS)),
        'M5d stage 3 E-07'),
    Row('B04', 'quest timers: QuestService.questTimerStart/End, invisibleTimerStart',
        (('QuestService', 'questTimerStart'), ('QuestService', 'questTimerEnd'), ('QuestService', 'invisibleTimerStart')), 'M5d E-05'),
    Row('B05', 'event quests: QuestService.startEventQuest', (('QuestService', 'startEventQuest'),),
        'M5d E-02 (m5d-plan.md:541; the event content itself is M5i)'),
    Row('B06', 'kinah and item removal on the inventory',
        (('Storage', 'getKinah'), ('Storage', 'tryDecreaseKinah'), ('Storage', 'decreaseKinah'), ('Storage', 'decreaseByItemId'),
         ('Storage', 'decreaseByObjectId'), ('Storage', 'isFull'), ('Storage', 'getFreeSlots'), ('Storage', 'getItemsByItemId')),
        'M5b-3'),
    Row('B07', 'item grants: ItemService.addItem', (('ItemService', 'addItem'),), 'M5b-3'),
    Row('B08', 'zone checks: Creature.isInsideZone, Player.isInsideItemUseZone',
        (('Creature', 'isInsideZone'), ('Player', 'isInsideItemUseZone'), ('Creature', 'isInsideItemUseZone'),
         ('VisibleObject', 'isInsideZone')), 'ported'),
    Row('B09', 'positions: getPosition, getWorldMapInstance, getX/Y/Z, getHeading, getInstanceId, getMapId',
        (('VisibleObject', 'getPosition'), ('VisibleObject', 'getWorldMapInstance'), ('VisibleObject', 'getX'), ('VisibleObject', 'getY'),
         ('VisibleObject', 'getZ'), ('VisibleObject', 'getHeading'), ('VisibleObject', 'getInstanceId'), ('WorldPosition', 'getMapId'),
         ('WorldPosition', 'getX'), ('WorldPosition', 'getY'), ('WorldPosition', 'getZ'), ('WorldPosition', 'getHeading'),
         ('WorldPosition', 'getInstanceId'), ('WorldPosition', 'getWorldMapInstance'), ('WorldMapInstance', 'getInstanceId'),
         ('WorldMapInstance', 'getMapId')), 'ported'),
    Row('B10', 'distances: PositionUtil.isInRange, getDistance', (('PositionUtil', 'isInRange'), ('PositionUtil', 'getDistance')), 'ported'),
    Row('B11', 'crafting: CraftSkillUpdateService.canLearnMore*CraftingSkill, recipe and skill list checks',
        (('CraftSkillUpdateService', 'getInstance'), ('CraftSkillUpdateService', 'canLearnMoreExpertCraftingSkill'),
         ('CraftSkillUpdateService', 'canLearnMoreMasterCraftingSkill'), ('Player', 'getRecipeList'), ('RecipeList', 'isRecipePresent'),
         ('Player', 'getSkillList'), ('PlayerSkillList', 'addSkill'), ('SkillList', 'getSkillLevel'), ('PlayerSkillList', 'getSkillLevel'),
         ('SkillList', 'isSkillPresent'), ('PlayerSkillList', 'isSkillPresent')), 'M5c'),
    Row('B12', 'instance creation: InstanceService.getNextAvailableInstance, registerPlayerWithInstance',
        (('InstanceService', 'getNextAvailableInstance'), ('InstanceService', 'registerPlayerWithInstance'),
         ('InstanceService', 'registerGroupWithInstance')), 'M5f'),
    Row('B13', 'npc despawn and death: getController().delete / deleteAndScheduleRespawn / die',
        (('Npc', 'getController'), ('VisibleObject', 'getController'), ('Creature', 'getController'),
         ('VisibleObjectController', 'delete_'), ('VisibleObjectController', 'deleteAndScheduleRespawn'), ('CreatureController', 'die')),
        'ported'),
    Row('B14', 'flight: Creature.setState/unsetState/isInState, Player.setFlightTeleportId, setFlightDistance',
        (('Creature', 'setState'), ('Creature', 'unsetState'), ('Creature', 'isInState'), ('Player', 'setFlightTeleportId'),
         ('Player', 'setFlightDistance')), 'ported'),
    Row('B15', 'skills and effects: SkillEngine.getInstance().applyEffectDirectly, getEffectController().hasAbnormalEffect/removeEffect',
        (('SkillEngine', 'getInstance'), ('SkillEngine', 'applyEffectDirectly'), ('Creature', 'getEffectController'),
         ('EffectController', 'hasAbnormalEffect'), ('EffectController', 'removeEffect')), 'M5b-2'),
    Row('B16', 'broadcasts: PacketSendUtility.broadcastPacket, sendMonologue',
        (('PacketSendUtility', 'broadcastPacket'), ('PacketSendUtility', 'sendMonologue')), 'ported'),
    Row('B17', 'system messages: SM_SYSTEM_MESSAGE.STR_* factories', (('SM_SYSTEM_MESSAGE', '*'),), 'generated (sysmsg.py)'),
    Row('B18', 'packets built in handlers: SM_PLAY_MOVIE, SM_EMOTION, SM_QUEST_ACTION, SM_ITEM_USAGE_ANIMATION, SM_USE_OBJECT',
        (('SM_PLAY_MOVIE', '<init>'), ('SM_EMOTION', '<init>'), ('SM_QUEST_ACTION', '<init>'), ('SM_ITEM_USAGE_ANIMATION', '<init>'),
         ('SM_USE_OBJECT', '<init>')), 'ported'),
    Row('B19', 'map ids: WorldMapType.X.getId()', (('WorldMapType', 'getId'),), 'ported (WorldMapTypeInfo.h)'),
    Row('B20', 'classes: PlayerClass.getStartingClass, isStartingClass', (('PlayerClass', 'getStartingClass'), ('PlayerClass', 'isStartingClass')),
        'ported (PlayerClassInfo.h)'),
    Row('B21', 'reward pages: DialogPage.X.id(), DialogPage.getRewardPageByIndex', (('DialogPage', 'id'), ('DialogPage', 'getRewardPageByIndex')),
        'ported (DialogPageInfo.h)'),
    Row('B22', 'random numbers: Rnd.get, chance, nextBoolean (a namespace of free functions in C++)',
        (('Rnd', 'get'), ('Rnd', 'chance'), ('Rnd', 'nextBoolean'), ('Rnd', 'nextInt')), 'ported (commons)'),
    Row('B23', 'equipment checks: Player.getEquipment, Equipment.getEquippedItemsByItemId, itemSetPartsEquipped',
        (('Player', 'getEquipment'), ('Equipment', 'getEquippedItemsByItemId'), ('Equipment', 'itemSetPartsEquipped')), 'ported'),
    Row('B24', 'npc state: isDead, getTarget, isSpawned, getMoveController().abortMove',
        (('Creature', 'isDead'), ('VisibleObject', 'getTarget'), ('Creature', 'getTarget'), ('VisibleObject', 'isSpawned'),
         ('Creature', 'getMoveController'), ('Player', 'getMoveController'), ('CreatureMoveController', 'abortMove')), 'ported'),
    Row('B25', 'divine power: PlayerCommonData.getDp/setDp', (('PlayerCommonData', 'getDp'), ('PlayerCommonData', 'setDp')), 'ported'),
)

# Members the table allows although no C++ header declares them yet: their C++ spelling (a callable, `{args}` filled in), the header that
# will declare them, the return type (C++ text) and the owner that adds the declaration.
@dataclass(frozen=True)
class Planned:
    cpp: str
    header: str
    ret: str
    owner: str


PLANNED = {
    ('HandlerResult', 'fromBoolean'): Planned('::aion::gameserver::questEngine::handlers::fromBoolean({args})',
                                              G + 'questEngine/handlers/HandlerResultInfo.h', 'HandlerResult',
                                              'm5d-plan.md H-06 (the HandlerResult companion, a new file)'),
}

# Java classes whose static members a handler may name, mapped to the C++ class or enum of the same role
STATIC_CLASSES = {'QuestService', 'TeleportService', 'PacketSendUtility', 'PositionUtil', 'ItemService', 'InstanceService', 'SkillEngine',
                  'CraftSkillUpdateService', 'ZoneName', 'HandlerResult', 'SM_SYSTEM_MESSAGE', 'Rnd', 'EventService'}
# Java classes of static methods that C++ ports as a namespace of free functions
STATIC_NAMESPACES = {'Rnd': ('aion', 'commons', 'utils', 'Rnd')}
# Java enums the handlers name (the C++ enum class has the same simple name)
ENUMS = {'QuestStatus', 'Race', 'PlayerClass', 'DialogPage', 'HandlerResult', 'WorldMapType', 'EmotionId', 'EmotionType', 'CreatureState',
         'QuestActionType', 'AbyssRankEnum', 'BonusType', 'Gender', 'ItemAddType', 'ItemUpdateType', 'TeleportAnimation', 'AIState',
         'TaskId'}
# Java nested type spelling -> C++ simple name
NESTED = {'ItemPacketService.ItemAddType': 'ItemPacketService_ItemAddType', 'ItemAddType': 'ItemPacketService_ItemAddType',
          'ItemPacketService.ItemUpdateType': 'ItemPacketService_ItemUpdateType', 'ItemUpdateType': 'ItemPacketService_ItemUpdateType'}


class Api:
    """The vocabulary, the header index and the prelude, loaded once."""

    def __init__(self, headers=HEADERS, java_dir=None):
        self.index = cppdecl.HeaderIndex(tuple(paths.CPP_INCLUDE_ROOTS) + (paths.CPP_ROOT / 'commons' / 'src',))
        for h in headers:
            self.index.scan(h)
        self.java_dir = java_dir or paths.JAVA_GAME_SERVER
        self.row_of = {}
        for r in API_TABLE:
            for m in r.members:
                self.row_of[m] = r

    # -- the prelude ----------------------------------------------------------------------------------------------------------------
    @cached_property
    def prelude_names(self):
        """simple names the quest prelude re-exports (`using ::aion::gameserver::x::Name;`) -> qualified name"""
        text = self.index.read(paths.QUEST_PRELUDE) or ''
        return {m.group(2): m.group(1) + m.group(2) for m in re.finditer(r'^using\s+::([\w:]+::)(\w+);', text, re.M)}

    @cached_property
    def prelude_closure(self):
        return self.index.include_closure(paths.QUEST_PRELUDE)

    @cached_property
    def dialog_actions(self):
        """Java DialogAction constant -> C++ spelling (the prelude's using-directive brings them in)"""
        _cu, consts, _doc = dialogaction.parse(str(self.java_dir))
        return {name: dialogaction.cpp_identifier(name) for name, _value, _c, _i in consts}

    # -- lookups --------------------------------------------------------------------------------------------------------------------
    def tier(self, owner, name, _seen=None):
        """'core', a Row, or None when (owner, name) is outside the vocabulary. A member of a derived class that narrows a base member
        the table allows (Player::getController narrows Creature::getController) is allowed with the base's row."""
        if name in CORE.get(owner, ()):
            return 'core'
        if owner == 'AbstractQuestHandler' and name not in SPAWN_HELPERS and name not in FOLLOW_HELPERS:
            return 'core'
        r = self.row_of.get((owner, name)) or self.row_of.get((owner, '*'))
        if r is not None:
            return r
        c = self.index.classes.get(owner)
        if c is None:
            return None
        _seen = _seen or {owner}
        for b in c.bases:
            bn = b.rpartition('::')[2]
            if bn not in _seen:
                _seen.add(bn)
                t = self.tier(bn, name, _seen)
                if t is not None:
                    return t
        return None

    def cpp_name(self, name):
        """C++ spelling of a class or enum simple name in a quest handler file: the bare name when the prelude re-exports it, else fully
        qualified (the handler namespaces ai/instance/zone would shadow a relative `instance::...`)"""
        if name in self.prelude_names:
            return name
        c = self.index.classes.get(name)
        if c is not None:
            return '::' + '::'.join(c.qual)
        e = self.index.enums.get(name)
        if e is not None:
            return '::' + '::'.join(e.qual)
        return name

    def header_of(self, name):
        c = self.index.classes.get(name)
        if c is not None:
            return c.header
        e = self.index.enums.get(name)
        return e.header if e is not None else None

    def needs_include(self, header):
        return header is not None and header not in self.prelude_closure

    def is_enum(self, name):
        return name in self.index.enums

    @cached_property
    def census_status(self):
        """(scope tuple, name) -> [(min arity, max arity, status)] of the scanned classes, from tools/porting/census.py"""
        import census  # noqa: E402  (tools/porting on sys.path)
        files = []
        for h in self.index.scanned:
            for root, prefix in ((paths.CPP_GAME_SERVER / 'src', 'src/'), (paths.CPP_GAME_SERVER / 'generated', 'generated/')):
                if (root / h).is_file():
                    files.append(prefix + h)
                    cpp = h[:-2] + '.cpp'
                    if (root / cpp).is_file():
                        files.append(prefix + cpp)
        idx = census.build_index(sorted(set(files)))
        out = {}
        for scope, names in idx.entities.items():
            for name, ents in names.items():
                out[(scope, name)] = [(e.min_arity, e.max_arity, e.status()) for e in ents]
        return out

    def body_status(self, func, arity):
        """ported | unported | partial | declaredOnly for the census entity of a scanned declaration; 'pure virtual' for an interface
        member (IStorage::tryDecreaseKinah: PlayerStorage implements it), 'commons' for the commons library census does not scan"""
        if func.pure:
            return 'pure virtual'
        if func.header.startswith('aion/commons/'):
            return 'commons'
        if func.header.endswith('.gen.h'):
            return 'generated'
        ents = self.census_status.get((tuple(func.ns), func.name), [])
        hits = [s for lo, hi, s in ents if lo <= arity <= hi] or [s for _lo, _hi, s in ents]
        if not hits:
            return 'declaredOnly' if not func.defined else 'ported'
        for s in ('unported', 'partial', 'declaredOnly'):
            if s in hits:
                return s
        return 'ported'
