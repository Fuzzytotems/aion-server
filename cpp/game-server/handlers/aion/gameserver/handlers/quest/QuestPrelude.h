#pragma once

// Prelude of the quest handlers (data/handlers/quest, also quest/<region>/...).
// A prelude holds the core names of its category as using-declarations in the category namespace, e.g. `using gameserver::ai::NpcAI;`
// (handlers-and-porting-plan.md §1.2). It is identical for every file of the category, so unity batches may include it any number of times,
// and it is the category's precompiled header. Handler files include it first; it also provides the registration markers and
// AION_UNPORTED. The handler file rules apply (aion_gs_regscan): declarations only inside the namespace of this directory, no namespace-scope
// `static`, no other namespaces.
// The DialogAction using-directive is the only one allowed in handler code (CONVENTIONS, handler file rules).
// Using-declarations (S0b): every core type that the Java files of the category import, when a forward header declares it, its simple
// name comes from one package only and no Java type of the category has that name (nested types are spelled Outer::Inner or
// Outer_Inner). The prelude includes the full headers of the category's handler base classes, the generated headers of the
// re-exported enums, the full hub headers the category's handlers use most (Player, Npc, PacketSendUtility, ...; spine freeze) and the forward
// headers of the rest.

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/ai/event/fwd.h"
#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/manager/fwd.h"
#include "aion/gameserver/configs/main/fwd.h"
#include "aion/gameserver/dataholders/fwd.h"
#include "aion/gameserver/geoEngine/math/fwd.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/instance/handlers/fwd.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/DialogPage.h"
#include "aion/gameserver/model/EmotionId.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/items/storage/fwd.h"
#include "aion/gameserver/model/team/group/fwd.h"
#include "aion/gameserver/model/templates/quest/fwd.h"
#include "aion/gameserver/model/templates/rewards/BonusType.h"
#include "aion/gameserver/model/templates/rewards/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/handlers/AbstractQuestHandler.h"
#include "aion/gameserver/questEngine/handlers/HandlerResult.h"
#include "aion/gameserver/questEngine/handlers/fwd.h"
#include "aion/gameserver/questEngine/model/QuestActionType.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/questEngine/task/fwd.h"
#include "aion/gameserver/services/craft/fwd.h"
#include "aion/gameserver/services/event/fwd.h"
#include "aion/gameserver/services/fwd.h"
#include "aion/gameserver/services/instance/fwd.h"
#include "aion/gameserver/services/item/fwd.h"
#include "aion/gameserver/services/reward/fwd.h"
#include "aion/gameserver/services/teleport/fwd.h"
#include "aion/gameserver/skillengine/fwd.h"
#include "aion/gameserver/spawnengine/fwd.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/fwd.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"
#include "aion/gameserver/utils/stats/fwd.h"
#include "aion/gameserver/world/WorldMapType.h"
#include "aion/gameserver/world/fwd.h"
#include "aion/gameserver/world/geo/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::handlers::quest {

using namespace aion::gameserver::model::DialogAction; // Java: import static DialogAction.*

using ::aion::gameserver::ai::AIState;
using ::aion::gameserver::ai::NpcAI;
using ::aion::gameserver::ai::event::AIEventType;
using ::aion::gameserver::ai::manager::WalkManager;
using ::aion::gameserver::configs::main::CustomConfig;
using ::aion::gameserver::configs::main::GroupConfig;
using ::aion::gameserver::dataholders::DataManager;
using ::aion::gameserver::geoEngine::math::Vector3f;
using ::aion::gameserver::instance::handlers::InstanceHandler;
using ::aion::gameserver::model::DialogPage;
using ::aion::gameserver::model::EmotionId;
using ::aion::gameserver::model::EmotionType;
using ::aion::gameserver::model::PlayerClass;
using ::aion::gameserver::model::Race;
using ::aion::gameserver::model::TaskId;
using ::aion::gameserver::model::animations::TeleportAnimation;
using ::aion::gameserver::model::gameobjects::Creature;
using ::aion::gameserver::model::gameobjects::Item;
using ::aion::gameserver::model::gameobjects::Npc;
using ::aion::gameserver::model::gameobjects::VisibleObject;
using ::aion::gameserver::model::gameobjects::player::Player;
using ::aion::gameserver::model::gameobjects::state::CreatureState;
using ::aion::gameserver::model::house::House;
using ::aion::gameserver::model::items::storage::Storage;
using ::aion::gameserver::model::team::group::PlayerGroup;
using ::aion::gameserver::model::templates::quest::QuestItems;
using ::aion::gameserver::model::templates::rewards::BonusType;
using ::aion::gameserver::model::templates::spawns::SpawnSearchResult;
using ::aion::gameserver::model::templates::spawns::SpawnTemplate;
using ::aion::gameserver::network::aion::serverpackets::SM_ASCENSION_MORPH;
using ::aion::gameserver::network::aion::serverpackets::SM_DIALOG_WINDOW;
using ::aion::gameserver::network::aion::serverpackets::SM_EMOTION;
using ::aion::gameserver::network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using ::aion::gameserver::network::aion::serverpackets::SM_NPC_INFO;
using ::aion::gameserver::network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using ::aion::gameserver::network::aion::serverpackets::SM_USE_OBJECT;
using ::aion::gameserver::questEngine::handlers::AbstractQuestHandler;
using ::aion::gameserver::questEngine::handlers::HandlerResult;
using ::aion::gameserver::questEngine::model::QuestActionType;
using ::aion::gameserver::questEngine::model::QuestEnv;
using ::aion::gameserver::questEngine::model::QuestState;
using ::aion::gameserver::questEngine::model::QuestStatus;
using ::aion::gameserver::questEngine::task::QuestTasks;
using ::aion::gameserver::services::ClassChangeService;
using ::aion::gameserver::services::GameTimeService;
using ::aion::gameserver::services::HousingService;
using ::aion::gameserver::services::QuestService;
using ::aion::gameserver::services::SiegeService;
using ::aion::gameserver::services::craft::CraftSkillUpdateService;
using ::aion::gameserver::services::event::EventService;
using ::aion::gameserver::services::instance::InstanceService;
using ::aion::gameserver::services::item::ItemService;
using ::aion::gameserver::services::reward::WebRewardService;
using ::aion::gameserver::services::teleport::TeleportService;
using ::aion::gameserver::skillengine::SkillEngine;
using ::aion::gameserver::spawnengine::SpawnEngine;
using ::aion::gameserver::utils::PacketSendUtility;
using ::aion::gameserver::utils::PositionUtil;
using ::aion::gameserver::utils::ThreadPoolManager;
using ::aion::gameserver::utils::stats::AbyssRankEnum;
using ::aion::gameserver::world::WorldMapInstance;
using ::aion::gameserver::world::WorldMapType;
using ::aion::gameserver::world::WorldPosition;
using ::aion::gameserver::world::geo::GeoService;
using ::aion::gameserver::world::zone::ZoneName;

} // namespace aion::gameserver::handlers::quest
