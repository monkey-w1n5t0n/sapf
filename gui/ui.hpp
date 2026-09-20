#pragma once
#include "backend.hpp"
#include "imgui.h"
namespace sapfui {
class UI {
    void item(const std::string &name);
    bool button(const std::string &key, const char *text);
    void expression(int id, int depth = 0);
    void numeric(Node &n);
    void sidebar();
    void browser();
    void workspace();
    void widgets();
    void waves();
    void console();
    void historyJump(int target);
    void fileDialog();
    void nodeMenu(int id);
    bool knob(const char *label, float &value, float low, float high);
    int copyPending_ = 0, deletePending_ = 0, editNode_ = 0, replaceNode_ = 0;
    char search_[128]{}, path_[1024]{}, commit_[128]{}, edit_[16384]{}, console_[16384]{"2 3 +"};
    std::string fileMode_, notice_, lastSaved_;
    bool showConsole_ = false, showDemo_ = false, closeRequested_ = false, confirmClose_ = false;
    float scale_ = 1;

  public:
    Document doc;
    Backend engine;
    json items;
    std::string filename, probe, testInput;
    int testSequence = 0;
    void driveTestInput();
    explicit UI(std::string launcher, float scale);
    void draw();
    void requestClose();
    void loadDocument(const std::string &file);
    bool shouldClose() const { return closeRequested_; }
    void writeProbe();
};
} // namespace sapfui
