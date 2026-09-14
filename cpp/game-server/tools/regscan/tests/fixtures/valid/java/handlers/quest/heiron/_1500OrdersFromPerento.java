package quest.heiron;

import com.aionemu.gameserver.questEngine.handlers.AbstractQuestHandler;

public class _1500OrdersFromPerento extends AbstractQuestHandler {

	public _1500OrdersFromPerento() {
		super(1500);
	}

	@Override
	public void register() {
		qe.registerQuestNpc(204500).addOnQuestStart(questId);
	}
}
