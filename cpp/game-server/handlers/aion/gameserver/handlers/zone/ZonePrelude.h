#pragma once

// Prelude of the zone handlers (data/handlers/zone, also zone/pvpZones).
// A prelude holds the core names of its category as using-declarations in the category namespace, e.g. `using gameserver::ai::NpcAI;`
// (handlers-and-porting-plan.md §1.2). It is identical for every file of the category, so unity batches may include it any number of times,
// and it is the category's precompiled header. Handler files include it first; it also provides the registration markers and
// AION_UNPORTED. The handler file rules apply (aion_gs_regscan): declarations only inside the namespace of this directory, no namespace-scope
// `static`, no other namespaces.
// Using-declarations (S0b): every core type that the Java files of the category import, when a forward header declares it, its simple
// name comes from one package only and no Java type of the category has that name (nested types are spelled Outer::Inner or
// Outer_Inner). The prelude includes the full headers of the category's handler base classes, the generated headers of the
// re-exported enums and the forward headers of the rest.

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/configs/main/fwd.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/zone/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/questEngine/model/fwd.h"
#include "aion/gameserver/services/player/fwd.h"
#include "aion/gameserver/services/teleport/fwd.h"
#include "aion/gameserver/utils/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"
#include "aion/gameserver/world/zone/handler/QuestZoneHandler.h"
#include "aion/gameserver/world/zone/handler/fwd.h"

namespace aion::gameserver::handlers::zone {

using ::aion::gameserver::configs::main::CustomConfig;
using ::aion::gameserver::controllers::observer::AbstractQuestZoneObserver;
using ::aion::gameserver::model::TaskId;
using ::aion::gameserver::model::gameobjects::Creature;
using ::aion::gameserver::model::gameobjects::player::Player;
using ::aion::gameserver::model::templates::zone::ZoneTemplate;
using ::aion::gameserver::network::aion::serverpackets::SM_QUEST_ACTION;
using ::aion::gameserver::network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using ::aion::gameserver::questEngine::model::QuestState;
using ::aion::gameserver::services::player::PlayerReviveService;
using ::aion::gameserver::services::teleport::TeleportService;
using ::aion::gameserver::utils::PacketSendUtility;
using ::aion::gameserver::utils::ThreadPoolManager;
using ::aion::gameserver::world::zone::PvPZoneInstance;
using ::aion::gameserver::world::zone::ZoneInstance;
using ::aion::gameserver::world::zone::ZoneName;
using ::aion::gameserver::world::zone::handler::AdvancedZoneHandler;
using ::aion::gameserver::world::zone::handler::QuestZoneHandler;

} // namespace aion::gameserver::handlers::zone
