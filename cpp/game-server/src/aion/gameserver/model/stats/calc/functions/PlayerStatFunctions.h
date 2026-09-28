#pragma once

#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/stats/calc/functions/fwd.h"

namespace aion::gameserver::model::stats::calc::functions {

/**
 * The stat functions every player has (attribute dependent attack, HP, MP, block, parry, evasion, accuracy and the weapon ratio functions).
 * <p>
 * C++: a static-only class. Java's `List<IStatFunction> FUNCTIONS` is filled only by the static initializer and never modified, so it is a
 * `static const std::vector` in Java order (hub-headers.md §11.1). Its functions are the package-private StatFunction subclasses of
 * PlayerStatFunctions.java (PhysicalAttackFunction, MaxHpFunction, ...), defined in the .cpp: immortal objects of a static initializer, whose
 * StatFunction retain/release are no-ops (StatFunction.h), so the stat containers hold them without counting.
 *
 * @author ATracer
 */
class PlayerStatFunctions {
public:
	PlayerStatFunctions() = delete;

private:
	static const std::vector<IStatFunction*> FUNCTIONS;

public:
	/** Java: the static list (read-only) */
	static std::vector<runtime::Ptr<IStatFunction>> getFunctions();

	static void addPredefinedStatFunctions(gameobjects::player::Player& player);
};

} // namespace aion::gameserver::model::stats::calc::functions
