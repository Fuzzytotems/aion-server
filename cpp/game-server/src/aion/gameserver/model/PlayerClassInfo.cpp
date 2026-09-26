#include "aion/gameserver/model/PlayerClassInfo.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "aion/gameserver/model/stats/calc/PlayerStatCalculator.h"
#include "aion/gameserver/model/templates/stats/StatsTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/LockClass.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::model {

namespace {

using stats::calc::PlayerStatCalculator;

/**
 * Java: the private inner class PlayerClass.PlayerStatsTemplate, a StatsTemplate whose base attributes are those of its player class and whose
 * speeds are fixed. Immortal like all templates (see createStatsTemplate).
 */
class PlayerStatsTemplate final : public templates::stats::StatsTemplate {
public:
	explicit PlayerStatsTemplate(PlayerClass value) : playerClass(value) {}

	int32_t getPower() const override { return model::getPower(playerClass); }

	int32_t getHealth() const override { return model::getHealth(playerClass); }

	int32_t getAgility() const override { return model::getAgility(playerClass); }

	int32_t getBaseAccuracy() const override { return model::getAccuracy(playerClass); }

	int32_t getKnowledge() const override { return model::getKnowledge(playerClass); }

	int32_t getWill() const override { return model::getWill(playerClass); }

	float getWalkSpeed() const override { return 1.5f; }

	float getRunSpeed() const override { return 6.0f; }

	float getFlySpeed() const override { return 9.0f; }

private:
	/** Java: the enclosing PlayerClass constant */
	const PlayerClass playerClass;
};

/** The interned templates by (class, level) */
struct InternTable {
	runtime::Monitor lock{AION_LOCK_CLASS(PlayerClass::createStatsTemplate)};
	std::map<std::pair<PlayerClass, int32_t>, std::unique_ptr<const templates::stats::StatsTemplate>> templates;
};

/** leaked immortal: PlayerGameStats keeps the templates until the process exits */
InternTable& internTable() {
	static auto* const table = new InternTable();
	return *table;
}

/** Java: the body of createStatsTemplate */
std::unique_ptr<const templates::stats::StatsTemplate> newStatsTemplate(PlayerClass playerClass, int32_t level) {
	auto statsTemplate = std::make_unique<PlayerStatsTemplate>(playerClass);
	statsTemplate->setMaxHp(PlayerStatCalculator::calculateMaxHp(playerClass, level));
	statsTemplate->setMaxMp(PlayerStatCalculator::calculateMaxMp(playerClass, level));
	statsTemplate->setBlock(PlayerStatCalculator::calculateBlockEvasionOrParry(level));
	statsTemplate->setParry(PlayerStatCalculator::calculateBlockEvasionOrParry(level));
	statsTemplate->setEvasion(PlayerStatCalculator::calculateBlockEvasionOrParry(level));
	statsTemplate->setAccuracy(PlayerStatCalculator::calculatePhysicalAccuracy(level));
	statsTemplate->setMacc(PlayerStatCalculator::calculateMagicalAccuracy(level));
	statsTemplate->setAttack(18);
	statsTemplate->setPcrit(2);
	statsTemplate->setMcrit(50);
	statsTemplate->setStrikeResist(PlayerStatCalculator::calculateStrikeResist(level));
	statsTemplate->setSpellResist(getMagicalCriticalResist(playerClass));
	return statsTemplate;
}

} // namespace

// lint: L7 C++ only: interning replaces Java's unsynchronized per-call `new PlayerStatsTemplate`, so the table needs its own lock
const templates::stats::StatsTemplate* createStatsTemplate(PlayerClass playerClass, int32_t level) {
	// Java creates a new PlayerStatsTemplate per call, which PlayerGameStats keeps until the next level change. Its values depend only on the
	// class and the level and nobody compares templates by identity, so C++ interns one immutable template per class and level: templates are
	// `const StatsTemplate*` and immortal (hub-headers.md §5), which a per-call object could not be without a leak per level change.
	InternTable& table = internTable();
	SYNCHRONIZED(table.lock) {
		std::unique_ptr<const templates::stats::StatsTemplate>& statsTemplate = table.templates[{playerClass, level}];
		if (!statsTemplate)
			statsTemplate = newStatsTemplate(playerClass, level);
		return statsTemplate.get();
	}
}

std::optional<PlayerClass> getPlayerClassById(int8_t classId, bool ignoreInvalidClassId) {
	for (size_t i = 0; i < detail::PLAYER_CLASS_DATA.size(); ++i) {
		if (detail::PLAYER_CLASS_DATA[i].classId == classId)
			return static_cast<PlayerClass>(i);
	}
	if (ignoreInvalidClassId)
		return std::nullopt;
	throw runtime::IllegalArgumentException("There is no player class with id " + std::to_string(classId));
}

PlayerClass getPlayerClassById(int8_t classId) {
	return *getPlayerClassById(classId, false);
}

} // namespace aion::gameserver::model
