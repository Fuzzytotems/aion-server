#include "aion/gameserver/model/stats/calc/functions/PlayerStatFunctions.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <unordered_set>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunctionProxy.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/stats/CalculationType.h"

namespace aion::gameserver::model::stats::calc::functions {

using container::StatEnum;
using gameobjects::Item;
using gameobjects::player::Player;
using runtime::Ptr;
using utils::stats::CalculationType;

namespace {

/** Java int subtraction (wraps on overflow) */
constexpr int32_t subtractInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

/** Java int multiplication (wraps on overflow) */
constexpr int32_t multiplyInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

} // namespace

/**
 * Java: the package-private function classes of PlayerStatFunctions.java. Immortal (created once by the static initializer of FUNCTIONS), so they
 * keep StatFunction's no-op retain/release (StatFunction.h).
 */
class PhysicalAttackFunction final : public StatFunction {
public:
	PhysicalAttackFunction() { stat = StatEnum::PHYSICAL_ATTACK; }

	void apply(Stat2& statValue, const std::unordered_set<CalculationType>& calculationTypes) override {
		if (Ptr<Player> player = runtime::as<Player>(statValue.getOwner())) {
			int32_t power = statValue.getOwner()->getGameStats()->getPower()->getCurrent();
			if (!player->getEquipment().getMainHandWeapon()) {
				statValue.setBaseRate(
					1 + static_cast<float>(multiplyInt(subtractInt(power, 100), getNoWeaponPowerMultiplier(player->getPlayerClass()))) / 10000.0f);
			} else {
				if (calculationTypes.contains(CalculationType::SKILL) && calculationTypes.contains(CalculationType::DUAL_WIELD)) {
					if (power > 100)
						power = commons::utils::Rnd::get(100, power);
					else
						power = commons::utils::Rnd::get(power, 100);
				}
				statValue.setBaseRate(power * 0.01f);
			}
		}
	}

	int32_t getPriority() const override { return 30; }
};

class MaxHpFunction final : public StatFunction {
public:
	MaxHpFunction() { stat = StatEnum::MAXHP; }

	void apply(Stat2& statValue, const std::unordered_set<CalculationType>& /*calculationTypes*/) override {
		if (Ptr<Player> player = runtime::as<Player>(statValue.getOwner()))
			statValue.addToBase(static_cast<float>(player->getGameStats()->getHealthDependentAdditionalHp()));
	}

	int32_t getPriority() const override { return 30; }
};

class MaxMpFunction final : public StatFunction {
public:
	MaxMpFunction() { stat = StatEnum::MAXMP; }

	void apply(Stat2& statValue, const std::unordered_set<CalculationType>& /*calculationTypes*/) override {
		if (Ptr<Player> player = runtime::as<Player>(statValue.getOwner()))
			statValue.addToBase(static_cast<float>(player->getGameStats()->getWillDependentAdditionalMp()));
	}

	int32_t getPriority() const override { return 30; }
};

class MagicalAttackFunction final : public StatFunction {
public:
	MagicalAttackFunction() { stat = StatEnum::MAGICAL_ATTACK; }

	void apply(Stat2& statValue, const std::unordered_set<CalculationType>& /*calculationTypes*/) override {
		float knowledge = static_cast<float>(statValue.getOwner()->getGameStats()->getKnowledge()->getCurrent());
		statValue.setBaseRate(knowledge * 0.01f);
	}

	int32_t getPriority() const override { return 30; }
};

class PDefFunction final : public StatFunction {
public:
	PDefFunction() { stat = StatEnum::PHYSICAL_DEFENSE; }

	void apply(Stat2& statValue, const std::unordered_set<CalculationType>& /*calculationTypes*/) override {
		if (statValue.getOwner()->isInFlyingState())
			statValue.setFinalRate(0.6f);
	}
};

class BlockFunction final : public StatFunction {
public:
	BlockFunction() { stat = StatEnum::BLOCK; }

	void apply(Stat2& statValue, const std::unordered_set<CalculationType>& /*calculationTypes*/) override {
		if (Ptr<Player> player = runtime::as<Player>(statValue.getOwner()))
			statValue.addToBase(static_cast<float>(player->getGameStats()->getAgilityDependentAdditionalBaseBlock()));
	}
};

class ParryFunction final : public StatFunction {
public:
	ParryFunction() { stat = StatEnum::PARRY; }

