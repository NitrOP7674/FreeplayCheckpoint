/*
 * Copyright (c) 2021
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "pch.h"
#include "CheckpointPlugin.h"

#include "bakkesmod/wrappers/GameEvent/TutorialWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarComponent/BoostWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"

#include "bakkesmod/wrappers/ArrayWrapper.h"

using namespace std::placeholders;

std::string_view DEFAULT_SAVE_FILE_NAME = "freeplaycheckpoint.data";

BAKKESMOD_PLUGIN(CheckpointPlugin, "Freeplay Checkpoint", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

// Controlled by cvars.
float snapshotInterval = 0.010f; //time (s) between updates
int historyTime = 30; // length of history (s)
int maxHistory = int(historyTime / snapshotInterval); // length of history (GameStates)

void CheckpointPlugin::log(std::string s) {
	if (debug) {
		cvarManager->log(s);
	}
}

void CheckpointPlugin::boolvar(std::string name, std::string desc, bool *var) {	
	auto cv = cvarManager->registerCvar(name, "0", desc, true, true, 0, true, 1);
	cv.addOnValueChanged([this, var](std::string old, CVarWrapper now) {
		*var = now.getBoolValue();
	});
	cv.notify();
}

std::unique_ptr<GameState> CheckpointPlugin::getReplayGameState() {
	ReplayServerWrapper replay = gameWrapper->GetGameEventAsReplay();
	if (!replay) {
		cvarManager->log("Error getting replay");
		return nullptr;
	}
	BallWrapper ball = replay.GetBall();
	if (!ball) {
		cvarManager->log("Error getting ball");
		return nullptr;
	}
	CameraWrapper cam = gameWrapper->GetCamera();
	if (!cam) {
		cvarManager->log("Error getting camera");
		return nullptr;
	}
	PriWrapper specPRI = PriWrapper(reinterpret_cast<std::uintptr_t>(cam.GetViewTarget().PRI));
	if (!specPRI) {
		cvarManager->log("Error getting PRI");
		return nullptr;
	}
	std::string playerName = specPRI.GetPlayerName().ToString();

	ArrayWrapper<CarWrapper> cars = replay.GetCars();
	for (int i = 0; i < cars.Count(); i++) {
		CarWrapper car = cars.Get(i);
		if (!car) {
			continue;
		}
		PriWrapper pri = car.GetPRI();
		if (!pri) {
			continue;
		}
		if (playerName == pri.GetPlayerName().ToString()) {
			return std::unique_ptr<GameState>(new GameState(car, ball));
		}
	}
	return nullptr;
}

void CheckpointPlugin::setFrozen(bool car, bool ball) {
	rewindMode = car;
	freezeBall = ball;
	cvarManager->getCvar("cpt_car_frozen").setValue(car);
	cvarManager->getCvar("cpt_ball_frozen").setValue(ball);
}

void CheckpointPlugin::onLoad()
{
	boolvar("cpt_clean_history", "If set, deletes history after the current point when exiting rewind mode", &deleteFutureHistory);

	boolvar("cpt_reset_on_goal", "If set, restore last resumed checkpoint when scoring a goal", &resetOnGoal);
	boolvar("cpt_reset_on_ball_ground", "If set, restore last resumed checkpoint when ball touches ground", &resetOnBallGround);
	boolvar("cpt_next_instead_of_reset", "If set, load next checkpoint instead of resetting", &nextInsteadOfReset);

	boolvar("cpt_debug", "If set, render debugging info", &debug);

	boolvar("cpt_next_prev_when_frozen", "LEGACY; DO NOT USE", &ignorePNNotFrozen);
	boolvar("cpt_ignore_next", "If set, ignore next when not frozen", &ignorePrev);
	boolvar("cpt_ignore_prev", "If set, ignore prev when not frozen", &ignoreNext);
	boolvar("cpt_ignore_freeze_ball", "If set, ignore freeze ball when not frozen", &ignoreFreezeBall);
	boolvar("cpt_disable_training", "If set, disable in custom training", &disableTraining);
	boolvar("cpt_disable_workshop", "If set, disable in workshop", &disableWorkshop);
	boolvar("cpt_show_boost", "If set, show player boost usage while rewinding", &showBoost);

	// Migration from cpt_next_prev_when_frozen to split variables.
	if (ignorePNNotFrozen) {
		cvarManager->getCvar("cpt_next_prev_when_frozen").setValue(false);
		cvarManager->getCvar("cpt_ignore_prev").setValue(true);
		cvarManager->getCvar("cpt_ignore_next").setValue(true);
		cvarManager->getCvar("cpt_ignore_freeze_ball").setValue(true);
	}

	boolvar("cpt_mirror_loads", "If set, randomly mirror when loading checkpoints", &mirrorLoads);
	boolvar("cpt_randomize_loads", "If set, load a random checkpoint instead of the latest", &randomizeLoads);

	cvarManager->registerCvar("cpt_allow_delete_all", "0", "Enables the delete all button", false, true, 0, true, 1, false);

	cvarManager->registerCvar("cpt_car_frozen", "0", "Set when the car is frozen; read-only", false, true, 0, true, 1, false);
	cvarManager->registerCvar("cpt_ball_frozen", "0", "Set when the ball is frozen; read-only", false, true, 0, true, 1, false);

	auto snapshotIntervalCV = cvarManager->registerCvar(
		"cpt_snapshot_interval", "1", "Collect a snapshot every <n> milliseconds; changing deletes history", true, true, 1, true, 10, true);
	snapshotIntervalCV.addOnValueChanged([this](std::string old, CVarWrapper now) {
		snapshotInterval = now.getIntValue()/100.0f;
		maxHistory = int(historyTime / snapshotInterval);
		history.resize(0);
		setFrozen(false, false);
		dodgeExpiration = 0.0;
	});
	snapshotIntervalCV.notify();

	auto historyLenCV = cvarManager->registerCvar(
		"cpt_history_length", "30", "Save history for <n> seconds", true, true, 10, true, 120, true);
	historyLenCV.addOnValueChanged([this](std::string old, CVarWrapper now) {
		historyTime = now.getIntValue();
		maxHistory = int(historyTime / snapshotInterval);
		if (history.size() > maxHistory) {
			history.erase(history.begin(), history.begin() + history.size() - maxHistory);
		}
	});
	historyLenCV.notify();

	auto filenameCV = cvarManager->registerCvar(
		"cpt_filename", static_cast<std::string>(DEFAULT_SAVE_FILE_NAME), "Sets the filename to use for saved checkpoints", true, false, 0, false, 0, true);
	filenameCV.addOnValueChanged([this](std::string old, CVarWrapper now) {
		setFrozen(false, false);
		curCheckpoint = 0;
		loadCheckpointFile();
	});
	snapshotIntervalCV.notify();

	auto resetDelayCV = cvarManager->registerCvar(
		"cpt_load_after_reset", "0", "Load last checkpoint on reset if loaded within last N seconds", true, true, 0, false, 0, true);

	registerVarianceCVars();

	loadCheckpointFile();

	// Continually call OnPreAsync.
	gameWrapper->HookEvent("Function PlayerController_TA.Driving.PlayerMove",
		bind(&CheckpointPlugin::OnPreAsync, this, _1));

	// Disable rewind mode if the user resets freeplay.
	// Or load latest checkpoint if within N seconds.
	gameWrapper->HookEvent("Function GameEvent_TA.Countdown.BeginState",
		[this](std::string eventName) {
			if (!enabledLoads()) {
				return;
			}
			int resetDelay = cvarManager->getCvar("cpt_load_after_reset").getIntValue();
			if (!rewindMode && playingFromCheckpoint && resetDelay > 0) {
				ServerWrapper sw = gameWrapper->GetGameEventAsServer();
				float lastLoad = sw.GetSecondsElapsed() - lastRewindTime;
				if (lastLoad > 0 && lastLoad < resetDelay) {
					loadLatestCheckpoint();
					return;
				}
			}
			playingFromCheckpoint = false;
			setFrozen(false, false);
			dodgeExpiration = 0.0;
		});

	gameWrapper->HookEvent("Function TAGame.Ball_TA.OnHitGoal",
		[this](std::string eventName) {
			if (!gameWrapper->IsInFreeplay() || rewindMode || !playingFromCheckpoint || !resetOnGoal) {
				return;
			}
			loadLatestCheckpoint();
		});

	// Enter rewind mode.
	cvarManager->registerNotifier("cpt_freeze", [this](std::vector<std::string> command) {
		if (!enabled() || history.size() == 0 || rewindMode || gameWrapper->IsInReplay()) {
			return;
		}
		latest = history.back();
		loadGameState(latest);
	}, "Activates rewind mode", PERMISSION_ALL);

	// If in play mode, load the latest checkpoint / quick checkpoint.
	// If in rewind mode, add a checkpoint or delete the current checkpoint.
	cvarManager->registerNotifier("cpt_do_checkpoint", std::bind(&CheckpointPlugin::doCheckpoint, this, _1), "Saves/restores/removes a checkpoint", PERMISSION_ALL);

	cvarManager->registerNotifier("cpt_lock_checkpoint", std::bind(&CheckpointPlugin::lockCheckpoint, this, _1), "Lock/unlock a checkpoint", PERMISSION_FREEPLAY);
	cvarManager->registerNotifier("cpt_prev_checkpoint", std::bind(&CheckpointPlugin::prevCheckpoint, this, _1), "Loads the previous checkpoint", PERMISSION_FREEPLAY);
	cvarManager->registerNotifier("cpt_next_checkpoint", std::bind(&CheckpointPlugin::nextCheckpoint, this, _1), "Loads the next checkpoint", PERMISSION_FREEPLAY);
	cvarManager->registerNotifier("cpt_rand_checkpoint", std::bind(&CheckpointPlugin::randCheckpoint, this, _1), "Restores a random saved checkpoint", PERMISSION_FREEPLAY);
	cvarManager->registerNotifier("cpt_delete_all", std::bind(&CheckpointPlugin::deleteAllCheckpoints, this, _1), "Deletes ALL checkpoints", PERMISSION_ALL);
	cvarManager->registerNotifier("cpt_mirror_state", std::bind(&CheckpointPlugin::mirrorState, this, _1), "Mirrors the current frozen state", PERMISSION_FREEPLAY);
	cvarManager->registerNotifier("cpt_freeze_ball", std::bind(&CheckpointPlugin::freezeBallUnfreezeCar, this, _1), "Freezes/unfreezes the ball", PERMISSION_FREEPLAY);
	cvarManager->registerNotifier("cpt_copy", std::bind(&CheckpointPlugin::copyShot, this, _1), "Copies the frozen state / quick checkpoint / last checkpoint to the clipboard", PERMISSION_ALL);
	cvarManager->registerNotifier("cpt_paste", std::bind(&CheckpointPlugin::pasteShot, this, _1), "Loads a checkpoint from the clipboard as a quick checkpoint", PERMISSION_FREEPLAY);

	// Add default bindings.
	registerBindingCVars();

	// Draw the checkpoint or notification about checkpoint deletion.
	gameWrapper->RegisterDrawable(std::bind(&CheckpointPlugin::Render, this, std::placeholders::_1));

	writeSettingsFile();
}

bool CheckpointPlugin::enabled() {
	if (gameWrapper->IsInReplay()) {
		// Replays may be paused when checkpoints are taken.
		return true;
	}
	if (gameWrapper->IsPaused()) {
		// Don't allow checkpoint operations while paused.
		return false;
	}
	if (!disableTraining && gameWrapper->IsInCustomTraining()) {
		return true;
	}
	if (!gameWrapper->IsInFreeplay()) {
		return false;
	}
	return !disableWorkshop || PlaylistIds(gameWrapper->GetGameEventAsServer().GetPlaylist().GetPlaylistId()) != PlaylistIds::Workshop;
}

bool CheckpointPlugin::enabledLoads() {
	if (gameWrapper->IsPaused()) {
		// Don't allow checkpoint operations while paused.
		return false;
	}
	if (!gameWrapper->IsInFreeplay()) {
		return false;
	}
	return !disableWorkshop || PlaylistIds(gameWrapper->GetGameEventAsServer().GetPlaylist().GetPlaylistId()) != PlaylistIds::Workshop;
}

void CheckpointPlugin::copyShot(std::vector<std::string> command) {
	std::string output = "";
	if (gameWrapper->IsInReplay()) {
		std::unique_ptr<GameState> gs = getReplayGameState();
		if (gs == nullptr) {
			return;
		}
		output = gs->toString();
	} else if (rewindMode) {
		cvarManager->log("Copying current position");
		output = latest.toString();
	} else if (hasQuickCheckpoint) {
		cvarManager->log("Copying quick checkpoint");
		output = quickCheckpoint.toString();
	} else if (checkpoints.size() > 0) {
		cvarManager->log("Copying checkpoint " + std::to_string(curCheckpoint + 1));
		output = checkpoints.at(curCheckpoint).toString();
	} else {
		cvarManager->log("No checkpoint to copy!");
		return;
	}
	output = "cpv1" + output + ".";
	OpenClipboard(nullptr);
	EmptyClipboard();
	HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, output.size() + 1);
	if (hg == nullptr) {
		cvarManager->log("Error copying to clipboard!");
		CloseClipboard();
		return;
	}
	LPVOID lptstrCopy = GlobalLock(hg);
	if (lptstrCopy == nullptr) {
		cvarManager->log("Error copying to clipboard!");
		CloseClipboard();
		return;
	}
	memcpy(lptstrCopy, output.c_str(), output.size() + 1);
	GlobalUnlock(hg);
	SetClipboardData(CF_TEXT, hg);
	CloseClipboard();
	GlobalFree(hg);
	cvarManager->log("Data copied to clipboard!");
	log("Written to clipboard: " + output);
}

void CheckpointPlugin::pasteShot(std::vector<std::string> command) {
	if (!enabledLoads()) {
		return;
	}
	OpenClipboard(nullptr);
	HANDLE hData = GetClipboardData(CF_TEXT);
	if (hData == nullptr) {
		cvarManager->log("Error reading clipboard!");
		return;
	}
	char* pszText = static_cast<char*>(GlobalLock(hData));
	if (pszText == nullptr) {
		cvarManager->log("Error reading clipboard!");
		return;
	}
	std::string input(pszText);
	GlobalUnlock(hData);
	CloseClipboard();
	log("Read from clipboard: " + input);
	if (input.substr(0, 4) != "cpv1" || input[input.size() - 1] != '.') {
		cvarManager->log("Malformed checkpoint in clipboard: " + input);
		return;
	}
	quickCheckpoint = GameState(input.substr(4, input.size() - 5));
	loadGameState(quickCheckpoint);
	hasQuickCheckpoint = true;
	rewindState.justLoadedQuickCheckpoint = true;
}

void CheckpointPlugin::freezeBallUnfreezeCar(std::vector<std::string> command) {
	if (!enabledLoads() || history.size() == 0) {
		return;
	}
	if (rewindMode) {
		setFrozen(false, true);
		return;
	}
	if (freezeBall) {
		setFrozen(false, false);
		latest.car = history.back().car;
		latest.apply(gameWrapper, showBoost);
		quickCheckpoint = latest;
		hasQuickCheckpoint = true;
		return;
	}
	if (ignoreFreezeBall) {
		return;
	}
	latest = history.back();
	latest.apply(gameWrapper, false);
	setFrozen(false, true);
}

void CheckpointPlugin::mirrorState(std::vector<std::string> command) {
	if (!enabledLoads() || !rewindMode) {
		return;
	}
	rewindState.atCheckpoint = false;
	hasQuickCheckpoint = true;
	quickCheckpoint = latest.mirror();
	loadLatestCheckpoint();
}

void CheckpointPlugin::deleteAllCheckpoints(std::vector<std::string> command) {
	if (!cvarManager->getCvar("cpt_allow_delete_all").getBoolValue()) {
		return;
	}
	cvarManager->getCvar("cpt_allow_delete_all").setValue("0");
	checkpoints.resize(0);
	locks.resize(0);
	curCheckpoint = 0;
	saveCheckpointFile();
}

void CheckpointPlugin::randCheckpoint(std::vector<std::string> command) {
	if (!enabledLoads()) {
		return;
	}
	loadRandomCheckpoint();
}

void CheckpointPlugin::prevCheckpoint(std::vector<std::string> command) {
	if (!enabledLoads() || checkpoints.size() == 0) {
		return;
	}
	if (ignorePrev && !rewindMode) {
		return;
	}
	if (!rewindState.justDeletedCheckpoint) {
		// If you just deleted a checkpoint, prev should go one prior to
		// the deleted one (the current one).
		if (curCheckpoint == 0) {
			curCheckpoint = checkpoints.size() - 1;
		} else {
			curCheckpoint--;
		}
	}
	loadCurCheckpoint();
}

void CheckpointPlugin::nextCheckpoint(std::vector<std::string> command) {
	if (!enabledLoads() || checkpoints.size() == 0) {
		return;
	}
	if (ignoreNext && !rewindMode) {
		return;
	}
	curCheckpoint++;
	if (curCheckpoint == checkpoints.size()) {
		curCheckpoint = 0;
	}
	loadCurCheckpoint();
}

void CheckpointPlugin::lockCheckpoint(std::vector<std::string> command) {
	if (gameWrapper->IsPaused() || !rewindMode || !rewindState.atCheckpoint) {
		return;
	}
	rewindState.deleting = false;
	if (locks.size() <= curCheckpoint) {
		locks.resize(curCheckpoint + 1);
	}
	if (locks[curCheckpoint]) {
		log("at cpt; unlocking: " + std::to_string(curCheckpoint + 1));
	} else {
		log("at cpt; locking: " + std::to_string(curCheckpoint + 1));
	}
	locks[curCheckpoint] = !locks[curCheckpoint];
	saveCheckpointFile();
}

void CheckpointPlugin::doCheckpoint(std::vector<std::string> command) {
	{
		if (!enabled()) {
			return;
		}
		if (gameWrapper->IsInReplay()) {
			std::unique_ptr<GameState> gs = getReplayGameState();
			if (gs == nullptr) {
				return;
			}
			cvarManager->log("adding checkpoint " + std::to_string(checkpoints.size() + 1));
			checkpoints.push_back(*gs);
			saveCheckpointFile();
			return;
		}
		if (gameWrapper->IsInCustomTraining()) {
			// Only support loading the quick checkpoint for now.
			if (hasQuickCheckpoint) {
				loadLatestCheckpoint();
			}
			return;
		}
		if (!rewindMode) {
			if (randomizeLoads) {
				loadRandomCheckpoint();
				return;
			}
			loadLatestCheckpoint();
			return;
		}
		hasQuickCheckpoint = false;
		if (rewindState.atCheckpoint) { // Delete the current checkpoint we are at.
			if (locks.size() > curCheckpoint && locks[curCheckpoint]) {
				log("at cpt but locked: " + std::to_string(curCheckpoint + 1));
				return;
			}
			if (!rewindState.deleting) {
				rewindState.deleting = true;
				return;
			}
			rewindState.deleting = false;
			log("at cpt; removing: " + std::to_string(curCheckpoint + 1));
			checkpoints.erase(checkpoints.begin() + curCheckpoint);
			if (locks.size() > curCheckpoint) {
				locks.erase(locks.begin() + curCheckpoint);
			}
			curCheckpoint = std::min(curCheckpoint, checkpoints.size() - 1);
			rewindState.atCheckpoint = false;
			rewindState.justDeletedCheckpoint = true;
			saveCheckpointFile();
			return;
		}
		// Add a new checkpoint here.
		log("adding checkpoint " + std::to_string(checkpoints.size() + 1));
		curCheckpoint = checkpoints.size();
		checkpoints.push_back(latest);
		saveCheckpointFile();
		loadGameState(latest);
		rewindState.atCheckpoint = true;
	}
}

void CheckpointPlugin::registerVarianceCVars() {
	cvarManager->registerCvar("cpt_variance_car_dir", "0", "If set, randomly vary car's direction when resuming", true, true, 0, true, 30, true);
	cvarManager->registerCvar("cpt_variance_car_spd", "0", "If set, randomly vary car's speed when resuming", true, true, 0, true, 50, true);
	cvarManager->registerCvar("cpt_variance_car_rot", "0", "If set, randomly vary car's rotation when resuming", true, true, 0, true, 10, true);
	cvarManager->registerCvar("cpt_variance_ball_dir", "0", "If set, randomly vary ball's direction when resuming", true, true, 0, true, 30, true);
	cvarManager->registerCvar("cpt_variance_ball_spd", "0", "If set, randomly vary ball's speed when resuming", true, true, 0, true, 50, true);
	cvarManager->registerCvar("cpt_variance_ball_rot", "0", "If set, randomly vary ball's rotation when resuming", true, true, 0, true, 10, true);
	cvarManager->registerCvar("cpt_variance_tot", "0", "Total variance applied to all factors (range)", true, true, 0, true, 50, true);
}

void CheckpointPlugin::onUnload() {
}

void CheckpointPlugin::loadLatestCheckpoint() {
	if (hasQuickCheckpoint) {
		log("loading quick checkpoint");
		loadGameState(quickCheckpoint);
		hasQuickCheckpoint = true;
		rewindState.justLoadedQuickCheckpoint = true;
		return;
	}
	if (checkpoints.size() > 0) {
		log("loading checkpoint " + std::to_string(curCheckpoint+1));
		loadCurCheckpoint();
		return;
	}
	log("no checkpoint to load");
	rewindState.virtualTimeOffset = 0;
	rewindState.holdingFor = 0;
}

void CheckpointPlugin::loadRandomCheckpoint() {
	if (checkpoints.size() == 0) {
		return;
	}
	hasQuickCheckpoint = false;
	curCheckpoint = rand() % checkpoints.size();
	loadLatestCheckpoint();
}

void CheckpointPlugin::loadCurCheckpoint() {
	auto checkpoint = checkpoints.at(curCheckpoint);
	if (mirrorLoads && rand() % 2 == 0) {
		checkpoint = checkpoint.mirror();
	}
	loadGameState(checkpoint);
	rewindState.atCheckpoint = true;
}

void CheckpointPlugin::loadGameState(const GameState& state) {
	latest = state;
	ServerWrapper sw = gameWrapper->GetGameEventAsServer();
	if (cvarManager->getCvar("sv_soccar_enablegoal").getBoolValue()) {
		sw.PlayerResetTraining(); // In case a goal was just scored, there may be no ball.
	}
	latest.apply(gameWrapper, false);
	rewindState.virtualTimeOffset = 0;
	rewindState.holdingFor = 0;
	setFrozen(true, true);
	rewindState.atCheckpoint = false;
	hasQuickCheckpoint = false;
	rewindState.justDeletedCheckpoint = false;
	rewindState.justLoadedQuickCheckpoint = false;
	rewindState.deleting = false;
	rewindState.buttonsDown = 0x7f;
	playingFromCheckpoint = true; // not playing yet but must resume eventually.
}

void CheckpointPlugin::OnPreAsync(std::string funcName)
{
	if (!gameWrapper->IsInFreeplay() && !gameWrapper->IsInCustomTraining()) {
		return;
	}
	ServerWrapper sw = gameWrapper->GetGameEventAsServer();
	if (sw.GetBall().IsNull() || sw.GetGameCar().IsNull()) {
		return;
	}

	if (rewindMode) {
		if (rewind(sw)) {
			applyVariance(latest).apply(gameWrapper, showBoost);
		}
	} else {
		record(sw);
	}
}

// Returns true if we need to apply the state again.
bool CheckpointPlugin::rewind(ServerWrapper sw) {
	ControllerInput ci = sw.GetCars().Get(0).GetInput();

	float currentTime = sw.GetSecondsElapsed();
	float elapsed = std::min(currentTime - lastRewindTime, 0.03f);
	if (elapsed < 0) {
		lastRewindTime = currentTime;
		return false;  // Ignored whatever inputs may have happened to exit mode; do not apply state.
	}
	if (elapsed < 0.01f) {
		return false;  // Ignored whatever inputs may have happened to exit mode; do not apply state.
	}
	lastRewindTime = currentTime;
	int buttonsDown = (abs(ci.Throttle) > 0.1 ? 0x01 : 0) |
		(abs(ci.Roll) > 0.1 ? 0x02 : 0) |
		(ci.Handbrake ? 0x04 : 0) |
		(ci.Jump ? 0x08 : 0) |
		(ci.ActivateBoost ? 0x10 : 0) |
		(ci.HoldingBoost ? 0x20 : 0) |
		((rewindState.atCheckpoint || rewindState.justLoadedQuickCheckpoint) && abs(ci.Steer) >= .05 ? 0x40 : 0) |
		((rewindState.atCheckpoint || rewindState.justLoadedQuickCheckpoint || abs(ci.Pitch) >= .7) && abs(ci.Pitch) >= .05 ? 0x80 : 0);
	// See if we should exit rewind mode due to input.
	if (buttonsDown != 0) {
		if ((buttonsDown > rewindState.buttonsDown && currentTime - lastRecordTime > 0.1f) ||
			currentTime - lastRecordTime > 0.5f) {
			log("resuming...");
			setFrozen(false, false);
			lastRecordTime = currentTime;
			dodgeExpiration = (latest.car.hasDodge && latest.car.lastJumped != -1) ? (currentTime + MAX_DODGE_TIME - latest.car.lastJumped) : 0;
			if (!rewindState.atCheckpoint) {
				log("quick checkpoint taken");
				hasQuickCheckpoint = true;
				quickCheckpoint = latest;
				if (deleteFutureHistory) {
					size_t current = std::clamp<size_t>(
						history.size() - 1 + size_t(ceil(rewindState.virtualTimeOffset / snapshotInterval)),
						0, history.size() - 1);
					history.erase(history.begin() + current, history.end());
				}
			}
			return false; // Leaving rewind; do not apply state.
		}
		rewindState.buttonsDown = buttonsDown;
		return true; // Staying in rewind; apply state.
	}
	rewindState.buttonsDown = buttonsDown;

	// Determine how much to rewind / advance time.
	if (abs(ci.Steer) < .05f) { // Ignore slight input; keep current game state.
		return true; // Ignoring input; apply state.
	}
	rewindState.deleting = false;
	if (ci.Steer < -.95 && rewindState.holdingFor <= 0) {
		rewindState.holdingFor -= elapsed;
	} else if (ci.Steer > .95 && rewindState.holdingFor >= 0) {
		rewindState.holdingFor += elapsed;
	} else {
		rewindState.holdingFor = 0;
	}
	float factor = std::clamp(abs(rewindState.holdingFor) * 2, 1.0f, 10.0f);

	// How much (in seconds) to move "current" (positive or negative)
	float deltaElapsed = factor * elapsed * ci.Steer; // full left = 2-5 seconds/second

	rewindState.virtualTimeOffset = std::clamp(
		rewindState.virtualTimeOffset + deltaElapsed, -snapshotInterval * history.size(), .0f);
	float historyOffset = rewindState.virtualTimeOffset / snapshotInterval;
	size_t current = std::clamp<size_t>(
		history.size() + size_t(floor(historyOffset)), 0, history.size() - 1);
	if (current < (history.size() - 1) /* && NEED TO INTERPOLATE */) {
		float advancePct = 1 - (historyOffset - floor(historyOffset));
		latest = GameState(history.at(current), history.at(current+1), advancePct);
		return true; // Apply new state.
	}
	latest = history.at(current);
	return true; // Apply new state.
}

