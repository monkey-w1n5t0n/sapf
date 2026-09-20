#pragma once
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>
namespace sapfui {
using json = nlohmann::json;
struct Spec {
    std::string name, category, help;
    std::vector<std::string> labels;
    std::vector<double> defaults, low, high;
    std::vector<std::string> units;
    std::set<int> scalar;
};
const std::vector<Spec> &specs();
const Spec &spec(const std::string &op);
const char *syntaxName(int n);
std::string number(double v);
struct Node {
    int id = 0;
    std::string op = "number", label, unit;
    double value = 0, low = -1, high = 1;
    bool logarithmic = false, open = true, text = false;
    std::vector<int> args;
    int implementation = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Node, id, op, label, unit, value, low, high, logarithmic, open, text, args,
                                   implementation)
struct Graph {
    int next = 1;
    std::map<int, Node> nodes;
    std::vector<int> roots;
    int numeric(double value, std::string label = "", double low = -1, double high = 1,
                std::string unit = "");
    int call(std::string op, std::vector<int> args);
    int create(const std::string &op);
    int clone(int id);
    std::string render(int id, int syntax) const;
    std::string code(int id, bool live = false, bool scalar = false) const;
    std::set<int> reachable(int id) const;
    std::set<int> scalarNumbers(int id) const;
    int parsePostfix(const std::string &text);
    void validate() const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Graph, next, nodes, roots)
struct Style {
    int theme = 1, syntax = 0;
    bool sliders = true, numbers = true, labels = true, units = true, ticks = true, boxes = true;
    float sliderWidth = 180, labelWidth = 92, numberWidth = 90, rounding = 4, spacing = 5, padding = 6,
          waveWidth = 2;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Style, theme, syntax, sliders, numbers, labels, units, ticks, boxes,
                                   sliderWidth, labelWidth, numberWidth, rounding, spacing, padding,
                                   waveWidth)
struct State {
    Graph graph;
    Style style;
    int page = 1, selected = 0;
    std::vector<float> widgets{880, .5,  0,   .5, 347.4, 2121, .4, .6, .2, .3,
                               .5,  .75, 200, .8, .5,    .1,   .1, .7, .1};
    std::vector<float> wave{3, .25f, .5f, 0};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(State, graph, style, page, selected, widgets, wave)
State initialState();
struct Revision {
    int parent = -1;
    std::string label, time;
    State state;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Revision, parent, label, time, state)
struct Document {
    State state = initialState();
    std::vector<Revision> history;
    int current = 0;
    Document();
    bool dirty() const;
    void commit(const std::string &label);
    bool undo();
    bool redo();
    void checkout(int index);
    void save(const std::string &filename) const;
    static Document load(const std::string &filename);
};
} // namespace sapfui
