#include "ui.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
namespace sapfui {
UI::UI(std::string launcher, float scale) : scale_(scale), engine(std::move(launcher)) {
    const char *home = std::getenv("HOME");
    std::snprintf(path_, sizeof(path_), "%s/sapf-experiment.json", home ? home : ".");
    lastSaved_ = json({{"state", doc.state}, {"history", doc.history}, {"current", doc.current}}).dump();
}
void UI::item(const std::string &key) {
    auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    items[key] = {{"x", (a.x + b.x) / 2}, {"y", (a.y + b.y) / 2}, {"left", a.x},
                  {"right", b.x},         {"top", a.y},           {"bottom", b.y}};
}
bool UI::button(const std::string &key, const char *text) {
    bool hit = ImGui::Button(text);
    item(key);
    return hit;
}
void UI::numeric(Node &n) {
    ImGui::PushID(n.id);
    auto &s = doc.state.style;
    if (s.labels) {
        float labelX = ImGui::GetCursorPosX();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(n.label.empty() ? "value" : n.label.c_str());
        ImGui::SameLine(labelX + std::max(s.labelWidth * scale_,
                                          ImGui::CalcTextSize(n.label.empty() ? "value" : n.label.c_str()).x +
                                              ImGui::GetStyle().ItemSpacing.x));
    }
    float v = float(n.value);
    std::string format = "%.4g";
    if (s.units && !n.unit.empty())
        format += " " + n.unit;
    if (s.sliders) {
        ImGui::SetNextItemWidth(s.sliderWidth * scale_);
        if (ImGui::SliderFloat("##slider", &v, float(n.low), float(n.high), format.c_str(),
                               n.logarithmic ? ImGuiSliderFlags_Logarithmic : 0))
            n.value = v;
        item("node/" + std::to_string(n.id) + "/slider");
        if (s.ticks) {
            auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
            auto *dl = ImGui::GetWindowDrawList();
            for (int k = 1; k < 10; ++k) {
                float x = a.x + (b.x - a.x) * k / 10;
                dl->AddLine({x, b.y - 3 * scale_}, {x, b.y}, ImGui::GetColorU32(ImGuiCol_TextDisabled));
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Ctrl+click to type. Right-click for expression actions.");
        nodeMenu(n.id);
    }
    if (s.numbers || !s.sliders) {
        if (s.sliders)
            ImGui::SameLine();
        ImGui::SetNextItemWidth(s.numberWidth * scale_);
        double value = n.value;
        if (ImGui::InputDouble("##number", &value, 0, 0, "%.5g") && std::isfinite(value)) {
            n.value = std::clamp(value, n.low, n.high);
        }
        item("node/" + std::to_string(n.id) + "/number");
        nodeMenu(n.id);
    }
    ImGui::PopID();
}
void UI::nodeMenu(int id) {
    ImGui::PushID(id);
    if (ImGui::BeginPopupContextItem("expression-menu")) {
        if (ImGui::MenuItem("Clone subexpression"))
            copyPending_ = id;
        if (ImGui::MenuItem("Copy displayed text"))
            ImGui::SetClipboardText(doc.state.graph.render(id, doc.state.style.syntax).c_str());
        if (ImGui::MenuItem("Copy runnable SAPF"))
            ImGui::SetClipboardText(doc.state.graph.code(id).c_str());
        if (ImGui::MenuItem("Edit as postfix...")) {
            editNode_ = id;
            std::snprintf(edit_, sizeof(edit_), "%s", doc.state.graph.render(id, 2).c_str());
        }
        if (ImGui::BeginMenu("Replace with")) {
            for (auto &s : specs())
                if (ImGui::MenuItem(s.name.c_str())) {
                    replaceNode_ = id;
                    notice_ = s.name;
                }
            ImGui::EndMenu();
        }
        if (doc.state.graph.nodes.at(id).op == "number") {
            auto &n = doc.state.graph.nodes.at(id);
            ImGui::Separator();
            ImGui::Checkbox("Logarithmic", &n.logarithmic);
            ImGui::SetNextItemWidth(130 * scale_);
            double lo = n.low, hi = n.high;
            if (ImGui::InputDouble("Minimum", &lo, 0, 0, "%.4g") && std::isfinite(lo) && lo < n.high)
                n.low = lo;
            ImGui::SetNextItemWidth(130 * scale_);
            if (ImGui::InputDouble("Maximum", &hi, 0, 0, "%.4g") && std::isfinite(hi) && hi > n.low)
                n.high = hi;
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
}
void UI::expression(int id, int depth) {
    if (depth > 40) {
        ImGui::TextDisabled("Use text view for deeper expressions.");
        return;
    }
    auto &g = doc.state.graph;
    auto &n = g.nodes.at(id);
    ImGui::PushID(id);
    if (n.op == "number") {
        numeric(n);
        ImGui::PopID();
        return;
    }
    ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImGui::SetNextItemOpen(n.open, ImGuiCond_Always);
    auto flags =
        ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
    bool open = ImGui::TreeNodeEx("##node", flags, "%s", n.op.c_str());
    item("node/" + std::to_string(id) + "/open");
    nodeMenu(id);
    n.open = open;
    ImGui::PopStyleColor();
    ImGui::SameLine(std::max(120.f * scale_, ImGui::GetContentRegionAvail().x - 190 * scale_));
    if (ImGui::SmallButton(n.text ? "UI" : "Text"))
        n.text = !n.text;
    item("node/" + std::to_string(id) + "/text");
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy"))
        copyPending_ = id;
    item("node/" + std::to_string(id) + "/copy");
    ImGui::SameLine();
    if (ImGui::SmallButton("Edit")) {
        editNode_ = id;
        std::snprintf(edit_, sizeof(edit_), "%s", g.render(id, 2).c_str());
    }
    item("node/" + std::to_string(id) + "/edit");
    if (open) {
        if (n.text) {
            auto text = g.render(id, doc.state.style.syntax);
            ImGui::PushTextWrapPos();
            ImGui::TextUnformatted(text.c_str());
            ImGui::PopTextWrapPos();
        } else {
            const auto &s = spec(n.op);
            for (size_t i = 0; i < n.args.size(); ++i) {
                int child = n.args[i];
                auto &a = g.nodes.at(child);
                ImGui::PushID(int(i));
                if (a.op != "number") {
                    ImGui::TextDisabled("%s", s.labels[i].c_str());
                    expression(child, depth + 1);
                } else
                    numeric(a);
                ImGui::PopID();
            }
            if (n.implementation) {
                ImGui::SeparatorText("implementation");
                expression(n.implementation, depth + 1);
            } else if (ImGui::TreeNode("Native implementation")) {
                ImGui::PushTextWrapPos();
                ImGui::TextUnformatted(s.help.c_str());
                ImGui::TextDisabled("Evaluated by the SAPF engine.");
                ImGui::PopTextWrapPos();
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}
void UI::historyJump(int target) {
    if (doc.dirty())
        doc.commit("Retained draft before navigation");
    doc.checkout(target);
}
void UI::sidebar() {
    ImGui::TextUnformatted("Top Level Directory");
    ImGui::Separator();
    const char *pages[] = {"sapf-ui / prototypes", "SAPF2 / expressions", "Silly & Wobbly Waves",
                           "ImGui / widget experiments"};
    for (int i = 0; i < 4; ++i) {
        if (ImGui::Selectable(pages[i], doc.state.page == i))
            doc.state.page = i;
        item("page/" + std::to_string(i));
    }
    ImGui::Spacing();
    ImGui::SeparatorText("History");
    if (button("commit", "Commit")) {
        doc.commit(commit_);
        commit_[0] = 0;
        notice_ = "Committed state and layout.";
    }
    ImGui::SameLine();
    if (button("undo", "Undo"))
        doc.undo();
    ImGui::SameLine();
    if (button("redo", "Redo"))
        doc.redo();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##commit", "optional commit label", commit_, sizeof(commit_));
    item("commit-label");
    if (button("first", "First"))
        historyJump(0);
    ImGui::SameLine();
    if (button("prev", "Prev"))
        historyJump(std::max(0, doc.current - 1));
    ImGui::SameLine();
    if (button("next", "Next"))
        historyJump(std::min(int(doc.history.size()) - 1, doc.current + 1));
    ImGui::SameLine();
    if (button("last", "Last"))
        historyJump(int(doc.history.size()) - 1);
    ImGui::TextDisabled("%zu entries%s", doc.history.size(), doc.dirty() ? "  * uncommitted" : "");
    ImGui::BeginChild("history-list", {0, 125 * scale_}, ImGuiChildFlags_Borders);
    for (int i = 0; i < int(doc.history.size()); ++i) {
        auto &h = doc.history[i];
        std::string text = std::to_string(i) + "  " +
                           (h.parent >= 0 ? "<- " + std::to_string(h.parent) + "  " : "") + h.label;
        if (ImGui::Selectable((text + "##" + std::to_string(i)).c_str(), i == doc.current))
            historyJump(i);
        item("history/" + std::to_string(i));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", h.time.c_str());
    }
    ImGui::EndChild();
    ImGui::SeparatorText("UI Style");
    auto &s = doc.state.style;
    const char *themes[] = {"Light", "Dark", "Blue", "Red"};
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##theme", &s.theme, themes, 4);
    item("theme");
    ImGui::Checkbox("Sliders", &s.sliders);
    ImGui::SameLine();
    ImGui::Checkbox("Numbers", &s.numbers);
    ImGui::Checkbox("Labels", &s.labels);
    ImGui::SameLine();
    ImGui::Checkbox("Units", &s.units);
    ImGui::Checkbox("Ticks", &s.ticks);
    ImGui::SameLine();
    ImGui::Checkbox("Boxes", &s.boxes);
    if (ImGui::TreeNode("Dimensions and spacing")) {
        auto control = [&](const char *name, float &v, float a, float b) {
            ImGui::SetNextItemWidth(125 * scale_);
            ImGui::SliderFloat(name, &v, a, b, "%.0f px");
        };
        control("Label width", s.labelWidth, 30, 180);
        control("Slider width", s.sliderWidth, 60, 400);
        control("Number width", s.numberWidth, 40, 200);
        control("Cell radius", s.rounding, 0, 16);
        control("Spacing", s.spacing, 0, 20);
        control("Padding", s.padding, 0, 20);
        control("Wave line", s.waveWidth, 1, 8);
        ImGui::TreePop();
    }
    ImGui::SeparatorText("Syntax");
    for (int i = 0; i < 5; ++i) {
        if (ImGui::RadioButton(syntaxName(i), s.syntax == i))
            s.syntax = i;
        item("syntax/" + std::to_string(i));
        if (i == 0 || i == 2)
            ImGui::SameLine();
    }
    ImGui::Spacing();
    if (button("console", "SAPF console"))
        showConsole_ = !showConsole_;
    ImGui::Spacing();
    ImGui::PushTextWrapPos();
    ImGui::TextDisabled("Rebuilt from McCartney's 2021 demos, 52:48-56:49. Right-click any expression for "
                        "clone, replace and text editing.");
    ImGui::PopTextWrapPos();
}
void UI::browser() {
    ImGui::SetNextItemWidth(220 * scale_);
    ImGui::InputTextWithHint("##search", "Find function...", search_, sizeof(search_));
    item("search");
    ImGui::SameLine();
    ImGui::TextDisabled("Click a function to add a prototype");
    if (ImGui::BeginTabBar("functions")) {
        for (const char *cat : {"User defined functions", "Unit generators", "Library",
                                "All system functions", "Random generators"}) {
            bool visible = ImGui::BeginTabItem(cat);
            item(std::string("tab/") + cat);
            if (!visible)
                continue;
            bool first = true;
            for (auto &s : specs()) {
                if (s.category != cat ||
                    (!std::string(search_).empty() && s.name.find(search_) == std::string::npos))
                    continue;
                if (!first)
                    ImGui::SameLine();
                first = false;
                if (button("add/" + s.name, s.name.c_str()) && doc.state.graph.roots.size() < 64) {
                    int id = doc.state.graph.create(s.name);
                    doc.state.graph.roots.push_back(id);
                    doc.state.selected = id;
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", s.help.c_str());
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
void UI::workspace() {
    auto &s = doc.state;
    auto &g = s.graph;
    if (s.page == 1)
        browser();
    else {
        ImGui::TextUnformatted("Code as prototypes");
        ImGui::SameLine();
        if (button("add-bubbles", "+ bubbles") && g.roots.size() < 64) {
            int id = g.create("bubbles");
            g.roots.push_back(id);
            s.selected = id;
        }
        ImGui::TextDisabled("Clone any nested expression; each copy is independent. Play the selected form.");
    }
    ImGui::Separator();
    ImGui::BeginChild("expressions", {0, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    // Multiple prototype cards sit side by side, as in the first demo.
    int columns = s.page == 0 ? std::max(1, std::min(3, int(g.roots.size()))) : 1;
    float cardWidth =
        std::max(600.f, s.style.labelWidth + s.style.sliderWidth + s.style.numberWidth + 180.f) * scale_;
    float tableWidth = s.page == 0 ? std::max(ImGui::GetContentRegionAvail().x, columns * cardWidth) : 0.f;
    if (ImGui::BeginTable("cards", columns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV,
                          {tableWidth, 0})) {
        auto roots = g.roots;
        for (int id : roots) {
            ImGui::TableNextColumn();
            ImGui::PushID(id);
            ImGui::BeginGroup();
            if (ImGui::RadioButton("", s.selected == id))
                s.selected = id;
            item("root/" + std::to_string(id) + "/select");
            ImGui::SameLine();
            ImGui::Text("%s  #%d", g.nodes.at(id).op.c_str(), id);
            ImGui::SameLine();
            if (button("root/" + std::to_string(id) + "/play", "Play")) {
                s.selected = id;
                engine.play(g, id);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Clone"))
                copyPending_ = id;
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove") && g.roots.size() > 1)
                deletePending_ = id;
            if (s.page == 0 && g.nodes.at(id).implementation)
                expression(g.nodes.at(id).implementation);
            else
                expression(id);
            ImGui::Spacing();
            ImGui::SeparatorText(syntaxName(s.style.syntax));
            auto text = g.render(id, s.style.syntax);
            ImGui::PushTextWrapPos();
            ImGui::TextUnformatted(text.c_str());
            ImGui::PopTextWrapPos();
            if (ImGui::SmallButton("Copy text"))
                ImGui::SetClipboardText(text.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Copy SAPF"))
                ImGui::SetClipboardText(g.code(id).c_str());
            ImGui::Spacing();
            ImGui::EndGroup();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}
bool UI::knob(const char *label, float &v, float lo, float hi) {
    ImGui::PushID(label);
    ImGui::BeginGroup();
    float size = 52 * scale_;
    auto p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("knob", {size, size});
    item("knob/" + std::string(label));
    bool changed = false;
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        float before = v;
        v = std::clamp(v - (ImGui::GetIO().MouseDelta.y - ImGui::GetIO().MouseDelta.x) * (hi - lo) / 180.f,
                       lo, hi);
        changed = v != before;
    }
    auto *dl = ImGui::GetWindowDrawList();
    ImVec2 c{p.x + size / 2, p.y + size / 2};
    float r = size * .44f;
    dl->AddCircleFilled(c, r, ImGui::GetColorU32(ImGuiCol_FrameBg));
    dl->AddCircle(c, r, ImGui::GetColorU32(ImGuiCol_Border), 40, 1.5f * scale_);
    float angle = 2.35619449f + std::clamp((v - lo) / (hi - lo), 0.f, 1.f) * 4.71238898f;
    dl->AddLine(c, {c.x + std::cos(angle) * r * .8f, c.y + std::sin(angle) * r * .8f},
                ImGui::GetColorU32(ImGuiCol_SliderGrabActive), 2 * scale_);
    ImGui::SetNextItemWidth(size + 8 * scale_);
    float edit = v;
    if (ImGui::DragFloat("##value", &edit, (hi - lo) / 1000, lo, hi, "%.3g", ImGuiSliderFlags_AlwaysClamp)) {
        v = edit;
        changed = true;
    }
    ImGui::TextUnformatted(label);
    ImGui::EndGroup();
    ImGui::PopID();
    return changed;
}
void UI::widgets() {
    auto &v = doc.state.widgets;
    ImGui::TextUnformatted("SoundAsPureForm2 / widget experiments");
    ImGui::TextDisabled(
        "Reconstruction of the custom panel at 56:30-56:45. Controls share values across views.");
    if (ImGui::CollapsingHeader("slider tests", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char *names[] = {"freq", "duty", "phase", "amp"};
        const char *fmt[] = {"%.2f Hz", "%.3f", "%.3f cyc", "%.4f"};
        for (int i = 0; i < 4; ++i) {
            ImGui::SetNextItemWidth(280 * scale_);
            ImGui::SliderFloat(names[i], &v[i], i == 0 ? 20 : 0, i == 0 ? 8000 : 1, fmt[i],
                               i == 0 ? ImGuiSliderFlags_Logarithmic : 0);
            item("widget/" + std::to_string(i));
        }
    }
    if (ImGui::CollapsingHeader("range sliders", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char *names[] = {"freq range", "duty range", "phase range", "amp range"};
        for (int i = 0; i < 4; ++i) {
            ImGui::SetNextItemWidth(280 * scale_);
            ImGui::DragFloatRange2(names[i], &v[4 + 2 * i], &v[5 + 2 * i], i == 0 ? 5 : .005f,
                                   i == 0 ? 20 : 0, i == 0 ? 8000 : 1, "%.3f", "%.3f",
                                   ImGuiSliderFlags_AlwaysClamp);
            item("range/" + std::to_string(i));
        }
    }
    if (ImGui::CollapsingHeader("knobs", ImGuiTreeNodeFlags_DefaultOpen)) {
        knob("Freq", v[12], 20, 2000);
        ImGui::SameLine();
        knob("Duty", v[13], 0, 1);
        ImGui::SameLine();
        knob("Phase", v[14], 0, 1);
        const char *names[] = {"Atk", "Dcy", "Sus", "Rls"};
        for (int i = 0; i < 4; ++i) {
            if (i)
                ImGui::SameLine();
            knob(names[i], v[15 + i], i == 2 ? 0 : .001f, i == 2 ? 1 : 2);
        }
    }
    if (ImGui::CollapsingHeader("vertical sliders"))
        for (int i = 0; i < 4; ++i) {
            if (i)
                ImGui::SameLine();
            ImGui::PushID(i);
            ImGui::VSliderFloat("##v", {45 * scale_, 110 * scale_}, &v[i], i == 0 ? 20 : 0, i == 0 ? 8000 : 1,
                                "%.2f");
            ImGui::PopID();
        }
    if (ImGui::CollapsingHeader("envelope", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto p = ImGui::GetCursorScreenPos();
        ImVec2 sz{400 * scale_, 100 * scale_};
        ImGui::InvisibleButton("envelope", sz);
        auto *dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p, {p.x + sz.x, p.y + sz.y}, ImGui::GetColorU32(ImGuiCol_FrameBg));
        float total = v[15] + v[16] + .4f + v[18];
        ImVec2 points[5] = {{p.x, p.y + sz.y},
                            {p.x + sz.x * v[15] / total, p.y + 4},
                            {p.x + sz.x * (v[15] + v[16]) / total, p.y + (1 - v[17]) * (sz.y - 4)},
                            {p.x + sz.x * (v[15] + v[16] + .4f) / total, p.y + (1 - v[17]) * (sz.y - 4)},
                            {p.x + sz.x, p.y + sz.y}};
        dl->AddPolyline(points, 5, IM_COL32(231, 204, 89, 255), 0, 2 * scale_);
    }
    ImGui::Checkbox("Show stock Dear ImGui Demo (separate)", &showDemo_);
    ImGui::TextDisabled(
        "The video demonstrates these widgets; it does not establish an instrument behind them.");
}
void UI::waves() {
    ImGui::TextUnformatted("Silly Waves / Wobbly Waves");
    auto &v = doc.state.wave;
    ImGui::SetNextItemWidth(260 * scale_);
    ImGui::SliderFloat("cycles", &v[0], .25, 16);
    ImGui::SetNextItemWidth(260 * scale_);
    ImGui::SliderFloat("wobble", &v[1], 0, 3);
    ImGui::SetNextItemWidth(260 * scale_);
    ImGui::SliderFloat("amplitude", &v[2], 0, 1);
    ImGui::SetNextItemWidth(260 * scale_);
    ImGui::SliderFloat("phase", &v[3], 0, 1);
    ImGui::TextDisabled("Wave graphic, as shown at 55:40; this is not an audio oscilloscope.");
    auto p = ImGui::GetCursorScreenPos();
    ImVec2 size{std::max(100.f, ImGui::GetContentRegionAvail().x - 10), 260 * scale_};
    ImGui::InvisibleButton("wave", size);
    auto *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, {p.x + size.x, p.y + size.y}, ImGui::GetColorU32(ImGuiCol_FrameBg));
    for (int j = 1; j < 8; ++j)
        dl->AddLine({p.x, p.y + size.y * j / 8}, {p.x + size.x, p.y + size.y * j / 8},
                    IM_COL32(120, 120, 120, 35));
    std::vector<ImVec2> points;
    for (int i = 0; i < 600; ++i) {
        float x = i / 599.f;
        float y = std::sin(6.2831853f * (v[0] * x + v[3]) + v[1] * std::sin(6.2831853f * x * 3)) * v[2];
        points.push_back({p.x + x * size.x, p.y + size.y * (.5f - .45f * y)});
    }
    dl->AddPolyline(points.data(), int(points.size()), IM_COL32(237, 210, 75, 255), 0,
                    doc.state.style.waveWidth * scale_);
}
void UI::console() {
    if (!showConsole_)
        return;
    ImGui::SetNextWindowSize({820 * scale_, 420 * scale_}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("SAPF console", &showConsole_)) {
        ImGui::TextDisabled(
            "Full SAPF interpreter. Ctrl+Enter evaluates. Esc stops; Reset interrupts busy code.");
        ImGui::InputTextMultiline("##source", console_, sizeof(console_), {-1, 100 * scale_});
        item("console-source");
        if (button("evaluate", "Evaluate") ||
            (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::GetIO().KeyCtrl &&
             ImGui::IsKeyPressed(ImGuiKey_Enter))) {
            engine.send(console_);
        }
        ImGui::SameLine();
        if (button("reset-engine", "Reset engine"))
            engine.reset();
        ImGui::SameLine();
        if (ImGui::Button("Clear output"))
            engine.log.clear();
        ImGui::BeginChild("output", {0, 0}, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
        bool bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 5;
        ImGui::TextUnformatted(engine.log.c_str());
        if (bottom)
            ImGui::SetScrollHereY(1);
        ImGui::EndChild();
    }
    ImGui::End();
}
void UI::fileDialog() {
    if (!fileMode_.empty() && !ImGui::IsPopupOpen("Document"))
        ImGui::OpenPopup("Document");
    if (ImGui::BeginPopupModal("Document", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s complete document and branching history", fileMode_.c_str());
        ImGui::SetNextItemWidth(580 * scale_);
        ImGui::InputText("File", path_, sizeof(path_));
        item("file-path");
        bool save = fileMode_ == "Save";
        if (!save)
            ImGui::TextDisabled(
                "Unsaved work stays in this document unless opening succeeds. Save first to retain it.");
        if (button("file-confirm", save ? "Save" : "Open")) {
            try {
                if (save) {
                    doc.save(path_);
                } else {
                    auto loaded = Document::load(path_);
                    engine.stop();
                    doc = std::move(loaded);
                }
                filename = path_;
                lastSaved_ =
                    json({{"state", doc.state}, {"history", doc.history}, {"current", doc.current}}).dump();
                notice_ = save ? "Saved document and all history branches." : "Opened document.";
                fileMode_.clear();
                ImGui::CloseCurrentPopup();
            } catch (const std::exception &e) {
                notice_ = e.what();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            fileMode_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::TextWrapped("%s", notice_.c_str());
        ImGui::EndPopup();
    }
}
void UI::loadDocument(const std::string &file) {
    auto loaded = Document::load(file);
    engine.stop();
    doc = std::move(loaded);
    filename = file;
    std::snprintf(path_, sizeof(path_), "%s", file.c_str());
    lastSaved_ = json({{"state", doc.state}, {"history", doc.history}, {"current", doc.current}}).dump();
}
void UI::requestClose() {
    if (json({{"state", doc.state}, {"history", doc.history}, {"current", doc.current}}).dump() != lastSaved_)
        confirmClose_ = true;
    else
        closeRequested_ = true;
}
void UI::draw() {
    items = json::object();
    auto &t = doc.state.style;
    if (t.theme == 0)
        ImGui::StyleColorsLight();
    else if (t.theme == 2)
        ImGui::StyleColorsClassic();
    else
        ImGui::StyleColorsDark();
    auto &style = ImGui::GetStyle();
    style.WindowRounding = 0;
    style.FrameRounding = t.rounding * scale_;
    style.ChildRounding = t.rounding * scale_;
    style.FramePadding = {t.padding * scale_, 3 * scale_};
    style.ItemSpacing = {t.spacing * scale_, t.spacing * scale_};
    style.IndentSpacing = 14 * scale_;
    style.WindowPadding = {8 * scale_, 8 * scale_};
    style.ChildBorderSize = t.boxes ? 1 : 0;
    if (t.theme == 3) {
        style.Colors[ImGuiCol_Header] = {.43f, .13f, .13f, 1};
        style.Colors[ImGuiCol_Button] = {.51f, .19f, .17f, 1};
        style.Colors[ImGuiCol_SliderGrab] = {.95f, .38f, .30f, 1};
        style.Colors[ImGuiCol_SliderGrabActive] = {1, .55f, .45f, 1};
    }
    auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("SAPF Experiments", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    if (button("play", "Play selected"))
        engine.play(doc.state.graph, doc.state.selected);
    ImGui::SameLine();
    if (button("stop", "Stop / Esc"))
        engine.stop();
    ImGui::SameLine();
    if (button("save", "Save...")) {
        fileMode_ = "Save";
        notice_.clear();
    }
    ImGui::SameLine();
    if (button("open", "Open...")) {
        fileMode_ = "Open";
        notice_.clear();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s  |  %s",
                        engine.playing ? "Playing"
                        : engine.root  ? "Starting..."
                                       : "Stopped",
                        filename.empty() ? "Untitled" : std::filesystem::path(filename).filename().c_str());
    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        engine.stop();
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
        fileMode_ = "Save";
    if (!ImGui::GetIO().WantTextInput && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z))
        doc.undo();
    if (!ImGui::GetIO().WantTextInput && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y))
        doc.redo();
    ImGui::Separator();
    ImGui::BeginChild("sidebar", {285 * scale_, -26 * scale_}, ImGuiChildFlags_Borders);
    sidebar();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("main", {0, -26 * scale_}, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    if (doc.state.page < 2)
        workspace();
    else if (doc.state.page == 2)
        waves();
    else
        widgets();
    ImGui::EndChild();
    ImGui::TextUnformatted(engine.error.empty() ? notice_.c_str() : engine.error.c_str());
    ImGui::End();
    auto &g = doc.state.graph;
    if (copyPending_ && g.roots.size() >= 64) {
        notice_ = "A document supports up to 64 prototype cards. Save and start another document.";
        copyPending_ = 0;
    }
    if (copyPending_) {
        int copy = g.clone(copyPending_);
        g.roots.push_back(copy);
        doc.state.selected = copy;
        notice_ = "Cloned subexpression as an independent prototype.";
        copyPending_ = 0;
    }
    if (deletePending_) {
        g.roots.erase(std::remove(g.roots.begin(), g.roots.end(), deletePending_), g.roots.end());
        if (doc.state.selected == deletePending_)
            doc.state.selected = g.roots[0];
        if (engine.root == deletePending_)
            engine.stop();
        deletePending_ = 0;
    }
    if (replaceNode_) {
        int created = g.create(notice_);
        Node n = g.nodes.at(created);
        n.id = replaceNode_;
        g.nodes.at(replaceNode_) = n;
        g.nodes.erase(created);
        notice_ = "Replaced expression.";
        replaceNode_ = 0;
    }
    if (editNode_ && !ImGui::IsPopupOpen("Edit postfix"))
        ImGui::OpenPopup("Edit postfix");
    if (ImGui::BeginPopupModal("Edit postfix", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Edit structural expression in postfix notation");
        ImGui::TextDisabled(
            "Numbers, two-element lists and the functions in the browser. Full SAPF: use Console.");
        ImGui::InputTextMultiline("##postfix", edit_, sizeof(edit_), {700 * scale_, 160 * scale_});
        item("postfix-input");
        if (button("postfix-apply", "Apply")) {
            try {
                Graph next = g;
                int parsed = next.parsePostfix(edit_);
                Node n = next.nodes.at(parsed);
                n.id = editNode_;
                next.nodes.at(editNode_) = n;
                next.nodes.erase(parsed);
                next.validate();
                g = std::move(next);
                editNode_ = 0;
                notice_ = "Expression updated.";
                ImGui::CloseCurrentPopup();
            } catch (const std::exception &e) {
                notice_ = e.what();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            editNode_ = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::TextWrapped("%s", notice_.c_str());
        ImGui::EndPopup();
    }
    fileDialog();
    console();
    if (showDemo_)
        ImGui::ShowDemoWindow(&showDemo_);
    if (confirmClose_ && !ImGui::IsPopupOpen("Close document?"))
        ImGui::OpenPopup("Close document?");
    if (ImGui::BeginPopupModal("Close document?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Save your expression and history before closing?");
        if (ImGui::Button("Save...")) {
            confirmClose_ = false;
            fileMode_ = "Save";
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard and close")) {
            closeRequested_ = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Keep working")) {
            confirmClose_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    engine.sync(g);
    engine.poll();
}
void UI::writeProbe() {
    if (probe.empty())
        return;
    json h = json::array();
    for (auto &r : doc.history)
        h.push_back({{"parent", r.parent}, {"label", r.label}});
    json data = {{"testSequence", testSequence},
                 {"items", items},
                 {"state", doc.state},
                 {"current", doc.current},
                 {"history", h},
                 {"dirty", doc.dirty()},
                 {"playing", engine.playing},
                 {"engineRoot", engine.root},
                 {"log", engine.log},
                 {"error", engine.error},
                 {"notice", notice_}};
    auto tmp = probe + ".tmp";
    std::ofstream(tmp) << data.dump();
    std::filesystem::rename(tmp, probe);
}
// Test-only input uses the same ImGui event path as SDL. No document mutations.
// Enabled only by an explicit --test-input path; ordinary launches never read it.
void UI::driveTestInput() {
    if (testInput.empty())
        return;
    static json action;
    static int phase = 0;
    static ImVec2 start{}, end{};
    auto &io = ImGui::GetIO();
    if (phase == 0) {
        std::ifstream f(testInput);
        if (!f)
            return;
        try {
            f >> action;
        } catch (...) {
            return;
        }
        if (action.value("seq", 0) <= testSequence)
            return;
        std::string target = action.value("target", "");
        if (!target.empty()) {
            if (!items.contains(target))
                return;
            auto r = items.at(target);
            start = {r.at("x").get<float>(), r.at("y").get<float>()};
            if (action.value("edge", false))
                start.x = r.at("left").get<float>() + 8 * scale_;
            end = start;
            if (action.contains("fraction"))
                end.x =
                    r.at("left").get<float>() + action.at("fraction").get<float>() *
                                                    (r.at("right").get<float>() - r.at("left").get<float>());
            end.y += action.value("dy", 0.f);
        }
        phase = 1;
    }
    io.AddFocusEvent(true);
    std::string kind = action.value("kind", "click");
    if (kind == "click" || kind == "drag") {
        io.AddMousePosEvent(phase >= 3 ? end.x : start.x, phase >= 3 ? end.y : start.y);
        if (phase == 2)
            io.AddMouseButtonEvent(action.value("button", 0), true);
        if (phase == 5)
            io.AddMouseButtonEvent(action.value("button", 0), false);
    } else if (kind == "type") {
        if (phase == 1) {
            io.AddKeyEvent(ImGuiMod_Ctrl, true);
            io.AddKeyEvent(ImGuiKey_A, true);
        }
        if (phase == 2) {
            io.AddKeyEvent(ImGuiKey_A, false);
            io.AddKeyEvent(ImGuiMod_Ctrl, false);
        }
        if (phase == 3)
            io.AddInputCharactersUTF8(action.value("text", "").c_str());
        if (phase == 4 && action.value("enter", true))
            io.AddKeyEvent(ImGuiKey_Enter, true);
        if (phase == 5)
            io.AddKeyEvent(ImGuiKey_Enter, false);
    } else if (kind == "key") {
        ImGuiKey key = action.value("key", "") == "Escape" ? ImGuiKey_Escape : ImGuiKey_Enter;
        if (phase == 2)
            io.AddKeyEvent(key, true);
        if (phase == 4)
            io.AddKeyEvent(key, false);
    }
    if (++phase > 7) {
        testSequence = action.at("seq").get<int>();
        phase = 0;
    }
}
} // namespace sapfui
