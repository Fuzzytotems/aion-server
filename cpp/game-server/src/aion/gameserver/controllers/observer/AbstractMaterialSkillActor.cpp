#include "aion/gameserver/controllers/observer/AbstractMaterialSkillActor.h"

#include <string>
#include <typeinfo>
#include <utility>

#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/geoEngine/scene/Spatial.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/materials/MaterialActCondition.h"
#include "aion/gameserver/model/templates/materials/MaterialSkill.h"
#include "aion/gameserver/model/templates/world/WeatherEntry.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/GameTimeService.h"
#include "aion/gameserver/services/WeatherService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Effect_ForceType.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/SimpleClassName.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/time/gametime/DayTime.h"
#include "aion/gameserver/utils/time/gametime/GameTime.h"

namespace aion::gameserver::controllers::observer {

using model::gameobjects::Creature;
using model::gameobjects::player::Player;
using model::templates::materials::MaterialActCondition;
using model::templates::materials::MaterialSkill;
using runtime::Ptr;
using runtime::Ref;

/**
 * Java: private class MaterialSkillTask implements Runnable, scheduled at a fixed rate by act(). K4 (fieldmap): RefCounted with create(), the
 * enclosing actor held by `const Ref` (javac this$0). Cycle actor.task -> Future -> task -> actor: java-hook abort() (cycles.toml,
 * AbstractMaterialSkillActor.java:49-53), which cancels the Future and so releases this task.
 */
class AbstractMaterialSkillActor::MaterialSkillTask final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	runtime::Field<const MaterialSkill*> skill{};
	runtime::Field<int32_t> secondsElapsed{};
	const Ref<AbstractMaterialSkillActor> abstractMaterialSkillActor; // this$0

protected:
	explicit MaterialSkillTask(AbstractMaterialSkillActor& outer) : abstractMaterialSkillActor(outer) {}
	~MaterialSkillTask() override = default;

public:
	static Ref<MaterialSkillTask> create(AbstractMaterialSkillActor& outer) { return runtime::makeRef<MaterialSkillTask>(outer); }

	void run() {
		AbstractMaterialSkillActor& outer = *abstractMaterialSkillActor;
		const MaterialSkill* currentSkill = skill.get();
		int32_t elapsed = secondsElapsed.get();
		secondsElapsed = elapsed + 1;
		if (elapsed % (currentSkill == nullptr ? 1 : currentSkill->getFrequency()) != 0)
			return;
		if (!outer.isTouched.get())
			return;
		Ptr<Creature> observed = outer.creature.get();
		if (!observed->isSpawned() || observed->isDead())
			return;
		if (Ptr<Player> player = runtime::as<Player>(observed); player && player->isProtectionActive())
			return;
		skill = outer.findFirstSkillWithMatchingCondition();
		if ((currentSkill = skill.get()) == nullptr) // skip if currently nothing matches (fires are off while raining)
			return;
		if (Ptr<Player> player = runtime::as<Player>(observed); configs::main::GeoDataConfig::GEO_MATERIALS_SHOWDETAILS && player && player->isStaff())
			utils::PacketSendUtility::sendMessage(*player, utils::simpleClassName(typeid(outer)) + " use skill=" + std::to_string(currentSkill->getId()));
		skillengine::SkillEngine::getInstance().applyEffectDirectly(currentSkill->getId(), currentSkill->getSkillLevel(), *observed, *observed,
			std::nullopt, skillengine::model::Effect_ForceType::MATERIAL_SKILL);
	}
};

AbstractMaterialSkillActor::AbstractMaterialSkillActor(model::gameobjects::Creature& creatureValue,
	runtime::Ptr<geoEngine::scene::Spatial> geometryValue, int8_t intentionsValue, CheckType checkType, model::TaskId taskIdValue,
	std::vector<const model::templates::materials::MaterialSkill*> skillsValue)
	: AbstractCollisionObserver(creatureValue, geometryValue, intentionsValue, checkType), taskId(taskIdValue) {
	runtime::Ref<runtime::RcArrayList<const model::templates::materials::MaterialSkill*>> list =
		runtime::RcArrayList<const model::templates::materials::MaterialSkill*>::create(AION_LOCK_CLASS(AbstractMaterialSkillActor::skills));
	for (const model::templates::materials::MaterialSkill* skill : skillsValue)
		list->add(skill);
	skills.set(std::move(list)); // Java: this.skills = skills
}

AbstractMaterialSkillActor::~AbstractMaterialSkillActor() = default;

void AbstractMaterialSkillActor::act() {
	if (!skills->isEmpty() && !creature->getController().hasTask(taskId)) {
		runtime::FutureRef t = utils::ThreadPoolManager::getInstance().scheduleAtFixedRate(runtime::Pin(),
			[materialSkillTask = MaterialSkillTask::create(*this)] { materialSkillTask->run(); }, 0, 1000);
		if (task.compareAndSet(nullptr, t))
			creature->getController().addTask(taskId, runtime::FutureRef(task.get()));
		else // should not happen
			t->cancel(false);
	}
}

void AbstractMaterialSkillActor::abort() {
	runtime::FutureRef t = task.getAndSet(nullptr);
	if (t)
		creature->getController().cancelTaskIfPresent(taskId, t);
}

void AbstractMaterialSkillActor::died(model::gameobjects::Creature& value) {
	isTouched = false;
	abort();
}

const model::templates::materials::MaterialSkill* AbstractMaterialSkillActor::findFirstSkillWithMatchingCondition() {
	Ptr<runtime::RcArrayList<const MaterialSkill*>> currentSkills = skills.get();
	SYNCHRONIZED(*currentSkills) {
		for (const MaterialSkill* skill : *currentSkills) {
			if (matchActConditions(skill))
				return skill;
		}
	}
	return nullptr;
}

bool AbstractMaterialSkillActor::matchActConditions(const model::templates::materials::MaterialSkill* skill) {
	if (skill->getConditions().empty())
		return true;
	for (MaterialActCondition condition : skill->getConditions()) {
		if (condition == MaterialActCondition::NIGHT &&
			services::GameTimeService::getInstance().getGameTime()->getDayTime() == utils::time::gametime::DayTime::NIGHT)
			return true;
		if (condition == MaterialActCondition::SUNNY) { // sunny actually means "not raining" (fireplaces don't burn during rain)
			const model::templates::world::WeatherEntry* weatherEntry = services::WeatherService::getInstance().findWeatherEntry(*creature);
			if (weatherEntry == nullptr)
				throw runtime::NullPointerException("WeatherService.findWeatherEntry returned null"); // Java dereferences it
			bool isRain = !weatherEntry->getWeatherName().empty() && weatherEntry->getWeatherName().starts_with("RAIN");
			if (!isRain || weatherEntry->isBefore()) // before means "before" the weather (e.g. clouds before rain)
				return true;
		}
	}
	return false;
}

} // namespace aion::gameserver::controllers::observer
