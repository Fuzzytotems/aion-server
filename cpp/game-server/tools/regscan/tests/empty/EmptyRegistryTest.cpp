// The *_empty registry libraries: the same accessors as the generated tables, without any handler (and without linking the handler libraries).

#include <gtest/gtest.h>

#include "aion/gameserver/handlers/HandlerRegistry.h"

using namespace aion::gameserver::handlers;

TEST(EmptyRegistryTest, AllTablesAreEmpty) {
	EXPECT_TRUE(aiHandlerEntries().empty());
	EXPECT_TRUE(instanceHandlerEntries().empty());
	EXPECT_TRUE(zoneHandlerEntries().empty());
	EXPECT_TRUE(questHandlerEntries().empty());
	EXPECT_TRUE(commandEntries().empty());
	EXPECT_TRUE(clientPacketEntries().empty());
	EXPECT_TRUE(npcIdsSpawnedByHandlers().empty());
	EXPECT_EQ(findAIHandler(aiHandlerEntries(), "aggressive"), nullptr);
	EXPECT_EQ(findQuestHandler(questHandlerEntries(), 1500), nullptr);
}