	void apply(Stat2& statValue, const std::unordered_set<CalculationType>& /*calculationTypes*/) override {
		if (Ptr<Player> player = runtime::as<Player>(statValue.getOwner()))
			statValue.addToBase(static_cast<float>(player->getGameStats()->getAgilityDependentAdditionalBaseParry()));
	}
};

class EvasionFunction final : public StatFunction {
public:
	EvasionFunction() { stat = StatEnum::EVASION; }

	void apply(Stat2& statValue, const std::unordered_set<CalculationType>& /*calculationTypes*/) override {
		if (Ptr<Player> player = runtime::as<Player>(statValue.getOwner()))
			statValue.addToBase(static_cast<float>(player->getGameStats()->getAgilityDependentAdditionalBaseEvasion()));
	}
};

class PhysicalCriticalFunction final : public StatFunction {
public:
	PhysicalCriticalFunction() { stat = StatEnum::PHYSICAL_CRITICAL; }

	void apply(Stat2& statValue, const std::unordered_set<CalculationType>& /*calculationTypes*/) override {
		if (Ptr<Player> player = runtime::as<Player>(statValue.getOwner()))
			statValue.addToBase(static_cast<float>(player->getGameStats()->getAccuracyDependentAdditionalBasePhysicalCritical()));
	}
};

class PhysicalAccuracyFunction final : public StatFunction {
public:
	PhysicalAccuracyFunction() { stat = StatEnum::PHYSICAL_ACCURACY; }

	void apply(Stat2& statValue, const std::unordered_set<CalculationType>& /*calculationTypes*/) override {
		if (Ptr<Player> player = runtime::as<Player>(statValue.getOwner()))
			statValue.addToBase(static_cast<float>(player->getGameStats()->getAccuracyDependentAdditionalBasePhysicalAccuracy()));
	}
};

class DuplicateStatFunction : public StatFunction {
public:
	void apply(Stat2& statValue, const std::unordered_set<CalculationType>& calculationTypes) override {
		Player& player = *runtime::cast<Player>(statValue.getOwner());
		Ptr<Item> mainWeapon = player.getEquipment().getMainHandWeapon();
		Ptr<Item> offWeapon = player.getEquipment().getOffHandWeapon();
		if (mainWeapon.get() == offWeapon.get())
			offWeapon = nullptr;

		if (mainWeapon) {
			const StatFunction* func1 = nullptr;
			const StatFunction* func2 = nullptr;
			std::vector<const StatFunction*> functions;
			const std::vector<std::unique_ptr<StatFunction>>* functions1 = templateOf(*mainWeapon).getModifiers();

			if (functions1 != nullptr) {
				std::vector<const StatFunction*> f1 = getFunctions(*functions1, statValue, *mainWeapon);
				if (!f1.empty()) {
					func1 = f1.front();
					functions.insert(functions.end(), f1.begin(), f1.end());
				}
			}

			if (mainWeapon->hasFusionedItem()) {
				const templates::item::ItemTemplate* fusionedTemplate = mainWeapon->getFusionedItemTemplate();
				if (fusionedTemplate == nullptr)
					throw runtime::NullPointerException("fusionedItemTemplate is null");
				const std::vector<std::unique_ptr<StatFunction>>* functions2 = fusionedTemplate->getModifiers();
				if (functions2 != nullptr) {
					std::vector<const StatFunction*> f2 = getFunctions(*functions2, statValue, *mainWeapon);
					if (!f2.empty()) {
						func2 = f2.front();
						functions.insert(functions.end(), f2.begin(), f2.end());
					}
				}
			} else if (offWeapon) {
				const std::vector<std::unique_ptr<StatFunction>>* functions2 = templateOf(*offWeapon).getModifiers();
				if (functions2 != nullptr) {
					std::vector<const StatFunction*> f2 = getFunctions(*functions2, statValue, *offWeapon);
					functions.insert(functions.end(), f2.begin(), f2.end());
				}
			}

			if (func1 != nullptr && func2 != nullptr) { // for fusioned weapons
				// Java: List.remove(Object) removes the first identical element (StatFunction has no equals)
				const StatFunction* toRemove = absValue(*func1) >= absValue(*func2) ? func2 : func1;
				functions.erase(std::find(functions.begin(), functions.end(), toRemove));
			}
			if (!functions.empty()) {
				if (getName() == StatEnum::PVP_ATTACK_RATIO) {
					for (const StatFunction* f : functions)
						asMutable(*f).apply(statValue, calculationTypes);
				} else {
					// Java: stream().max(comparing value).get() - BinaryOperator.maxBy keeps the first of equal elements
					const StatFunction* max = functions.front();
					for (const StatFunction* f : functions) {
						if (valueOf(*f) > valueOf(*max))
							max = f;
					}
					asMutable(*max).apply(statValue, calculationTypes);
				}
				functions.clear();
			}
		}
	}

