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

void CheckpointPlugin::onLoad()
{
	loadCheckpointFile();

	boolvar("cpt_clean_history", "If set, deletes history after the current point when exiting rewind mode", &deleteFutureHistory);

	boolvar("cpt_reset_on_goal", "If set, restore last resumed checkpoint when scoring a goal", &resetOnGoal);
	boolvar("cpt_reset_on_ball_ground", "If set, restore last resumed checkpoint when ball touches ground", &resetOnBallGround);
	boolvar("cpt_next_instead_of_reset", "If set, load next checkpoint instead of resetting", &nextInsteadOfReset);

	boolvar("cpt_debug", "If set, render debugging info", &debug);

	boolvar("cpt_next_prev_when_frozen", "If set, ignore next/prev when not frozen", &ignorePNNotFrozen);

	auto snapshotIntervalCV = cvarManager->registerCvar(
		"cpt_snapshot_interval", "1", "Collect a snapshot every <n> milliseconds; changing deletes history", true, true, 1, true, 10, true);
	snapshotIntervalCV.addOnValueChanged([this](std::string old, CVarWrapper now) {
		snapshotInterval = now.getIntValue()/100.0f;
		maxHistory = int(historyTime / snapshotInterval);
		history.resize(0);
		rewindMode = false;
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

	registerVarianceCVars();

	// Continually call OnPreAsync.
	gameWrapper->HookEvent("Function PlayerController_TA.Driving.PlayerMove",
		bind(&CheckpointPlugin::OnPreAsync, this, _1));

	// Disable rewind mode if the user resets freeplay.
	gameWrapper->HookEvent("Function GameEvent_TA.Countdown.BeginState",
		[this](std::string eventName) {
			playingFromCheckpoint = false;
			rewindMode = false;
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
		if (!gameWrapper->IsInFreeplay() || gameWrapper->IsPaused() || history.size() == 0 || rewindMode) {
			return;
		}
		latest = history.back();
		loadGameState(latest);
	}, "Activates rewind mode", PERMISSION_FREEPLAY);

	// If in play mode, load the latest checkpoint / quick checkpoint.
	// If in rewind mode, add a checkpoint or delete the current checkpoint.
	// TODO: different button for delete?
	cvarManager->registerNotifier("cpt_do_checkpoint", [this](std::vector<std::string> command) {
		if (gameWrapper->IsInReplay()) {
			std::unique_ptr<GameState> gs = getReplayGameState();
			if (gs == nullptr) {
				return;
			}
			cvarManager->log("adding checkpoint " + std::to_string(checkpoints.size()+1));
			checkpoints.push_back(*gs);
			saveCheckpointFile();
			return;
		}
		if (!gameWrapper->IsInFreeplay() || gameWrapper->IsPaused()) {
			return;
		}
		if (!rewindMode) {
			loadLatestCheckpoint();
			return;
		}
		hasQuickCheckpoint = false;
		if (rewindState.atCheckpoint) { // Delete the current checkpoint we are at.
			if (!rewindState.deleting) {
				rewindState.deleting = true;
				return;
			}
			rewindState.deleting = false;
			log("at cpt; erasing: " + std::to_string(curCheckpoint+1));
			checkpoints.erase(checkpoints.begin() + curCheckpoint);
			curCheckpoint = std::min(curCheckpoint, checkpoints.size() - 1);
			rewindState.atCheckpoint = false;
			rewindState.justDeletedCheckpoint = true;
			saveCheckpointFile();
			return;
		}
		// Add a new checkpoint here.
		log("adding checkpoint " + std::to_string(checkpoints.size()+1));
		curCheckpoint = checkpoints.size();
		checkpoints.push_back(latest);
		rewindState.atCheckpoint = true;
		rewindState.justDeletedCheckpoint = false;
		saveCheckpointFile();
	}, "Saves/restores/removes a checkpoint", PERMISSION_ALL);

	// Go to previous checkpoint.
	cvarManager->registerNotifier("cpt_prev_checkpoint", [this](std::vector<std::string> command) {
		if (!gameWrapper->IsInFreeplay() || gameWrapper->IsPaused() || checkpoints.size() == 0) {
			return;
		}
		if (ignorePNNotFrozen && !rewindMode) {
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
	}, "Loads the previous checkpoint", PERMISSION_FREEPLAY);

	// Go to next checkpoint.
	cvarManager->registerNotifier("cpt_next_checkpoint", [this](std::vector<std::string> command) {
		if (!gameWrapper->IsInFreeplay() || gameWrapper->IsPaused() || checkpoints.size() == 0) {
			return;
		}
		if (ignorePNNotFrozen && !rewindMode) {
			return;
		}
		curCheckpoint++;
		if (curCheckpoint == checkpoints.size()) {
			curCheckpoint = 0;
		}
		loadCurCheckpoint();
	}, "Loads the next checkpoint", PERMISSION_FREEPLAY);

	cvarManager->registerNotifier("cpt_freeze_ball", [this](std::vector<std::string> command) {
		if (!gameWrapper->IsInFreeplay() || gameWrapper->IsPaused() || history.size() == 0) {
			return;
		}
		if (rewindMode) {
			freezeBall = true;  // takes us out of rewind mode
			return;
		}
		if (freezeBall) {
			freezeBall = false;
			latest.car = history.back().car;
			latest.apply(gameWrapper->GetGameEventAsServer());
			quickCheckpoint = latest;
			hasQuickCheckpoint = true;
			return;
		}
		if (ignorePNNotFrozen) {
			return;
		}
		latest = history.back();
		latest.apply(gameWrapper->GetGameEventAsServer());
		freezeBall = true;
	}, "Loads the next checkpoint", PERMISSION_FREEPLAY);

	cvarManager->registerNotifier("cpt_copy", [this](std::vector<std::string> command) {
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
			cvarManager->log("Copying checkpoint " + std::to_string(curCheckpoint+1));
			output = checkpoints.at(curCheckpoint).toString();
		} else {
			cvarManager->log("No checkpoint to copy!");
			return;
		}
		output = "cpv1" + output + ".";
		OpenClipboard(nullptr);
		EmptyClipboard();
		HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, output.size()+1);
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
		memcpy(lptstrCopy, output.c_str(), output.size()+1);
		GlobalUnlock(hg);
		SetClipboardData(CF_TEXT, hg);
		CloseClipboard();
		GlobalFree(hg);
		cvarManager->log("Data copied to clipboard!");
		log("Written to clipboard: " + output);
	}, "Copies the frozen state / quick checkpoint / last checkpoint to the clipboard", PERMISSION_ALL);

	cvarManager->registerNotifier("cpt_paste", [this](std::vector<std::string> command) {
		if (!gameWrapper->IsInFreeplay() || gameWrapper->IsPaused()) {
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
	}, "Loads a checkpoint from the clipboard as a quick checkpoint", PERMISSION_ALL);

	// Add default bindings.
	registerBindingCVars();

	// Draw the checkpoint or notification about checkpoint deletion.
	gameWrapper->RegisterDrawable(std::bind(&CheckpointPlugin::Render, this, std::placeholders::_1));

	writeSettingsFile();
}

void CheckpointPlugin::registerVarianceCVars() {
	cvarManager->registerCvar("cpt_variance_car_dir", "0", "If set, randomly vary car's direction when resuming", true, true, 0, true, 30, true);
	cvarManager->registerCvar("cpt_variance_car_spd", "0", "If set, randomly vary car's speed when resuming", true, true, 0, true, 50, true);
	cvarManager->registerCvar("cpt_variance_ball_dir", "0", "If set, randomly vary ball's direction when resuming", true, true, 0, true, 30, true);
	cvarManager->registerCvar("cpt_variance_ball_spd", "0", "If set, randomly vary ball's speed when resuming", true, true, 0, true, 50, true);
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

void CheckpointPlugin::loadCurCheckpoint() {
	loadGameState(checkpoints.at(curCheckpoint));
	rewindState.atCheckpoint = true;
}

void CheckpointPlugin::loadGameState(const GameState &state) {
	latest = state;
	ServerWrapper sw = gameWrapper->GetGameEventAsServer();
	sw.PlayerResetTraining(); // In case there is a goal explosion in progress.
	state.apply(sw);
	rewindState.virtualTimeOffset = 0;
	rewindState.holdingFor = 0;
	rewindMode = true;
	freezeBall = false;
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
	if (!gameWrapper->IsInFreeplay()) {
		return;
	}
	ServerWrapper sw = gameWrapper->GetGameEventAsServer();
	if (sw.GetBall().IsNull() || sw.GetGameCar().IsNull()) {
		return;
	}

	if (rewindMode) {
		rewind(sw);
		if (freezeBall) {
			rewindMode = false;
		}
		if (rewindMode) {
			latest.apply(sw);
		} else {
			// Exited rewind mode.
			applyVariance(latest).apply(sw);
		}
	} else {
		record(sw);
	}
}

void CheckpointPlugin::rewind(ServerWrapper sw) {
	ControllerInput ci = sw.GetCars().Get(0).GetInput();

	float currentTime = sw.GetSecondsElapsed();
	float elapsed = std::min(currentTime - lastRewindTime, 0.03f);
	if (elapsed < 0) {
		lastRewindTime = currentTime;
		return;
	}
	if (elapsed < 0.01f) {
		return;
	}
	lastRewindTime = currentTime;
	int buttonsDown = (abs(ci.Throttle) > 0.1 ? 0x01 : 0) |
		(abs(ci.Roll) > 0.1 ? 0x02 : 0) |
		(ci.Handbrake ? 0x04 : 0) |
		(ci.Jump ? 0x08 : 0) |
		(ci.ActivateBoost ? 0x10 : 0) |
		(ci.HoldingBoost ? 0x20 : 0) |
		((rewindState.atCheckpoint || rewindState.justLoadedQuickCheckpoint) && abs(ci.Steer) >= .05 ? 0x40 : 0);
	// See if we should exit rewind mode due to input.
	if (buttonsDown != 0) {
		if (buttonsDown > rewindState.buttonsDown && currentTime - lastRecordTime > 0.1f) {
			log("resuming...");
			rewindMode = false;
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
		}
		rewindState.buttonsDown = buttonsDown;
		return;
	}
	rewindState.buttonsDown = buttonsDown;

	// Determine how much to rewind / advance time.
	if (abs(ci.Steer) < .05f) { // Ignore slight input; keep current game state.
		return;
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
		return;
	}
	latest = history.at(current);
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
	history.emplace_back(sw);
}

void show(CanvasWrapper canvas, Vector2 *loc, std::string s) {
	static const float scale = 1.5f;
	canvas.SetPosition(*loc);
	canvas.DrawString(s, scale, scale);
	loc->Y += 20;
}

void CheckpointPlugin::Render(CanvasWrapper canvas) {
	if (!gameWrapper->IsInFreeplay() || gameWrapper->IsPaused()) {
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
		Vector2 loc = { (int)(screenSize.X * 0.80), (int)(screenSize.Y * 0.08) };
		canvas.SetPosition(loc + Vector2{ 5,5 });
		canvas.SetColor(0, 0, 0, 100);
		canvas.DrawString(std::to_string(curCheckpoint + 1) + " | " + std::to_string(checkpoints.size()), 6, 6);
		canvas.SetPosition(loc);
		canvas.SetColor('\xff', '\xff', '\xff', '\xdc');
		canvas.DrawString(std::to_string(curCheckpoint + 1) + " | " + std::to_string(checkpoints.size()), 6, 6);
	}
}

std::string_view SAVE_FILE_NAME = "freeplaycheckpoint.data";

// Prevent loading an unknown version's save file.
constexpr uint32_t SAVE_FILE_VERSION = 1;

void CheckpointPlugin::loadCheckpointFile() {
	std::ifstream in(gameWrapper->GetDataFolder() / SAVE_FILE_NAME, std::ios::binary);
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
	in.close();
}

void CheckpointPlugin::saveCheckpointFile() {
	std::ofstream out(gameWrapper->GetDataFolder() / SAVE_FILE_NAME, std::ios::binary | std::ios::out | std::ios::trunc);
	auto ver = SAVE_FILE_VERSION;
	writePOD(out, ver);
	auto size = int32_t(checkpoints.size());
	writePOD(out, size);
	for (auto& fav : checkpoints) {
		fav.write(out);
	}
	out.close();
}
