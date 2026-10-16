#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

#include <optional>
#include <unordered_set>

#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //stat: shows and modifies any stats.
 * <p>
 * C++: Java's package-private nested types keep their names. CommandStatFunction is created at run time as
 * `RcStatFunction<Stat::CommandStatFunction>::create(stat, value)` (StatFunction.h); Speed creates it too (Java imports Stat.CommandStatFunction).
 * CommandStatOwner (a Java record with a static EnumMap) is an immortal StatOwner per StatEnum constant, created on first use like
 * computeIfAbsent and never removed.
 */
class Stat : public AdminCommand {
public:
	Stat();

	void execute(Player& admin, std::span<const std::string> params) override;

	void showStatFunctions(Player& admin, Creature& target, std::string_view searchStat);

	void listStats(Player& admin);

	void setStat(Player& admin, Creature& target, std::string_view searchStat, int32_t value);

	void cancelStatOverrides(Player& admin, Creature& target);

	/** Java: static class CommandStatFunction extends StatFunction */
	class CommandStatFunction : public model::stats::calc::functions::StatFunction {
	public:
		CommandStatFunction(StatEnum name, int32_t value);

		void apply(model::stats::calc::Stat2& stat, const std::unordered_set<CalculationType>& calculationTypes) override;

		int32_t getPriority() const override final;
	};

	/** Java: record CommandStatOwner(StatEnum stat) implements StatOwner */
	class CommandStatOwner final : public model::stats::calc::StatOwner {
	public:
		explicit CommandStatOwner(StatEnum stat) : stat_(stat) {}

		StatEnum stat() const { return stat_; }

		/** immortal (one per StatEnum constant, never freed): Ref<StatOwner> may hold it */
		void retain() const noexcept override {}
		void release() const noexcept override {}

		static model::stats::calc::StatOwner& get(StatEnum stat);

		static void forEach(const std::function<void(model::stats::calc::StatOwner&)>& consumer);

	private:
		const StatEnum stat_;
	};

private:
	std::optional<StatEnum> findStat(Player& admin, std::string_view searchStat);

	std::vector<StatEnum> findPossibleMatches(std::string_view searchStat);

	void showActiveStatFunctions(Player& admin, Creature& target, StatEnum stat);

	void applyStatFunction(Creature& creature, model::stats::calc::functions::StatFunction& statFunction);

	/** Java: record StatFunctionInfo(int value, boolean bonus, int priority, StatOwner owner, String type) */
	struct StatFunctionInfo {
		int32_t value;
		bool bonus;
		int32_t priority;
		runtime::Ptr<model::stats::calc::StatOwner> owner;
		std::string type;

		explicit StatFunctionInfo(model::stats::calc::functions::IStatFunction& f);

		/** Java record equals: the components, the owner with Object.equals */
		bool operator==(const StatFunctionInfo& other) const;

		std::string toString() const;

		bool isOverrideFunction() const;
	};

	/** Java: getClass().getSimpleName() of a stat function; a run-time RcStatFunction<T> is Java's T */
	static std::string statFunctionClassName(model::stats::calc::functions::IStatFunction& f);
};

} // namespace aion::gameserver::handlers::admincommands
