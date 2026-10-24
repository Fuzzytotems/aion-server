// fix/cpp-roah-training-epoch (owner, 2026-10-19): the Eternal Challenge's model training (PlayerModelController::runTraining, the body of
// trainModelForPlayer's long-running task) must not hold one reclaimer epoch for the whole training. The play kit logged "Reclamation lag
// 10200 ms: thread 110 (long-running at .../PlayerModelController.cpp:35)". The training of a realistic size runs on its own thread in a
// LONG_RUNNING task scope, as the long-running pool runs it, while this thread scans and reads the reclaimer's lag.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/custom/instance/neuralnetwork/DataSet.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModel.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModelController.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/ThreadContext.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::custom::instance::neuralnetwork {
namespace {

using runtime::Ref;

// a realistic model: 25 skills (toStateInputArray: 23 state values + the skills; toActionOutputArray: one target per skill), 30 recorded
// entries and PlayerModelController's 1000 epochs - about 15 s in a Debug build, the play kit's ~10 s training (a Release build trains a
// run's few hundred entries in that time)
constexpr int32_t SKILLS = 25;
constexpr int32_t INPUTS = 23 + SKILLS;
constexpr int32_t ENTRIES = 30;
constexpr int32_t EPOCHS = 1000;

std::vector<Ref<DataSet>> dataSets() {
	std::vector<Ref<DataSet>> sets;
	for (int32_t e = 0; e < ENTRIES; e++) {
		std::vector<double> values(INPUTS);
		std::vector<double> targets(SKILLS, 0.0);
		for (int32_t i = 0; i < INPUTS; i++)
			values[i] = static_cast<double>((e * 31 + i * 17) % 100) / 100.0;
		targets[e % SKILLS] = 1.0;
		sets.push_back(DataSet::create(values, targets));
	}
	return sets;
}

Ref<PlayerModel> model(uint64_t seed) {
	commons::utils::Rnd::seedCurrentThreadForTests(seed);
	return PlayerModel::create(INPUTS, 10, SKILLS, 1, std::nullopt, std::nullopt); // PlayerModelController.trainModelForPlayer's shape
}

std::vector<double> estimate(PlayerModel& m, const Ref<DataSet>& input) {
	return m.getOutputEstimation(input->getValues());
}

/** the training releases its epoch between training epochs: the reclaimer's lag stays far below the training's duration */
TEST(PlayerModelTrainingEpochTest, TheTrainingDoesNotHoldTheReclaimerEpochForItsWholeDuration) {
	runtime::Reclaimer& reclaimer = runtime::Reclaimer::getInstance();
	std::vector<Ref<DataSet>> sets;
	Ref<PlayerModel> trained;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		sets = dataSets();
		trained = model(91);
	}
	reclaimer.drain();

	std::atomic<bool> done{false};
	std::atomic<uint64_t> trainingThread{0};
	const auto start = std::chrono::steady_clock::now();
	std::thread training([&] {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::LONG_RUNNING)); // the long-running pool's task scope
		trainingThread = runtime::ThreadContext::current().threadId();
		PlayerModelController::runTraining(trained, sets, EPOCHS);
		done = true;
	});
	int64_t maxLag = 0;
	int32_t samples = 0;
	while (!done.load()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		reclaimer.reclaimNow();
		runtime::Reclaimer::Stats stats = reclaimer.stats();
		if (stats.oldestPublishedThreadId == trainingThread.load())
			maxLag = std::max<int64_t>(maxLag, stats.lag.count());
		samples++;
	}
	training.join();
	const int64_t trainingMillis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

	ASSERT_GE(trainingMillis, 2000) << "the training must last long enough to tell a held epoch from released ones";
	EXPECT_GE(samples, 20);
	EXPECT_LT(maxLag, 1000) << "the training held one epoch for " << maxLag << " ms of its " << trainingMillis << " ms";
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	EXPECT_TRUE(trained->isReady.get()) << "train finished and published the model as Java does";
}

/** runTraining trains exactly as train does: the same seed and the same data give the same weights, so the same estimations */
TEST(PlayerModelTrainingEpochTest, RunTrainingGivesTheSameModelAsTrain) {
	std::vector<Ref<DataSet>> sets;
	Ref<PlayerModel> plain;
	Ref<PlayerModel> quiescent;
	{
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		sets = dataSets();
		plain = model(4711);
		quiescent = model(4711);
	}
	std::thread first([&] {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::LONG_RUNNING));
		commons::utils::Rnd::seedCurrentThreadForTests(5);
		plain->train(sets, 50);
	});
	first.join();
	std::thread second([&] {
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::LONG_RUNNING));
		commons::utils::Rnd::seedCurrentThreadForTests(5);
		PlayerModelController::runTraining(quiescent, sets, 50);
	});
	second.join();
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	for (int32_t e = 0; e < ENTRIES; e += 7)
		EXPECT_EQ(estimate(*plain, sets[e]), estimate(*quiescent, sets[e])) << "entry " << e;
}

} // namespace
} // namespace aion::gameserver::custom::instance::neuralnetwork
