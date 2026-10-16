#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/autogroup/AutoGroupType.h"
#include "aion/gameserver/model/autogroup/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::autogroup {

/**
 * Companion of the generated enum AutoGroupType (docs/design/static-data.md §2.5): the constructor data (instance mask id, maximum join time,
 * difficulty id), the methods and the constant-specific newAutoInstance bodies of the Java enum as free functions found by ADL
 * (`getTemplate(AutoGroupType::IDGEL_DOME)` for Java `AutoGroupType.IDGEL_DOME.getTemplate()`). Java's nullable static lookups answer
 * `std::nullopt`.
 * <p>
 * C++: Java looks the template up once, in the enum's constructor (DataManager.AUTO_GROUP.getTemplateByInstanceMaskId, a warning if there is
 * none); getTemplate looks it up in the published holder on each call - the same template, the holder being immortal static data
 * (P5-10.md, M5j stage 4). A method that dereferences a missing template throws Java's NullPointerException explicitly.
 *
 * @author xTz, Estrayl
 */

/** Java: the instanceMaskId constructor argument */
int32_t getInstanceMaskId(AutoGroupType agt);

/** Java: agt.getTemplate(); nullptr when AUTO_GROUP has no template of the mask id */
const AutoGroup* getTemplate(AutoGroupType agt);

/** Java: agt.getL10nId() */
int32_t getL10nId(AutoGroupType agt);

/** Java: agt.getMaximumJoinTime() */
int32_t getMaximumJoinTime(AutoGroupType agt);

/** Java: agt.containNpcId(npcId) */
bool containNpcId(AutoGroupType agt, int32_t npcId);

/** Java: agt.isPeriodicInstance() */
bool isPeriodicInstance(AutoGroupType agt);

/** Java: AutoGroupType.getAGTByMaskId(instanceMaskId) */
std::optional<AutoGroupType> getAGTByMaskId(int32_t instanceMaskId);

/** Java: AutoGroupType.getAutoGroup(level, npcId) */
std::optional<AutoGroupType> getAutoGroup(int32_t level, int32_t npcId);

/** Java: AutoGroupType.getAutoGroupByWorld(level, worldId) */
std::optional<AutoGroupType> getAutoGroupByWorld(int32_t level, int32_t worldId);

/** Java: AutoGroupType.getAutoGroup(npcId) */
std::optional<AutoGroupType> getAutoGroup(int32_t npcId);

/** Java: agt.isPvPSoloArena() */
bool isPvPSoloArena(AutoGroupType agt);

/** Java: agt.isTrainingPvPSoloArena() */
bool isTrainingPvPSoloArena(AutoGroupType agt);

/** Java: agt.isPvPFFAArena() */
bool isPvPFFAArena(AutoGroupType agt);

/** Java: agt.isTrainingPvPFFAArena() */
bool isTrainingPvPFFAArena(AutoGroupType agt);

/** Java: agt.isTrainingHarmonyArena() */
bool isTrainingHarmonyArena(AutoGroupType agt);

/** Java: agt.isHarmonyArena() */
bool isHarmonyArena(AutoGroupType agt);

/** Java: agt.isGloryArena() */
bool isGloryArena(AutoGroupType agt);

/** Java: agt.isPvpArena() */
bool isPvpArena(AutoGroupType agt);

/** Java: agt.isInLvlRange(level) */
bool isInLvlRange(AutoGroupType agt, int32_t level);

/** Java: agt.getDifficultId() (0 for the constants constructed without one) */
int8_t getDifficultId(AutoGroupType agt);

/** Java: agt.createAutoInstance() - the constant-specific newAutoInstance: AutoPvpInstance, AutoPvPFFAInstance or AutoHarmonyInstance */
runtime::Ref<AutoInstance> createAutoInstance(AutoGroupType agt);

} // namespace aion::gameserver::model::autogroup