void CheckpointPlugin::record(ServerWrapper sw)
{
	float currentTime = sw.GetSecondsElapsed();
	float elapsed = currentTime - lastRecordTime;
	if (elapsed < 0) {
		elapsed = snapshotInterval;
	}
	if (elapsed < snapshotInterval) {
		return;
	}
	// This cannot be event-based since goals may be disabled.
	if (playingFromCheckpoint && (resetOnGoal || resetOnBallGround)) {
		auto ball = sw.GetBall();
		if (ball.IsNull()) {
			return;
		}
		auto ballLoc = ball.GetLocation();
		auto ballRad = ball.GetRadius();
		if ((resetOnGoal && sw.IsInGoal(ballLoc)) ||
			(resetOnBallGround && ballLoc.Z < ballRad + 5)) {
			if (nextInsteadOfReset && !hasQuickCheckpoint && checkpoints.size() > 0) {
				if (randomizeLoads) {
					loadRandomCheckpoint();
					return;
				}
				curCheckpoint++;
				if (curCheckpoint == checkpoints.size()) {
					curCheckpoint = 0;
				}
				loadCurCheckpoint();
				return;
			}
			loadLatestCheckpoint();
			return;
		}
	}

	lastRecordTime = currentTime;
	if (dodgeExpiration != 0) {
		// If the timer expires or if the player double-jumps or gets a reset,
		// clear the jump timer so we don't take the player's dodge.
		auto c = sw.GetGameCar();
		if (c && (currentTime > dodgeExpiration ||
				  c.GetbDoubleJumped() ||
				  c.GetNumWheelContacts() == 4)) {
			c.SetbJumped(true);
			c.SetbDoubleJumped(true);
			dodgeExpiration = 0;
		}
	}

	if (freezeBall) {
		latest.ball.apply(sw.GetBall());
	}

	// TODO: use a ring buffer?
	if (history.size() == maxHistory) {
		history.erase(history.begin());
	}
	if (dodgeExpiration == 0) {
		history.emplace_back(gameWrapper);
	} else {
		history.emplace_back(gameWrapper, MAX_DODGE_TIME - currentTime + dodgeExpiration);
	}
}

