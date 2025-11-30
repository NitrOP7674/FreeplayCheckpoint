/*
 * Copyright (c) 2021
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "pch.h"
#include "CheckpointPlugin.h"
#include "bindings.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_rangeslider.h"

extern KEYBIND_ASSIGNWHICH which_is_being_bound;

void CheckpointPlugin::SetImGuiContext(uintptr_t ctx) {
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

/**
* @brief
* @details Done within the IMGUI section.
*
*/
inline void OpenMenuForKeybinding(std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper) {
    close_opened_menus(cvarManager, gameWrapper);
    cvarManager->executeCommand("openmenu checkpointplugin", false);
    gameWrapper->HookEventWithCaller<ActorWrapper>(
        "Function TAGame.GameViewportClient_TA.HandleKeyPress",
        [=](ActorWrapper aw, void* params, std::string eventName) {
            OnKeyPressed(cvarManager, gameWrapper, aw, params, eventName);
        });

    if (which_is_being_bound == KEYBIND_ASSIGNWHICH::CPT_REWIND_KEY || which_is_being_bound == KEYBIND_ASSIGNWHICH::CPT_FASTFORWARD_KEY) {
        gameWrapper->HookEventWithCaller<ActorWrapper>(
            "Function TAGame.GameViewportClient_TA.HandleAxisPress",
            [=](ActorWrapper aw, void* params, std::string eventName) {
                OnKeyAxisInput(cvarManager, gameWrapper, aw, params, eventName);
            });
    }
}

