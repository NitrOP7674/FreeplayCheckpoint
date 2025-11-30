#include "pch.h"

extern std::shared_ptr<CVarManagerWrapper> _globalCvarManager;
template<typename S, typename... Args>
void LOG(const S& format_str, Args&&... args) {
    _globalCvarManager->log(std::format(format_str, args...));
}

static std::vector<std::string> menu_names;
void close_opened_menus(std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gw) {
    cvarManager->executeCommand("debugui", true);

    CVarWrapper bmlf = cvarManager->getCvar("bakkesmod_log_instantflush");  // I NEED *this* to be a thing.
    int         temp = 0;
    if (bmlf) {
        temp = bmlf.getIntValue();
        bmlf.setValue(1);
    }
    cvarManager->log("FLUSH?");
    if (bmlf) {
        bmlf.setValue(temp);
    }

    auto logfile = std::ifstream(gw->GetBakkesModPath() / "bakkesmod.log", std::ios::ate);
    logfile.seekg(-1, std::ios::end);  // otherwise we're at the -1 end
    if (logfile.peek() == '\n' || logfile.peek() == '\r') {
        // skip the last possible newline
        logfile.seekg(-1, std::ios::cur);
    }
    const auto get_prev_line = [&logfile]() -> auto {
        std::string line;
        if (logfile.peek() == '\n' || logfile.peek() == '\r') {
            // because "\r\n" might exist.
            logfile.seekg(-1, std::ios::cur);
        }

        while (logfile.good() && logfile.peek() != '\r' && logfile.peek() != '\n') {
            line += logfile.peek();
            logfile.seekg(-1, std::ios::cur);
        }

        if (logfile.peek() == '\n' || logfile.peek() == '\r') {
            // because "\r\n" might exist.
            logfile.seekg(-1, std::ios::cur);
        }

        return line | std::views::reverse | std::ranges::to<std::string>();
        };

     // Limit 30 for "runaway" protection... JUST IN CASE THERE'S A BUG!
    for (int i = 0; i < 30 && logfile.good(); i++) {
        std::string line = get_prev_line();
        std::transform(begin(line), end(line), begin(line), [](unsigned char c) { return std::toupper(c); });

        if (line.contains("CURRENTLY ACTIVE WINDOWS")) {
            int num = std::stoi(line.substr(line.rfind(" ") + 1));

            std::string nl;
            // read the current line since getter is set back due to get_prev_line()
            std::getline(logfile, nl);
            // read the current line since getter is set back due to get_prev_line()
            std::getline(logfile, nl);

            for (int i = 0; i < num; ++i) {
                std::getline(logfile, nl);
                menu_names.push_back(nl.substr(nl.rfind(" ") + 1));
            }

            break;
        }
    }

    for (auto& mn : menu_names) {
        cvarManager->executeCommand(std::format("closemenu {}", mn), false);
    }
}

void reopen_closed_menus(std::shared_ptr<CVarManagerWrapper>  cvarManager) {
    for (auto& mn : menu_names) {
        cvarManager->executeCommand(std::format("openmenu {}", mn), false);
    }

    menu_names.clear();
}

/**
 * @brief This is for helping with IMGUI stuff
 *
 *  copied from: https://github.com/ocornut/imgui/discussions/3862
 *
 * @param width total width of items
 * @param alignment where on the line to align. Default = 0.5f
 */
void AlignForWidth(float width, float alignment) {
    float avail = ImGui::GetContentRegionAvail().x;
    float off = (avail - width) * alignment;
    if (off > 0.0f) { ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off); }
}

/**
 * @brief https://mastodon.gamedev.place/@dougbinks/99009293355650878
 *
 * @param col_ The color the underline should be.
 */
void AddUnderline(ImColor col_) {
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    min.y = max.y;
    ImGui::GetWindowDrawList()->AddLine(min, max, col_, 1.0f);
}

/**
 * @brief taken from https://gist.github.com/dougbinks/ef0962ef6ebe2cadae76c4e9f0586c69
 * "hyperlink urls"
 *
 * @param text_ The shown text.
 * @param URL_ The url accessed after clicking the shown text.
 * @param SameLineBefore_ Should use on the same line before?
 * @param SameLineAfter_ Should use on the same line after?
 */
void TextURL(const char* text_, const char* URL_, uint8_t SameLineBefore_, uint8_t SameLineAfter_) {
    if (1 == SameLineBefore_) {
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 165, 255, 255));
    ImGui::Text("%s", text_);
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        if (ImGui::IsMouseClicked(0)) {
            // What if the URL length is greater than int but less than size_t?
            // well then the program should crash, but this is fine.
            const int nchar
                = std::clamp(static_cast<int>(std::strlen(URL_)), 0, (std::numeric_limits<int>::max)() - 1);
            wchar_t* URL = new wchar_t[nchar + 1];
            wmemset(URL, 0, nchar + 1);
            MultiByteToWideChar(CP_UTF8, 0, URL_, nchar, URL, nchar);
            ShellExecute(NULL, L"open", URL, NULL, NULL, SW_SHOWNORMAL);

            delete[] URL;
        }
        AddUnderline(ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
        ImGui::SetTooltip("  Open in browser\n%s", URL_);
    } else {
        AddUnderline(ImGui::GetStyle().Colors[ImGuiCol_Button]);
    }
    if (1 == SameLineAfter_) {
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    }
}
