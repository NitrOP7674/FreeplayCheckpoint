/*
 * Copyright (c) 2021
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "pch.h"
#include "CheckpointPlugin.h"
#include "utils/parser.h"

void CheckpointPlugin::removeBindKeys(std::vector<std::string> params) {
	removeBind(cvarManager->getCvar("cpt_freeze_key").getStringValue(), "cpt_freeze");
	removeBind(cvarManager->getCvar("cpt_do_checkpoint_key").getStringValue(), "cpt_do_checkpoint");
	removeBind(cvarManager->getCvar("cpt_prev_checkpoint_key").getStringValue(), "cpt_prev_checkpoint");
	removeBind(cvarManager->getCvar("cpt_next_checkpoint_key").getStringValue(), "cpt_next_checkpoint");
}

void CheckpointPlugin::applyBindKeys(std::vector<std::string> params) {
	addBind(cvarManager->getCvar("cpt_freeze_key").getStringValue(), "cpt_freeze");
	addBind(cvarManager->getCvar("cpt_do_checkpoint_key").getStringValue(), "cpt_do_checkpoint");
	addBind(cvarManager->getCvar("cpt_prev_checkpoint_key").getStringValue(), "cpt_prev_checkpoint");
	addBind(cvarManager->getCvar("cpt_next_checkpoint_key").getStringValue(), "cpt_next_checkpoint");
}

static const std::vector<std::string> KEY_LIST = {
	"XboxTypeS_A", "XboxTypeS_B", "XboxTypeS_X", "XboxTypeS_Y", "XboxTypeS_RightShoulder", "XboxTypeS_RightTrigger",
	"XboxTypeS_RightThumbStick", "XboxTypeS_LeftShoulder", "XboxTypeS_LeftTrigger", "XboxTypeS_LeftThumbStick", "XboxTypeS_Start",
	"XboxTypeS_Back", "XboxTypeS_DPad_Up", "XboxTypeS_DPad_Left", "XboxTypeS_DPad_Right", "XboxTypeS_DPad_Down" };

void CheckpointPlugin::captureBindKey(std::vector<std::string> params) {
	if (params.size() != 2) {
		cvarManager->log("cpt_capture_key: error: requires exactly 1 param.");
		return;
	}
	std::string command = params.back();
	auto cvar = cvarManager->getCvar(command + "_key");
	if (cvar.IsNull()) {
		cvarManager->log("cpt_capture_key: error: unknown cvar to capture to.");
		return;
	}
	auto oldKey = cvar.getStringValue();
	removeBind(oldKey, command);
	for (auto key : KEY_LIST) {
		if (gameWrapper->IsKeyPressed(gameWrapper->GetFNameIndexByString(key))) {
			cvar.setValue(key);
			log("cpt_capture_key: " + command + " = " + key);
			break;
		}
	}
	applyBindKeys(std::vector<std::string>());
}

void CheckpointPlugin::addBind(std::string key, std::string cmd) {
	std::string old = cvarManager->getBindStringForKey(key);
	std::vector<std::string> cmds;

	for (char* token = strtok(const_cast<char*>(old.c_str()), ";");
		token != nullptr;
		token = strtok(nullptr, ";"))
	{
		auto tok = std::string(token);
		trim(tok);
		if (tok != "") {
			cmds.push_back(std::string(tok));
		}
	}
	if (std::find(cmds.begin(), cmds.end(), cmd) == cmds.end()) {
		cmds.push_back(trim(cmd));
	}
	std::stringstream s;
	std::copy(cmds.begin(), cmds.end() - 1, std::ostream_iterator<std::string>(s, ";"));
	s << cmds.back();
	cvarManager->setBind(key, s.str());
}

void CheckpointPlugin::removeBind(std::string key, std::string cmd) {
	std::string old = cvarManager->getBindStringForKey(key);
	std::vector<std::string> cmds;
	log("removing " + cmd + " from " + key);
	for (char* token = strtok(const_cast<char*>(old.c_str()), ";");
		token != nullptr;
		token = strtok(nullptr, ";"))
	{
		auto tok = std::string(token);
		trim(tok);
		if (tok != "" && tok != cmd) {
			log("pushing " + tok);
			cmds.push_back(std::string(tok));
		}
	}
	if (cmds.size() == 0) {
		cvarManager->executeCommand("unbind " + key);
		return;
	}
	std::stringstream s;
	std::copy(cmds.begin(), cmds.end() - 1, std::ostream_iterator<std::string>(s, ";"));
	s << cmds.back();
	log("setting " + key + " to " + s.str());
	cvarManager->setBind(key, s.str());
}
