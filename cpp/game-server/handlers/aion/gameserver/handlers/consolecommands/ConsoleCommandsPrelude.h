#pragma once

// Prelude of the console chat command handlers (data/handlers/consolecommands; the three command packages form one library,
// aion_gs_handlers_commands).
// A prelude holds the core names of its category as using-declarations in the category namespace, e.g. `using gameserver::ai::NpcAI;`
// (handlers-and-porting-plan.md §1.2). It is identical for every file of the category, so unity batches may include it any number of times.
// Handler files include it first; it also provides the registration markers and AION_UNPORTED. The handler file rules apply
// (aion_gs_regscan): declarations only inside the namespace of this directory, no namespace-scope `static`, no other namespaces.
// Each command package has its own prelude: a using-declaration here and a command class of the same name in this package (Pet, Event,
// SysMail, WorldRaid) are a compile error in every build, instead of lookup that depends on the unity batch. Such a name is not re-exported;
// the commands spell the core type qualified (test_handler_preludes checks the names against the Java classes of the package).
// Using-declarations (S0b): every core type that the Java files of the category import, when a forward header declares it, its simple
// name comes from one package only and no Java type of the category has that name (nested types are spelled Outer::Inner or
// Outer_Inner). The prelude includes the full headers of the category's handler base classes, the generated headers of the
// re-exported enums, the full hub headers the category's handlers use most (Player, Npc, PacketSendUtility, ...; spine freeze) and the forward
// headers of the rest.

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/configs/main/fwd.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/dataholders/fwd.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/gameobjects/state/fwd.h"
#include "aion/gameserver/model/skill/fwd.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/model/templates/fwd.h"
#include "aion/gameserver/model/templates/item/ItemQuality.h"
#include "aion/gameserver/model/templates/item/enums/EquipType.h"
#include "aion/gameserver/model/templates/item/enums/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/model/templates/quest/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/network/aion/skillinfo/fwd.h"
#include "aion/gameserver/questEngine/fwd.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/services/fwd.h"
#include "aion/gameserver/services/instance/fwd.h"
#include "aion/gameserver/services/item/fwd.h"
#include "aion/gameserver/services/teleport/fwd.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/effect/fwd.h"
#include "aion/gameserver/spawnengine/fwd.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/chathandlers/ConsoleCommand.h"
#include "aion/gameserver/utils/chathandlers/fwd.h"
#include "aion/gameserver/utils/collections/fwd.h"
#include "aion/gameserver/utils/fwd.h"
#include "aion/gameserver/utils/xml/fwd.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/fwd.h"
#include "aion/gameserver/world/geo/fwd.h"

namespace aion::gameserver::handlers::consolecommands {

using ::aion::gameserver::configs::main::GSConfig;
using ::aion::gameserver::dao::BookmarkDAO;
using ::aion::gameserver::dataholders::DataManager;
using ::aion::gameserver::model::PlayerClass;
using ::aion::gameserver::model::gameobjects::Item;
using ::aion::gameserver::model::gameobjects::VisibleObject;
using ::aion::gameserver::model::gameobjects::player::CustomPlayerState;
using ::aion::gameserver::model::gameobjects::player::Player;
using ::aion::gameserver::model::gameobjects::state::CreatureVisualState;
using ::aion::gameserver::model::skill::PlayerSkillEntry;
using ::aion::gameserver::model::team::legion::Legion;
using ::aion::gameserver::model::team::legion::LegionMember;
using ::aion::gameserver::model::templates::QuestTemplate;
using ::aion::gameserver::model::templates::item::ItemQuality;
using ::aion::gameserver::model::templates::item::enums::EquipType;
using ::aion::gameserver::model::templates::quest::FinishedQuestCond;
using ::aion::gameserver::model::templates::quest::XMLStartCondition;
using ::aion::gameserver::model::templates::spawns::SpawnTemplate;
using ::aion::gameserver::network::aion::serverpackets::SM_ABNORMAL_STATE;
using ::aion::gameserver::network::aion::serverpackets::SM_GM_BOOKMARK_ADD;
using ::aion::gameserver::network::aion::serverpackets::SM_GM_SEARCH;
using ::aion::gameserver::network::aion::serverpackets::SM_GM_SHOW_LEGION_INFO;
using ::aion::gameserver::network::aion::serverpackets::SM_GM_SHOW_LEGION_MEMBERLIST;
using ::aion::gameserver::network::aion::serverpackets::SM_GM_SHOW_PLAYER_SKILLS;
using ::aion::gameserver::network::aion::serverpackets::SM_GM_SHOW_PLAYER_STATUS;
using ::aion::gameserver::network::aion::serverpackets::SM_PLAYER_STATE;
using ::aion::gameserver::network::aion::serverpackets::SM_QUEST_ACTION;
using ::aion::gameserver::network::aion::serverpackets::SM_RESURRECT;
using ::aion::gameserver::network::aion::serverpackets::SM_STATS_INFO;
using ::aion::gameserver::network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using ::aion::gameserver::network::aion::skillinfo::SkillEntryWriter;
using ::aion::gameserver::questEngine::QuestEngine;
using ::aion::gameserver::questEngine::model::QuestEnv;
using ::aion::gameserver::questEngine::model::QuestState;
using ::aion::gameserver::questEngine::model::QuestStatus;
using ::aion::gameserver::services::AdminService;
using ::aion::gameserver::services::ClassChangeService;
using ::aion::gameserver::services::CubeExpandService;
using ::aion::gameserver::services::EnchantService;
using ::aion::gameserver::services::QuestService;
using ::aion::gameserver::services::SkillLearnService;
using ::aion::gameserver::services::instance::InstanceService;
using ::aion::gameserver::services::item::ItemFactory;
using ::aion::gameserver::services::item::ItemPacketService;
using ::aion::gameserver::services::item::ItemService;
using ::aion::gameserver::services::teleport::TeleportService;
using ::aion::gameserver::skillengine::effect::AbnormalState;
using ::aion::gameserver::spawnengine::SpawnEngine;
using ::aion::gameserver::utils::ChatUtil;
using ::aion::gameserver::utils::PacketSendUtility;
using ::aion::gameserver::utils::chathandlers::ChatProcessor;
using ::aion::gameserver::utils::chathandlers::ConsoleCommand;
using ::aion::gameserver::utils::collections::DynamicServerPacketBodySplitList;
using ::aion::gameserver::utils::collections::FixedElementCountSplitList;
using ::aion::gameserver::utils::collections::SplitList;
using ::aion::gameserver::utils::xml::JAXBUtil;
using ::aion::gameserver::world::World;
using ::aion::gameserver::world::WorldMapInstance;
using ::aion::gameserver::world::geo::GeoService;

} // namespace aion::gameserver::handlers::consolecommands