void show(CanvasWrapper canvas, Vector2 *loc, std::string s) {
	static const float scale = 1.5f;
	canvas.SetPosition(*loc);
	canvas.DrawString(s, scale, scale);
	loc->Y += 20;
}

void CheckpointPlugin::Render(CanvasWrapper canvas) {
	if (!enabled()) {
		return;
	}
	if (debug) {
		canvas.SetColor('\xff', '\xff', '\xff', '\xdc');
		auto screenSize = canvas.GetSize();
		Vector2 loc = { (int)(screenSize.X * 0.08), (int)(screenSize.Y * 0.08) };
		show(canvas, &loc, "rewindMode: " + std::to_string(rewindMode));
		show(canvas, &loc, "atCheckpoint: " + std::to_string(rewindState.atCheckpoint));
		show(canvas, &loc, "justDeletedCheckpoint: " + std::to_string(rewindState.justDeletedCheckpoint));
		show(canvas, &loc, "justLoadedQuickCheckpoint: " + std::to_string(rewindState.justLoadedQuickCheckpoint));
		show(canvas, &loc, "hasQuickCheckpoint: " + std::to_string(hasQuickCheckpoint));
		show(canvas, &loc, "virtualTimeOffset: " + std::to_string(rewindState.virtualTimeOffset));
		show(canvas, &loc, "buttonsDown: " + std::to_string(rewindState.buttonsDown));
		size_t current = std::clamp<size_t>(
			history.size() + size_t(ceil(rewindState.virtualTimeOffset / snapshotInterval)),
			0, history.size() - 1);
		show(canvas, &loc, "current: " + std::to_string(current));
	}
	if (!rewindMode) {
		return;
	}
	if (rewindState.deleting) {
		auto screenSize = canvas.GetSize();
		Vector2 loc = { (int)(screenSize.X * 0.80), (int)(screenSize.Y * 0.08) };
		loc.X = int(screenSize.X * .70);
		canvas.SetPosition(loc);
		canvas.SetColor('\xff', '\xff', '\xff', '\xdc');
		canvas.DrawString("Press again to delete...", 5, 5);
		return;
	}
	if (rewindState.justDeletedCheckpoint) {
		auto screenSize = canvas.GetSize();
		Vector2 loc = { (int)(screenSize.X * 0.80), (int)(screenSize.Y * 0.08) };
		loc.X = int(screenSize.X * .70);
		canvas.SetPosition(loc);
		canvas.SetColor('\xff', '\xff', '\xff', '\xdc');
		canvas.DrawString("Checkpoint deleted!", 5, 5);
		return;
	}
	if (rewindState.atCheckpoint) {
		auto screenSize = canvas.GetSize();
		std::string l = "";
		if (locks.size() > curCheckpoint && locks[curCheckpoint]) {
			l = " (L)";
		}
		Vector2 loc = { (int)(screenSize.X * 0.80), (int)(screenSize.Y * 0.08) };
		canvas.SetPosition(loc + Vector2{ 5,5 });
		canvas.SetColor(0, 0, 0, 100);
		canvas.DrawString(std::to_string(curCheckpoint + 1) +
			" | " +
			std::to_string(checkpoints.size()) + l, 6, 6);
		canvas.SetPosition(loc);
		canvas.SetColor('\xff', '\xff', '\xff', '\xdc');
		canvas.DrawString(std::to_string(curCheckpoint + 1) +
			" | " +
			std::to_string(checkpoints.size()) + l, 6, 6);
	}
}

