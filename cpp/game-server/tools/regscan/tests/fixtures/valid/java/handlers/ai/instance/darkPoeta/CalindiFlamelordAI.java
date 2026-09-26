package ai.instance.darkPoeta;

import com.aionemu.gameserver.ai.AIName;
import com.aionemu.gameserver.model.gameobjects.Npc;

/**
 * @AIName("javadoc_is_ignored")
 */
@AIName("calindi_flamelord")
public class CalindiFlamelordAI extends GeneralNpcAI {

	public CalindiFlamelordAI(Npc owner) {
		super(owner);
	}

	private void summonAdds() {
		spawn(700001, 1f, 2f, 3f, (byte) 0);
	}
}
