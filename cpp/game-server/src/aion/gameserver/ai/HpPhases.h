#pragma once

#include <cstdint>
#include <initializer_list>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/ai/fwd.h"

namespace aion::gameserver::ai {

/**
 * The HP percentages at which an NPC AI enters its next phase, in descending order and each entered at most once.
 * <p>
 * RefCounted (fieldmap K4: `ai.events.RedNosedGrankerKingAI.hpPhases` keeps one). Java's `HpPhases(int hpPercent, int... moreHpPercents)` is
 * one constructor taking the first percentage and an `std::initializer_list` of the rest (hub-headers.md §7.4), so `HpPhases(50)` keeps its
 * Java syntax.
 *
 * @author Neon
 */
class HpPhases : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	/** Java: public interface PhaseHandler, implemented by the AI handler next to NpcAI */
	class PhaseHandler {
	public:
		virtual void handleHpPhase(int32_t phaseHpPercent) = 0;

	protected:
		~PhaseHandler() = default;
	};

private:
	runtime::ArrayList<int32_t> phaseHpPercents{AION_LOCK_CLASS(HpPhases::phaseHpPercents)};
	runtime::Field<int32_t> currentPhase{0};

protected:
	HpPhases(int32_t hpPercent, std::initializer_list<int32_t> moreHpPercents);
	~HpPhases() override;

public:
	/** Java: new HpPhases(hpPercent, moreHpPercents...) */
	static runtime::Ref<HpPhases> create(int32_t hpPercent, std::initializer_list<int32_t> moreHpPercents = {});

	void reset(); // synchronized (phaseHpPercents)

	/**
	 * Java: `<T extends NpcAI & PhaseHandler> void tryEnterNextPhase(T ai)`. javac erases T to its first bound NpcAI and checkcasts to
	 * PhaseHandler where the AI is used as one, so the parameter is `NpcAI&` and the cast happens at the handleHpPhase call.
	 *
	 * @throws ClassCastException if the AI does not implement PhaseHandler (Java: the erased checkcast)
	 */
	void tryEnterNextPhase(NpcAI& ai); // synchronized (phaseHpPercents)

	/** java-race: Java reads currentPhase without the phaseHpPercents monitor that reset() and tryEnterNextPhase() hold (HpPhases.java:37-39) */
	int32_t getCurrentPhase() const { return currentPhase.get(); }
};

} // namespace aion::gameserver::ai