void CheckpointPlugin::RenderSettings() {
    static const auto openmenufunc = [this]() {
        gameWrapper->Execute([this](GameWrapper* gw) {
            OpenMenuForKeybinding(cvarManager, gameWrapper);
            });
        };

    // main driver for rendering plugin settings
    ImGui::TextUnformatted("Bindings");
    ImGui::TextUnformatted("Instructions: Click the button corresponding to the binding to choose a button to set.");
    ImGui::TextUnformatted("After assigning bindings to each action, click \"Apply All Bindings\" to set them.");

    ImGui::Separator();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, ImGui::GetStyle().ItemSpacing.y));

    ImGui::BeginColumns("bindings columns##bc", 3, ImGuiColumnsFlags_NoBorder | ImGuiColumnsFlags_NoResize);
    ImGui::SetColumnWidth(0, 280.f);
    ImGui::SetColumnWidth(1, 230.f);
    ImGui::SetColumnWidth(2, 200.f);

    // freeze key
    if (ImGui::Button("Freeze (cpt_freeze)##fz", ImVec2(250.f, 0.f))) {
        which_is_being_bound = KEYBIND_ASSIGNWHICH::CPT_FREEZE_KEY;
        openmenufunc();
    }

    ImGui::NextColumn();
    static std::string cfk;
    cfk = cvarManager->getCvar("cpt_freeze_key").getStringValue();
    ImGui::Text("[ %s ]", cfk.c_str());

    ImGui::NextColumn();
    ImGui::NextColumn();

    // do checkpoint
    if (ImGui::Button("Checkpoint (cpt_do_checkpoint)##cp", ImVec2(250.f, 0.f))) {
        which_is_being_bound = KEYBIND_ASSIGNWHICH::CPT_DO_CHECKPOINT_KEY;
        openmenufunc();
    }

    ImGui::NextColumn();

    static std::string cdck;
    cdck = cvarManager->getCvar("cpt_do_checkpoint_key").getStringValue();
    ImGui::Text("[ %s ] ", cdck.c_str());

    ImGui::NextColumn();
    ImGui::NextColumn();

    // prev checkpoint
    if (ImGui::Button("Prev. Checkpoint (cpt_prev_checkpoint)##pc", ImVec2(250.f, 0.f))) {
        which_is_being_bound = KEYBIND_ASSIGNWHICH::CPT_PREV_CHECKPOINT_KEY;
        openmenufunc();
    }

    ImGui::NextColumn();

    static std::string cpck;
    cpck = cvarManager->getCvar("cpt_prev_checkpoint_key").getStringValue();
    ImGui::Text("[ %s ]", cpck.c_str());

    ImGui::NextColumn();

    static bool ignore_while_playing_prev;
    ignore_while_playing_prev = cvarManager->getCvar("cpt_ignore_prev").getBoolValue();
    if (ImGui::Checkbox("Ignore While Playing##prev", &ignore_while_playing_prev)) {
        cvarManager->getCvar("cpt_ignore_prev").setValue(ignore_while_playing_prev);
    }

    ImGui::NextColumn();

    // next checkpoint
    if (ImGui::Button("Next Checkpoint (cpt_next_checkpoint)##nc", ImVec2(250.f, 0.f))) {
        which_is_being_bound = KEYBIND_ASSIGNWHICH::CPT_NEXT_CHECKPOINT_KEY;
        openmenufunc();
    }

    ImGui::NextColumn();

    static std::string cnck;
    cnck = cvarManager->getCvar("cpt_next_checkpoint_key").getStringValue();
    ImGui::Text("[ %s ]", cnck.c_str());

    ImGui::NextColumn();

    static bool ignore_while_playing_next;
    ignore_while_playing_next = cvarManager->getCvar("cpt_ignore_next").getBoolValue();
    if (ImGui::Checkbox("Ignore While Playing##next", &ignore_while_playing_next)) {
        cvarManager->getCvar("cpt_ignore_next").setValue(ignore_while_playing_next);
    }

    ImGui::NextColumn();

    // freeze ball
    if (ImGui::Button("Freeze Ball/Unfreeze Car (cpt_freeze_ball)##fbuc", ImVec2(250.f, 0.f))) {
        which_is_being_bound = KEYBIND_ASSIGNWHICH::CPT_FREEZE_BALL_KEY;
        openmenufunc();
    }

    ImGui::NextColumn();

    static std::string cfbuc;
    cfbuc = cvarManager->getCvar("cpt_freeze_ball_key").getStringValue();
    ImGui::Text("[ %s ]", cfbuc.c_str());

    ImGui::NextColumn();

    static bool ignore_freeze_ball;
    ignore_freeze_ball = cvarManager->getCvar("cpt_ignore_freeze_ball").getBoolValue();
    if (ImGui::Checkbox("Ignore While Playing##fzbl", &ignore_freeze_ball)) {
        cvarManager->getCvar("cpt_ignore_freeze_ball").setValue(ignore_freeze_ball);
    }

    ImGui::NewLine();
    ImGui::NextColumn();

    // mirror state
    if (ImGui::Button("Mirror shot (cpt_mirror_state)##ms", ImVec2(250.f, 0.f))) {
        which_is_being_bound = KEYBIND_ASSIGNWHICH::CPT_MIRROR_STATE_KEY;
        openmenufunc();
    }

    ImGui::NextColumn();

    static std::string cmsk;
    cmsk = cvarManager->getCvar("cpt_mirror_state_key").getStringValue();
    ImGui::Text("[ %s ]", cmsk.c_str());

    ImGui::NextColumn();    ImGui::NextColumn();

    // rewind
    if (ImGui::Button("Rewind (cpt_rewind)##rw", ImVec2(250.f, 0.f))) {
        which_is_being_bound = KEYBIND_ASSIGNWHICH::CPT_REWIND_KEY;
        openmenufunc();
    }

    ImGui::NextColumn();

    static std::string crwk;
    crwk = cvarManager->getCvar("cpt_rewind_key").getStringValue();
    ImGui::Text("[ %s ]", crwk.c_str());

    ImGui::NextColumn();    ImGui::NextColumn();

    // fast forward
    if (ImGui::Button("Fast Forward (cpt_fastforward)##ff", ImVec2(250.f, 0.f))) {
        which_is_being_bound = KEYBIND_ASSIGNWHICH::CPT_FASTFORWARD_KEY;
        openmenufunc();
    }

    ImGui::NextColumn();

    static std::string cffk;
    cffk = cvarManager->getCvar("cpt_fastforward_key").getStringValue();
    ImGui::Text("[ %s ]", cffk.c_str());

    ImGui::EndColumns();
    ImGui::PopStyleVar();

    ImGui::NewLine();

    // apply all bindings
    if (ImGui::Button("Apply All Bindings##aab")) {
        applyBindKeys({});
    }

    ImGui::SameLine(0.f, 50.f);
    ImGui::TextUnformatted("(if bindings have never been set)");

    ImGui::SameLine(0.f, 50.f);
    // remove binding
    ImGui::SetNextItemWidth(20.f);
    if (ImGui::Button("Remove Bindings##rb")) {
        removeBindKeys({});
    }
    ImGui::SameLine(0.f, 10.f);
    ImGui::TextUnformatted("*Removes action on the button from the adjacent key* also :(");

    // reset default bindings
    if (ImGui::Button("Reset Default Bindings##rdb")) {
        resetDefaultBindKeys({});
    }
    ImGui::SameLine(0.f, 50.f);
    ImGui::TextUnformatted("(does not apply them)");

    // disable working while in custom training
    static bool disable_training;
    disable_training = cvarManager->getCvar("cpt_disable_training").getBoolValue();
    if (ImGui::Checkbox("Disable binds in custom training##dbict", &disable_training)) {
        cvarManager->getCvar("cpt_disable_training").setValue(disable_training);
    }

    // disable working while in a workshop map
    static bool disable_workshop;
    disable_workshop = cvarManager->getCvar("cpt_disable_workshop").getBoolValue();
    if (ImGui::Checkbox("Disable binds in workshop##dbiw", &disable_workshop)) {
        cvarManager->getCvar("cpt_disable_workshop").setValue(disable_workshop);
    }

    ImGui::NewLine();

    // reset button
    ImGui::TextUnformatted("Reset Shot button loads last checkpoint instead of resetting if loaded before");
    static int load_after_reset;
    load_after_reset = cvarManager->getCvar("cpt_load_after_reset").getIntValue();
    if (ImGui::SliderInt("(seconds)##rb", &load_after_reset, 0, 30)) {
        cvarManager->getCvar("cpt_load_after_reset").setValue(load_after_reset);
    }

    ImGui::Separator();
    ImGui::NewLine();

    // car and ball variance
    ImGui::TextUnformatted("Variance - applied when leaving rewind mode");
    ImGui::Separator();
    static float variance_car_dir;
    variance_car_dir = cvarManager->getCvar("cpt_variance_car_dir").getFloatValue();
    if (ImGui::SliderFloat("Car Direction (degrees)##cdd", &variance_car_dir, 0.0f, 30.0f, "%.2f")) {
        cvarManager->getCvar("cpt_variance_car_dir").setValue(variance_car_dir);
    }

    static float variance_car_spd;
    variance_car_spd = cvarManager->getCvar("cpt_variance_car_spd").getFloatValue();
    if (ImGui::SliderFloat("Car Speed (percent)##css", &variance_car_spd, 0.0f, 50.0f, "%.2f")) {
        cvarManager->getCvar("cpt_variance_car_spd").setValue(variance_car_spd);
    }

    static float variance_car_rot[2];
    static std::vector<std::string> twoval_holder;
    static std::string var_car_rot;
    var_car_rot = cvarManager->getCvar("cpt_variance_car_rot").getStringValue();
    replace(var_car_rot, "(", "");
    replace(var_car_rot, ")", "");
    split(var_car_rot, twoval_holder, ',');
    switch (twoval_holder.size()) {
    case 0:
        variance_car_rot[0] = 0.0f;
        variance_car_rot[1] = 0.0f;
        break;
    case 1:
        variance_car_rot[0] = variance_car_rot[1] = std::stof(twoval_holder[0]);
        break;
    case 2:
        variance_car_rot[0] = std::stof(twoval_holder[0]);
        variance_car_rot[1] = std::stof(twoval_holder[1]);
        break;
    }
    if (ImGui::RangeSliderFloat("Car Rotation (strength)##crs", &variance_car_rot[0], &variance_car_rot[1], 0.0f, 10.0f, "(%.2f, %.2f)")) {
        cvarManager->getCvar("cpt_variance_car_rot").setValue(std::format("({}, {})", variance_car_rot[0], variance_car_rot[1]));
    }

    static float variance_ball_dir;
    variance_ball_dir = cvarManager->getCvar("cpt_variance_ball_dir").getFloatValue();
    if (ImGui::SliderFloat("Ball Direction (degrees)##bdd", &variance_ball_dir, 0.0f, 30.0f, "%.2f")) {
        cvarManager->getCvar("cpt_variance_ball_dir").setValue(variance_ball_dir);
    }

    static float variance_ball_spd;
    variance_ball_spd = cvarManager->getCvar("cpt_variance_ball_spd").getFloatValue();
    if (ImGui::SliderFloat("Ball Speed (percent)##bsp", &variance_ball_spd, 0.0f, 50.0f, "%.2f")) {
        cvarManager->getCvar("cpt_variance_ball_spd").setValue(variance_ball_spd);
    }

    static float variance_ball_rot[2];
    static std::string var_ball_rot;
    var_ball_rot = cvarManager->getCvar("cpt_variance_ball_rot").getStringValue();
    replace(var_ball_rot, "(", "");
    replace(var_ball_rot, ")", "");
    split(var_ball_rot, twoval_holder, ',');
    switch (twoval_holder.size()) {
    case 0:
        variance_ball_rot[0] = 0.0f;
        variance_ball_rot[1] = 0.0f;
        break;
    case 1:
        variance_ball_rot[0] = variance_ball_rot[1] = std::stof(twoval_holder[0]);
        break;
    case 2:
        variance_ball_rot[0] = std::stof(twoval_holder[0]);
        variance_ball_rot[1] = std::stof(twoval_holder[1]);
        break;
    }
    if (ImGui::RangeSliderFloat("Ball Rotation (strength)##brs", &variance_ball_rot[0], &variance_ball_rot[1], 0.0f, 10.0f, "(%.2f, %.2f)")) {
        cvarManager->getCvar("cpt_variance_ball_rot").setValue(std::format("({}, {})", variance_ball_rot[0], variance_ball_rot[1]));
    }

    static int variance_tot;
    variance_tot = cvarManager->getCvar("cpt_variance_tot").getIntValue();
    if (ImGui::SliderInt("Max. Total Variance##mtv", &variance_tot, 0, 50)) {
        cvarManager->getCvar("cpt_variance_tot").setValue(variance_tot);
    }

    ImGui::NewLine();
    // mirror randomly
    static bool mirror_random;
    mirror_random = cvarManager->getCvar("cpt_mirror_loads").getBoolValue();
    if (ImGui::Checkbox("Randomly mirror when loading checkpoint##mrand", &mirror_random)) {
        cvarManager->getCvar("cpt_mirror_loads").setValue(mirror_random);
    }

    // random chkpt
    static bool checkpoint_random;
    checkpoint_random = cvarManager->getCvar("cpt_randomize_loads").getBoolValue();
    if (ImGui::Checkbox("Load random checkpoint instead of latest##crand", &checkpoint_random)) {
        cvarManager->getCvar("cpt_randomize_loads").setValue(checkpoint_random);
    }
    ImGui::Separator();

    // auto-reset checkpoint
    ImGui::NewLine();
    ImGui::TextUnformatted("Auto-reset checkpoint - reset when the following occurs:");
    ImGui::Separator();
    static bool goal_scored;
    goal_scored = cvarManager->getCvar("cpt_reset_on_goal").getBoolValue();
    if (ImGui::Checkbox("Goal Scored", &goal_scored)) {
        cvarManager->getCvar("cpt_reset_on_goal").setValue(goal_scored);
    }
    static bool reset_on_ball_ground;
    reset_on_ball_ground = cvarManager->getCvar("cpt_reset_on_ball_ground").getBoolValue();
    if (ImGui::Checkbox("Ball touches ground", &reset_on_ball_ground)) {
        cvarManager->getCvar("cpt_reset_on_ball_ground").setValue(reset_on_ball_ground);
    }
    static bool next_instead_of_reset;
    next_instead_of_reset = cvarManager->getCvar("cpt_next_instead_of_reset").getBoolValue();
    if (ImGui::Checkbox("Load next checkpoint instead of resetting", &next_instead_of_reset)) {
        cvarManager->getCvar("cpt_next_instead_of_reset").setValue(next_instead_of_reset);
    }
    ImGui::Separator();
    ImGui::NewLine();

    // else
    ImGui::TextUnformatted("Other options:");
    ImGui::Separator();
    ImGui::TextUnformatted("Save File Name:");
    ImGui::SameLine(0.0f, 50.0f);

    static char fn_str[1048] = "";
    static std::once_flag never_again;
    std::call_once(never_again, [this](char* str) {
        std::string filename = cvarManager->getCvar("cpt_filename").getStringValue();
        std::strncpy(fn_str, filename.c_str(), 1024);
        }, fn_str);
    if (ImGui::InputText("##filenametb", fn_str, 1024)) { // maximum 1024 characters
        cvarManager->getCvar("cpt_filename").setValue(fn_str);
    }
    if (ImGui::SmallButton("Delete ALL Shots (even locked shots; not undo-able!)##das")) {
        deleteAllCheckpoints({});
    }
    ImGui::SameLine(0.0f, 50.0f);

    ImGui::NewLine();
    static bool show_boost;
    show_boost = cvarManager->getCvar("cpt_show_boost").getBoolValue();
    if (ImGui::Checkbox("Show player boost while rewinding##sb", &show_boost)) {
        cvarManager->getCvar("cpt_show_boost").setValue(show_boost);
    }
    static bool clean_hist;
    clean_hist = cvarManager->getCvar("cpt_clean_history").getBoolValue();
    if (ImGui::Checkbox("Clean History -- Erases future history points when resuming##ch", &clean_hist)) {
        cvarManager->getCvar("cpt_clean_history").setValue(clean_hist);
    }
    static int hist_len;
    hist_len = cvarManager->getCvar("cpt_history_length").getIntValue();
    if (ImGui::SliderInt("History Length (seconds)##hlen", &hist_len, 10, 120)) {
        cvarManager->getCvar("cpt_history_length").setValue(hist_len);
    }
    static int hist_refresh;
    hist_refresh = cvarManager->getCvar("cpt_snapshot_interval").getIntValue();
    if (ImGui::SliderInt("History Refresh Rate (ms)##hrr", &hist_refresh, 1, 10)) {
        cvarManager->getCvar("cpt_snapshot_interval").setValue(hist_refresh);
    }
    ImGui::NewLine();
    static bool show_debug;
    show_debug = cvarManager->getCvar("cpt_debug").getBoolValue();
    if (ImGui::Checkbox("Debug -- Show debugging state##dbg", &show_debug)) {
        cvarManager->getCvar("cpt_debug").setValue(show_debug);
    }

    ImGui::NewLine();
    ImGui::NewLine();

    ImGui::TextUnformatted("Freeplay Checkpoint");
    ImGui::TextUnformatted("Bugs/Feature Requests:");
    TextURL("github.com/NitrOP7674", "https://github.com/NitrOP7674", TRUE, TRUE);
    ImGui::TextUnformatted(" -or- on Discord:");
    TextURL("https://discord.gg/SPBxrtfrZw", "https://discord.gg/SPBxrtfrZw", TRUE, FALSE);
    ImGui::TextUnformatted("** Please make sure to read the README first! **");
}

