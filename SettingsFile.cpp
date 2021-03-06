/*
 * Copyright (c) 2021
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "pch.h"
#include "CheckpointPlugin.h"

void CheckpointPlugin::writeSettingsFile() {
	std::ofstream setFile(gameWrapper->GetBakkesModPath() / "plugins" / "settings" / "checkpointplugin.set");
	setFile << R"(Freeplay Checkpoint
9|Bindings - Enter Freeplay, HOLD button you wish to assign and click desired action button
8|
0|                  Freeze (cpt_freeze)                  |cpt_capture_key cpt_freeze
7|
9| [ $cpt_freeze_key$ ]
0|         Checkpoint (cpt_do_checkpoint)       |cpt_capture_key cpt_do_checkpoint
7|
9| [ $cpt_do_checkpoint_key$ ]
0|  Prev. Checkpoint (cpt_prev_checkpoint)  |cpt_capture_key cpt_prev_checkpoint
7|
9| [ $cpt_prev_checkpoint_key$ ]
0|  Next. Checkpoint (cpt_next_checkpoint)  |cpt_capture_key cpt_next_checkpoint
7|
9| [ $cpt_next_checkpoint_key$ ]
0|Freeze Ball/Unfreeze Car (cpt_freeze_ball) |cpt_capture_key cpt_freeze_ball
7|
9| [ $cpt_freeze_ball_key$ ]
0|  Rewind & Fast Forward (cannot change)  |
7|
9| [ Steer Left & Right ]
8|
0|  Apply Bindings  |cpt_apply_bindings
7|
9|(if bindings have never been set)
7|
0| Remove Bindings  |cpt_remove_bindings
7|
9|:(
9|** KBM players: bind cpt_ commands manually in Bakkesmod Bindings tab **
8|
9|
9|Variance - applied when leaving rewind mode
8|
5|Car Direction (degrees)|cpt_variance_car_dir|0|30
5|Car Speed (percent)|cpt_variance_car_spd|0|50
5|Ball Direction (degrees)|cpt_variance_ball_dir|0|30
5|Ball Speed (percent)|cpt_variance_ball_spd|0|50
5|Max. Total Variance|cpt_variance_tot|0|50
8|
9|
9|Auto-reset checkpoint - reset when the following occurs:
8|
1|Goal Scored|cpt_reset_on_goal
1|Ball touches ground|cpt_reset_on_ball_ground
9|
1|Load next checkpoint instead of resetting|cpt_next_instead_of_reset
8|
9|
9|Other options:
8|
1|Ignore cmds except freeze & do_checkpoint when not frozen|cpt_next_prev_when_frozen
1|Clean History -- Deletes future history points when resuming|cpt_clean_history
5|History Length (seconds)|cpt_history_length|10|120
5|History Refresh Rate (ms)|cpt_snapshot_interval|1|10
9|
1|Debug -- Show debugging state|cpt_debug
9|
9|
9| Freeplay Checkpoint
9| Bugs/Feature Requests: github.com/NitrOP7674 -or- on Discord: NitroP#7674
)";
	setFile.close();
	cvarManager->executeCommand("cl_settings_refreshplugins");
}