	int32_t getPriority() const override { return 60; }

private:
	/** Java: getFunctions(List<StatFunction> list, Stat2 stat, Item item) */
	std::vector<const StatFunction*> getFunctions(const std::vector<std::unique_ptr<StatFunction>>& list, Stat2& statValue, Item& item) {
		std::vector<const StatFunction*> functions;
		for (const std::unique_ptr<StatFunction>& func : list) {
			if (asMutable(*func).getName() == getName()) {
				runtime::Ref<StatFunctionProxy> func2 = StatFunctionProxy::create(Ptr<StatOwner>(item), *StatFunction::ofTemplate(func.get()));
				if (func2->validate(statValue))
					functions.push_back(func.get());
			}
		}
		return functions;
	}

	static const templates::item::ItemTemplate& templateOf(Item& item) {
		if (item.getItemTemplate() == nullptr)
			throw runtime::NullPointerException("itemTemplate is null");
		return *item.getItemTemplate();
	}

	/** Static data is never modified after loading; the non-const IStatFunction interface only reads it (StatFunction::ofTemplate) */
	static StatFunction& asMutable(const StatFunction& function) { return *StatFunction::ofTemplate(&function); }

	static int32_t valueOf(const StatFunction& function) { return asMutable(function).getValue(); }

	/** Java: Math.abs(func.getValue()) (Integer.MIN_VALUE stays negative) */
	static int32_t absValue(const StatFunction& function) {
		int32_t value = valueOf(function);
		return value < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(value)) : value;
	}
};

class AttackSpeedFunction final : public DuplicateStatFunction {
public:
	AttackSpeedFunction() { stat = StatEnum::ATTACK_SPEED; }
};

class BoostCastingTimeFunction final : public DuplicateStatFunction {
public:
	BoostCastingTimeFunction() { stat = StatEnum::BOOST_CASTING_TIME; }
};

class PvEAttackRatioFunction final : public StatFunction {
public:
	PvEAttackRatioFunction() { stat = StatEnum::PVE_ATTACK_RATIO; }

	void apply(Stat2& statValue, const std::unordered_set<CalculationType>& /*calculationTypes*/) override {
		const templates::world::WorldMapTemplate* worldMapTemplate = dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(statValue.getOwner()->getWorldId());
		if (worldMapTemplate == nullptr)
			throw runtime::NullPointerException("world map template is null");
		statValue.addToBonus(static_cast<float>(worldMapTemplate->getPvEAttackRatio()));
	}
};

class PvEDefendRatioFunction final : public StatFunction {
public:
	PvEDefendRatioFunction() { stat = StatEnum::PVE_DEFEND_RATIO; }

	void apply(Stat2& statValue, const std::unordered_set<CalculationType>& /*calculationTypes*/) override {
		const templates::world::WorldMapTemplate* worldMapTemplate = dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(statValue.getOwner()->getWorldId());
		if (worldMapTemplate == nullptr)
			throw runtime::NullPointerException("world map template is null");
		statValue.addToBonus(static_cast<float>(worldMapTemplate->getPvEDefendRatio()));
	}
};

class PvPAttackRatioFunction final : public DuplicateStatFunction {
public:
	PvPAttackRatioFunction() { stat = StatEnum::PVP_ATTACK_RATIO; }
};

const std::vector<IStatFunction*> PlayerStatFunctions::FUNCTIONS{
	new PhysicalAttackFunction(), new MagicalAttackFunction(), new AttackSpeedFunction(), new BoostCastingTimeFunction(), new PvPAttackRatioFunction(),
	new PDefFunction(), new MaxHpFunction(), new MaxMpFunction(), new BlockFunction(), new ParryFunction(), new EvasionFunction(),
	new PhysicalCriticalFunction(), new PhysicalAccuracyFunction(), new PvEAttackRatioFunction(), new PvEDefendRatioFunction()};

std::vector<Ptr<IStatFunction>> PlayerStatFunctions::getFunctions() {
	return std::vector<Ptr<IStatFunction>>(FUNCTIONS.begin(), FUNCTIONS.end());
}

void PlayerStatFunctions::addPredefinedStatFunctions(Player& player) {
	player.getGameStats()->addEffectOnly(nullptr, getFunctions());
}

} // namespace aion::gameserver::model::stats::calc::functions
