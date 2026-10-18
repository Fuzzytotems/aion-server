#include "aion/gameserver/handlers/admincommands/SpawnAssembledNpc.h"

#include <exception>
#include <vector>

#include "aion/commons/utils/Numbers.h"
#include "aion/gameserver/dataholders/AssembledNpcsData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/assemblednpc/AssembledNpc.h"
#include "aion/gameserver/model/assemblednpc/AssembledNpcPart.h"
#include "aion/gameserver/network/aion/serverpackets/SM_NPC_ASSEMBLER.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(SpawnAssembledNpc);

// Java SpawnAssembledNpc.java:21-23
SpawnAssembledNpc::SpawnAssembledNpc() : AdminCommand("spawnAssembledNpc") {
}

// Java SpawnAssembledNpc.java:25-50
void SpawnAssembledNpc::execute(Player& admin, std::span<const std::string> params) {
	if (params.size() != 1) { // parity= if (params.length != 1) {
		info(admin, std::nullopt);
		return;
	}
	int32_t spawnId = 0;
	try {
		spawnId = commons::utils::parseInt(params[0]);
	} catch (const std::exception&) { // parity= } catch (Exception e) {
		info(admin, std::nullopt);
		return;
	}

	const model::templates::assemblednpc::AssembledNpcTemplate* template_ =
		dataholders::DataManager::ASSEMBLED_NPC_DATA->getAssembledNpcTemplate(spawnId);
	if (template_ == nullptr) {
		PacketSendUtility::sendMessage(admin, "This spawnId is wrong.");
		return;
	}
	std::vector<runtime::Ref<model::assemblednpc::AssembledNpcPart>> parts; // parity= List<AssembledNpcPart> assembledParts = new ArrayList<>();
	for (const auto& npcPart : template_->getAssembledNpcPartTemplates()) {
		parts.push_back(model::assemblednpc::AssembledNpcPart::create(utils::idfactory::IDFactory::getInstance().nextId(), &npcPart)); // parity= assembledParts.add(new AssembledNpcPart(IDFactory.getInstance().nextId(), npcPart));
	}
	const std::vector<runtime::Ptr<model::assemblednpc::AssembledNpcPart>> assembledParts(parts.begin(), parts.end()); // parity: C++ the Ptr view of the parts
	runtime::Ref<model::assemblednpc::AssembledNpc> npc = // parity= AssembledNpc npc = new AssembledNpc(template.getRouteId(), template.getMapId(), template.getLiveTime(), assembledParts);
		model::assemblednpc::AssembledNpc::create(template_->getRouteId(), template_->getMapId(), template_->getLiveTime(), assembledParts); // parity: (the same)
	PacketSendUtility::broadcastToWorld(SM_NPC_ASSEMBLER(npc));
}

// Java SpawnAssembledNpc.java:52-55
void SpawnAssembledNpc::info(Player& player, std::optional<std::string_view> /*message*/) {
	PacketSendUtility::sendMessage(player, "syntax //spawnAssembledNpc <spawnId>");
}

} // namespace aion::gameserver::handlers::admincommands
