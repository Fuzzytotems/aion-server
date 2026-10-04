#include "aion/gameserver/skillengine/effect/SummonSkillAreaEffect.h"

#include <cstdint>
#include <string>

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/NpcObjectType.h"
#include "aion/gameserver/model/gameobjects/Servant.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/model/skill/NpcSkillList.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::skillengine::effect {

namespace {

/**
 * Java: the anonymous Runnable of SummonSkillAreaEffect.applyEffect (SummonSkillAreaEffect.java:58-66, fieldmap key SummonSkillAreaEffect$1),
 * scheduled at a fixed rate and stored as the servant's SKILL_USE task: each run makes the servant use the skill at the next position of its
 * skill list. Its skillPos changes between runs, so it is K4 (fieldmap.toml [kinds]): RefCounted, created with create(), retaining the
 * servant; the pending periodic Future pins it (its only holder) until the task is cancelled.
 */
class SummonSkillAreaEffect_Runnable final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<gameserver::model::gameobjects::Servant> servant; // captured local Servant servant
	runtime::Field<int32_t> skillPos{0};

protected:
	explicit SummonSkillAreaEffect_Runnable(gameserver::model::gameobjects::Servant& servantValue) : servant(servantValue) {}
	~SummonSkillAreaEffect_Runnable() override = default;

public:
	static runtime::Ref<SummonSkillAreaEffect_Runnable> create(gameserver::model::gameobjects::Servant& servant) {
		return runtime::makeRef<SummonSkillAreaEffect_Runnable>(servant);
	}

	void run() {
		controllers::CreatureController& controller = servant->getController();
		controller.useSkill(servant->getSkillList()->getSkillOnPosition(skillPos.get())->getSkillId());
		skillPos.set(skillPos.get() + 1);
	}
};

} // namespace

void SummonSkillAreaEffect::applyEffect(model::Effect& effect) const {
	float x = effect.getX();
	float y = effect.getY();
	float z = effect.getZ();
	if (x == 0 && y == 0) {
		gameserver::model::gameobjects::Creature& effected = *effect.getEffected();
		x = effected.getX();
		y = effected.getY();
		z = effected.getZ();
	}

	int32_t tickDelay = 3000;
	int32_t spawnDuration = time;

	const std::string& group = effect.getSkillTemplate()->getGroup(); // Java null: "" (matches no case)
	if (group == "KN_THREATENINGWAVE") {
		spawnDuration = 15; // client files say 11s but description 15s
		tickDelay = 2000;
	} else if (group == "WI_SUMMONTORNADO") {
		tickDelay = 1900;
	} else if (group == "WI_DELAYEDSTRIKE") {
		tickDelay = 5000;
		spawnDuration = 9;
	}

	runtime::Ref<gameserver::model::gameobjects::Servant> servant =
		spawnServant(effect, spawnDuration, gameserver::model::gameobjects::NpcObjectType::SKILLAREA, x, y, z);
	if (effect.getEffected()) // point skill without any initial target (we cannot trigger handleAttack with a null target)
		servant->getAi().onCreatureEvent(ai::event::AIEventType::ATTACK, *effect.getEffected());

	runtime::Ref<SummonSkillAreaEffect_Runnable> runnable = SummonSkillAreaEffect_Runnable::create(*servant);
	SummonSkillAreaEffect_Runnable& tick = *runnable;
	runtime::FutureRef task = utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({&tick}, [&tick] { tick.run(); }, 0, tickDelay);
	servant->getController().addTask(gameserver::model::TaskId::SKILL_USE, std::move(task));
}

} // namespace aion::gameserver::skillengine::effect