// Prevent loading an unknown version's save file.
constexpr uint32_t SAVE_FILE_VERSION = 1;

void CheckpointPlugin::loadCheckpointFile() {
	checkpoints.clear();
	locks.clear();
	std::ifstream in(gameWrapper->GetDataFolder() / cvarManager->getCvar("cpt_filename").getStringValue(), std::ios::binary);
	uint32_t version;
	readPOD(in, version);
	if (version != SAVE_FILE_VERSION) {
		in.close();
		log("could not load save file with version " + std::to_string(version));
		return;
	}
	int32_t numSaves;
	readPOD(in, numSaves);
	for (int32_t i = 0; i < numSaves; i++) {
		checkpoints.emplace_back(in);
	}
	int32_t numLocks = 0; // older save files did not have this data; initialize to 0.
	readPOD(in, numLocks);
	for (int32_t i = 0; i < numLocks; i++) {
		bool locked;
		readPOD(in, locked);
		locks.push_back(locked);
	}
	in.close();
}

void CheckpointPlugin::saveCheckpointFile() {
	std::ofstream out(gameWrapper->GetDataFolder() / cvarManager->getCvar("cpt_filename").getStringValue(), std::ios::binary | std::ios::out | std::ios::trunc);
	auto ver = SAVE_FILE_VERSION;
	writePOD(out, ver);
	auto size = int32_t(checkpoints.size());
	writePOD(out, size);
	for (auto& fav : checkpoints) {
		fav.write(out);
	}
	size = int32_t(locks.size());
	writePOD(out, size);
	for (bool l : locks) {
		writePOD(out, l);
	}
	out.close();
}