/**
 * @brief do the following on menu open
 */
void CheckpointPlugin::OnOpen() {
    if (which_is_being_bound == KEYBIND_ASSIGNWHICH::NONE) {
        gameWrapper->Execute(
            [this](GameWrapper* gw) { cvarManager->executeCommand("closemenu checkpointplugin", false); });
    }
};

/**
 * @brief do the following on menu close
 */
void CheckpointPlugin::OnClose() {
    gameWrapper->Execute([this](GameWrapper* gw) { reopen_closed_menus(cvarManager); });
};

/**
 * @brief (ImGui) Code called while rendering your menu window
 */
void CheckpointPlugin::Render() {
    if (which_is_being_bound != KEYBIND_ASSIGNWHICH::NONE) {
        // show the key binding window
        // create a translucent background
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(30.f, 30.0f, 30.0f, 0.2f));
        ImGui::Begin(
            "translucent_background",
            NULL,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
            | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs);
        ImGui::PopStyleColor();

        const static float  scale = 1.4f;
        static const char* line1 = "Press any key";
        static const char* line2 = "-- or --";
        static const char* line3 = "Move an Input Axis";
        static const ImVec2 linesz1 = ImGui::CalcTextSize(line1) * scale;
        static const ImVec2 linesz2 = ImGui::CalcTextSize(line2) * scale;
        static const ImVec2 linesz3 = ImGui::CalcTextSize(line3) * scale;

        ImVec2 totalsz;
        switch (which_is_being_bound) {
        case KEYBIND_ASSIGNWHICH::CPT_REWIND_KEY:
        case KEYBIND_ASSIGNWHICH::CPT_FASTFORWARD_KEY:
            totalsz += linesz2 + linesz3;
            [[fallthrough]];
        default:
            totalsz += linesz1;
        }
        ImVec2 PopupWindowSize = totalsz
            + ImGui::GetStyle().WindowPadding * 2
            + ImGui::GetStyle().FramePadding * 2;
        ImGui::SetNextWindowSize(PopupWindowSize);
        ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        static bool is_open = false;
        if (!is_open) {
            ImGui::OpenPopup("Set Keybind##popup");
        }
        if (is_open = ImGui::BeginPopupModal(
            "Set Keybind##popup",
            NULL,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::SetWindowFontScale(scale);

            AlignForWidth(linesz1.x);
            ImGui::TextUnformatted("Press Any Key");
            if (which_is_being_bound == KEYBIND_ASSIGNWHICH::CPT_REWIND_KEY || which_is_being_bound == KEYBIND_ASSIGNWHICH::CPT_FASTFORWARD_KEY) {
                // only show that moving an axis is available for the only two binds it matters for
                AlignForWidth(linesz2.x);
                ImGui::TextUnformatted("-- or --");
                AlignForWidth(linesz3.x);
                ImGui::TextUnformatted("Move an Input Axis");
            }
            ImGui::EndPopup();
        }
        ImGui::End();
    }
};

/**
 * @brief Returns the name of the menu to refer to it by
 *
 * @return The name used refered to by togglemenu
 */
std::string CheckpointPlugin::GetMenuName() {
    return "checkpointplugin";
};

/**
 * @brief Returns a std::string to show as the title
 *
 * @return The title of the menu
 */
std::string CheckpointPlugin::GetMenuTitle() {
    return "CheckpointPlugin";
};

/**
 * @brief Is it the active overlay(window)?
 *
 * @return true if overlay which isn't interacted with (pluginwindow.h)
 */
bool CheckpointPlugin::IsActiveOverlay() {
    return true;
};

/**
 * @brief Should this block input from the rest of the program?
 * (aka RocketLeague and BakkesMod windows)
 *
 * @return True/False for if bakkesmod should block input
 */
bool CheckpointPlugin::ShouldBlockInput() {
    return false;
};