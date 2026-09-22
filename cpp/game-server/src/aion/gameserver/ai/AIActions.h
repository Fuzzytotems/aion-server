#pragma once

#include <concepts>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::ai {

/**
 * Here will be placed some common AI actions. These methods have access to AI's owner
 * <p>
 * A static-only utility class (hub-headers.md §11.1); the AI parameter is the erased `AbstractAI<? extends Creature>` (§8.1). The
 * `Object... requestParams` of addRequest are the values SM_QUESTION_WINDOW formats, so they take the variadic-template form of §7.4 and
 * forward to the `std::vector<std::string>` overload.
 *
 * @author ATracer
 */
class AIActions {
public:
	AIActions() = delete;

	/**
	 * Despawn and delete owner
	 */
	static void deleteOwner(AbstractAI& ai);

	/**
	 * AI's owner will die
	 */
	static void die(AbstractAI& ai);

	/**
	 * AI's owner will die from specified attacker
	 */
	static void die(AbstractAI& ai, model::gameobjects::Creature& attacker);

	/**
	 * Use skill or add intention to use (will be implemented later)
	 */
	static void useSkill(AbstractAI& ai, int32_t skillId, int32_t level);

	static void useSkill(AbstractAI& ai, int32_t skillId);

	static void targetSelf(AbstractAI& ai);

	static void targetCreature(AbstractAI& ai, model::gameobjects::Creature& target);

	static void handleUseItemFinish(AbstractAI& ai, model::gameobjects::player::Player& player);

	/** registeredPlayers: Java `Collection<Player>`, read by registerDrop */
	static void registerDrop(AbstractAI& ai, model::gameobjects::player::Player& player,
		const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& registeredPlayers);

	static void scheduleRespawn(AbstractAI& ai);

	/**
	 * Add RequestResponseHandler to player, valid within 5 meters around the AI owner (client will auto decline when walking out of range).
	 */
	static void addRequest(AbstractAI& ai, model::gameobjects::player::Player& player, int32_t requestId, AIRequest& request,
		std::vector<std::string> requestParams = {});

	/**
	 * Add RequestResponseHandler to player, valid in the given range around the AI owner (client will auto decline when walking out of range).
	 * For special windows like for artifact activation, this parameter instead controls the reuse cooldown.
	 */
	static void addRequest(AbstractAI& ai, model::gameobjects::player::Player& player, int32_t requestId, int32_t rangeOrCooldownSeconds,
		AIRequest& request, std::vector<std::string> requestParams = {});

	/**
	 * Java: addRequest(ai, player, requestId, request, Object... requestParams) with the values to format (hub-headers.md §7.4). The
	 * conversion is SM_QUESTION_WINDOW's, resolved at the call site through the defaulted template parameter, so this header needs only the
	 * forward declaration of the packet (the pattern of PacketSendUtility).
	 */
	template <class QuestionWindow = network::aion::serverpackets::SM_QUESTION_WINDOW, class... Params>
		requires(sizeof...(Params) > 0 && !(sizeof...(Params) == 1 && (std::same_as<std::remove_cvref_t<Params>, std::vector<std::string>> && ...)))
	static void addRequest(AbstractAI& ai, model::gameobjects::player::Player& player, int32_t requestId, AIRequest& request,
		Params&&... requestParams) {
		addRequest(ai, player, requestId, request, std::vector<std::string>{QuestionWindow::toParam(std::forward<Params>(requestParams))...});
	}

	/** Java: addRequest(ai, player, requestId, rangeOrCooldownSeconds, request, Object... requestParams) (hub-headers.md §7.4) */
	template <class QuestionWindow = network::aion::serverpackets::SM_QUESTION_WINDOW, class... Params>
		requires(sizeof...(Params) > 0 && !(sizeof...(Params) == 1 && (std::same_as<std::remove_cvref_t<Params>, std::vector<std::string>> && ...)))
	static void addRequest(AbstractAI& ai, model::gameobjects::player::Player& player, int32_t requestId, int32_t rangeOrCooldownSeconds,
		AIRequest& request, Params&&... requestParams) {
		addRequest(ai, player, requestId, rangeOrCooldownSeconds, request,
			std::vector<std::string>{QuestionWindow::toParam(std::forward<Params>(requestParams))...});
	}
};

} // namespace aion::gameserver::ai
