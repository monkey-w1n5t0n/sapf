#include "model.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unistd.h>
namespace sapfui {
const std::vector<Spec> &specs() {
    static const std::vector<Spec> v{
        {"bubbles",
         "User defined functions",
         "Analog bubbles, from SAPF's first example. Expand to edit the actual signal expression.",
         {"center pitch", "lo rate", "hi rate"},
         {81, .4, 8},
         {24, .01, .1},
         {108, 8, 30},
         {"nn", "Hz", "Hz"},
         {}},
        {"sinosc",
         "Unit generators",
         "Sine oscillator. Expand the phase accumulator, phase offset and sine transformation.",
         {"freq", "phase"},
         {440, 0},
         {20, 0},
         {20000, 1},
         {"Hz", "cyc"},
         {}},
        {"lfsaw",
         "Unit generators",
         "Bipolar low-frequency saw. Expand the phase accumulator, wrap and bipolar conversion.",
         {"freq", "phase"},
         {.4, 0},
         {.01, 0},
         {30, 1},
         {"Hz", "cyc"},
         {1}},
        {"lftri",
         "Unit generators",
         "Bipolar low-frequency triangle; scalar initial phase.",
         {"freq", "phase"},
         {1, 0},
         {.01, 0},
         {30, 1},
         {"Hz", "cyc"},
         {1}},
        {"lfpulse",
         "Unit generators",
         "Unipolar low-frequency pulse with duty cycle.",
         {"freq", "phase", "duty"},
         {2, 0, .5},
         {.01, 0, .01},
         {30, 1, .99},
         {"Hz", "cyc", ""},
         {1}},
        {"lfnoise1",
         "Random generators",
         "Linearly interpolated random signal.",
         {"freq"},
         {2},
         {.01},
         {100},
         {"Hz"},
         {}},
        {"combn",
         "Unit generators",
         "Feedback comb delay. Max delay is scalar (0 derives it from a scalar delay).",
         {"in", "delay", "max delay", "decay"},
         {0, .2, 2, 4},
         {-1, .001, .001, .01},
         {1, 2, 4, 12},
         {"", "sec", "sec", "sec"},
         {2}},
        {"lag",
         "Unit generators",
         "Smooth changes over the given lag time.",
         {"in", "time"},
         {0, .1},
         {-1, .001},
         {1, 4},
         {"", "sec"},
         {}},
        {"nnhz",
         "Library",
         "Convert MIDI note number to Hertz: 440 * 2^((nn - 69)/12).",
         {"note"},
         {69},
         {0},
         {127},
         {"nn"},
         {}},
        {"+",
         "All system functions",
         "Add; SAPF expands over arrays and signals.",
         {"x", "y"},
         {0, 0},
         {-100, -100},
         {100, 100},
         {"", ""},
         {}},
        {"-",
         "All system functions",
         "Subtract.",
         {"x", "y"},
         {0, 0},
         {-100, -100},
         {100, 100},
         {"", ""},
         {}},
        {"*", "All system functions", "Multiply.", {"x", "y"}, {0, .04}, {-1, 0}, {1, 1}, {"", ""}, {}},
        {"/", "All system functions", "Divide.", {"x", "y"}, {1, 2}, {-10, .01}, {10, 10}, {"", ""}, {}},
        {"phac",
         "Unit generators",
         "Phase accumulator in cycles: integrate frequency, wrapping to [0,1). Lowered to SAPF lfsaw with a "
         "fixed zero initial phase.",
         {"freq"},
         {1},
         {.01},
         {20000},
         {"Hz"},
         {}},
        {"frac",
         "All system functions",
         "Fractional part: x - floor(x).",
         {"x"},
         {0},
         {-2},
         {2},
         {"cyc"},
         {}},
        {"bi", "Library", "Convert unipolar to bipolar: 2*x - 1.", {"x"}, {.5}, {0}, {1}, {""}, {}},
        {"sin", "All system functions", "Sine of radians.", {"x"}, {0}, {-6.283}, {6.283}, {"rad"}, {}},
        {"exp2", "All system functions", "Two raised to a power.", {"x"}, {1}, {-8}, {12}, {""}, {}},
        {"list",
         "All system functions",
         "Two-channel array. Numeric controls remain independently editable.",
         {"L", "R"},
         {8, 7.23},
         {.01, .01},
         {30, 30},
         {"Hz", "Hz"},
         {}}};
    return v;
}
const Spec &spec(const std::string &op) {
    for (auto &s : specs())
        if (s.name == op)
            return s;
    throw std::runtime_error("Unknown function: " + op);
}
const char *syntaxName(int n) {
    static const char *names[] = {"Algebraic", "Lisp", "Postfix", "Pipeline", "Haskell"};
    return names[std::clamp(n, 0, 4)];
}
std::string number(double v) {
    std::ostringstream s;
    s << std::setprecision(9) << v;
    return s.str();
}
int Graph::numeric(double v, std::string label, double lo, double hi, std::string unit) {
    int id = next++;
    Node n;
    n.id = id;
    n.value = v;
    n.label = label;
    n.low = lo;
    n.high = hi;
    n.unit = unit;
    n.logarithmic = (lo > 0 && hi / lo > 100);
    nodes.emplace(id, n);
    return id;
}
int Graph::call(std::string op, std::vector<int> args) {
    if (args.size() != spec(op).labels.size())
        throw std::runtime_error("Wrong arity for " + op);
    int id = next++;
    Node n;
    n.id = id;
    n.op = op;
    n.args = args;
    nodes.emplace(id, n);
    if (op == "bubbles") {
        int zero = numeric(0, "phase", 0, 1, "cyc");
        int low = call("*", {call("lfsaw", {args[1], zero}), numeric(24, "depth", 0, 48, "nn")});
        int rates = call("list", {args[2], call("*", {args[2], numeric(7.23 / 8, "detune ratio", .5, 1.5)})});
        int high = call("*", {call("lfsaw", {rates, numeric(0, "phase", 0, 1, "cyc")}),
                              numeric(3, "depth", 0, 12, "nn")});
        int hz = call("nnhz", {call("+", {call("+", {low, high}), args[0]})});
        int osc = call("sinosc", {hz, numeric(0, "phase", 0, 1, "cyc")});
        nodes.at(id).implementation =
            call("combn", {call("*", {osc, numeric(.04, "amp", 0, .2)}), numeric(.2, "delay", .001, 2, "sec"),
                           numeric(2, "max delay", .01, 4, "sec"), numeric(4, "decay", .01, 12, "sec")});
        nodes.at(id).open = true;
        nodes.at(nodes.at(id).implementation).open = false;
    } else if (op == "sinosc") {
        int phase = call("+", {call("phac", {args[0]}), args[1]});
        nodes.at(id).implementation =
            call("sin",
                 {call("*", {phase, numeric(6.283185307179586, "radians/cycle", .01, 12.566370614359172)})});
    } else if (op == "lfsaw") {
        int phase = call("+", {call("phac", {args[0]}), args[1]});
        nodes.at(id).implementation =
            call("bi", {call("frac", {call("+", {phase, numeric(.5, "phase bias", 0, 1, "cyc")})})});
    } else if (op == "nnhz") {
        int semitones = call("-", {args[0], numeric(69, "reference note", 0, 127, "nn")});
        int octaves = call("/", {semitones, numeric(12, "notes/octave", 1, 48)});
        nodes.at(id).implementation =
            call("*", {call("exp2", {octaves}), numeric(440, "reference Hz", 200, 880, "Hz")});
    } else if (op == "bi") {
        nodes.at(id).implementation =
            call("-", {call("*", {args[0], numeric(2, "scale", .1, 4)}), numeric(1, "offset", -2, 2)});
    }
    if (nodes.at(id).implementation)
        nodes.at(nodes.at(id).implementation).open = false;
    return id;
}
int Graph::create(const std::string &op) {
    const auto &s = spec(op);
    std::vector<int> a;
    for (size_t i = 0; i < s.defaults.size(); ++i)
        a.push_back(numeric(s.defaults[i], s.labels[i], s.low[i], s.high[i], s.units[i]));
    return call(op, a);
}
int Graph::clone(int id) {
    std::map<int, int> copied;
    std::function<int(int)> copy = [&](int old) {
        if (copied.count(old))
            return copied.at(old);
        Node n = nodes.at(old);
        int k = next++;
        copied[old] = k;
        n.id = k;
        for (int &c : n.args)
            c = copy(c);
        if (n.implementation)
            n.implementation = copy(n.implementation);
        nodes.emplace(k, n);
        return k;
    };
    return copy(id);
}
static std::string join(const std::vector<std::string> &a, const std::string &sep) {
    std::string s;
    for (auto &t : a) {
        if (!s.empty())
            s += sep;
        s += t;
    }
    return s;
}
std::string Graph::render(int id, int syntax) const {
    const auto &n = nodes.at(id);
    if (n.op == "number")
        return number(n.value);
    std::vector<std::string> a;
    for (int k : n.args)
        a.push_back(render(k, syntax));
    if (n.op == "list")
        return "[" + join(a, (syntax == 0 || syntax == 4) ? ", " : " ") + "]";
    if (syntax == 1)
        return "(" + n.op + " " + join(a, " ") + ")";
    if (syntax == 2)
        return join(a, " ") + " " + n.op;
    if (syntax == 3) {
        std::vector<std::string> rest(a.begin() + 1, a.end());
        return "(" + a[0] + " |> " + n.op + (rest.empty() ? "" : "(" + join(rest, ", ") + ")") + ")";
    }
    if (syntax == 4) {
        if (n.op == "+" || n.op == "-" || n.op == "*" || n.op == "/")
            return "(" + a[0] + " " + n.op + " " + a[1] + ")";
        return "(" + n.op + " " + join(a, " ") + ")";
    }
    if ((n.op == "+" || n.op == "-" || n.op == "*" || n.op == "/") && a.size() == 2)
        return "(" + a[0] + " " + n.op + " " + a[1] + ")";
    return n.op + "(" + join(a, ", ") + ")";
}
std::string Graph::code(int id, bool live, bool scalar) const {
    const auto &n = nodes.at(id);
    if (n.op == "number")
        return live && !scalar ? "gui" + std::to_string(id) + " zctl" : number(n.value);
    if (n.implementation)
        return code(n.implementation, live, scalar);
    std::vector<std::string> a;
    auto &s = spec(n.op);
    for (size_t i = 0; i < n.args.size(); ++i)
        a.push_back(code(n.args[i], live, scalar || s.scalar.count(int(i))));
    if (n.op == "list")
        return "[" + join(a, " ") + "]";
    if (n.op == "phac")
        return "(" + a[0] + " 0 lfsaw 0.5 * frac)";
    return "(" + join(a, " ") + " " + n.op + ")";
}
std::set<int> Graph::reachable(int id) const {
    std::set<int> r;
    std::function<void(int)> walk = [&](int k) {
        if (!r.insert(k).second)
            return;
        auto &n = nodes.at(k);
        for (int c : n.args)
            walk(c);
        if (n.implementation)
            walk(n.implementation);
    };
    walk(id);
    return r;
}
std::set<int> Graph::scalarNumbers(int id) const {
    std::set<int> r;
    std::function<void(int, bool)> walk = [&](int k, bool scalar) {
        auto &n = nodes.at(k);
        if (n.op == "number") {
            if (scalar)
                r.insert(k);
            return;
        }
        if (n.implementation) {
            walk(n.implementation, scalar);
            return;
        }
        auto &s = spec(n.op);
        for (size_t i = 0; i < n.args.size(); ++i)
            walk(n.args[i], scalar || s.scalar.count(int(i)));
    };
    walk(id, false);
    return r;
}
int Graph::parsePostfix(const std::string &text) {
    // Transactional parser for the structural vocabulary. Full SAPF goes through the console.
    Graph work = *this;
    std::string spaced;
    for (char c : text) {
        if (c == '[' || c == ']' || c == '(' || c == ')') {
            spaced += ' ';
            spaced += c;
            spaced += ' ';
        } else
            spaced += c;
    }
    std::istringstream in(spaced);
    std::string tok;
    std::vector<int> stack;
    std::vector<size_t> arrays;
    size_t tokens = 0;
    while (in >> tok) {
        if (++tokens > 512)
            throw std::runtime_error("Expression is too large (512 tokens max).");
        if (tok == "(" || tok == ")")
            continue;
        if (tok == "[") {
            arrays.push_back(stack.size());
            continue;
        }
        if (tok == "]") {
            if (arrays.empty() || stack.size() - arrays.back() != 2)
                throw std::runtime_error("Use exactly two values in a structural list.");
            int b = stack.back();
            stack.pop_back();
            int a = stack.back();
            stack.pop_back();
            arrays.pop_back();
            stack.push_back(work.call("list", {a, b}));
            continue;
        }
        char *end = nullptr;
        double value = std::strtod(tok.c_str(), &end);
        if (end != tok.c_str() && !*end) {
            if (!std::isfinite(value) || std::abs(value) > 1e9)
                throw std::runtime_error("Number must be finite and within +/-1e9.");
            double range = std::max(1., std::abs(value) * 2);
            stack.push_back(work.numeric(value, "", value < 0 ? -range : 0, range));
            continue;
        }
        const auto &s = spec(tok);
        size_t floor = arrays.empty() ? 0 : arrays.back();
        if (stack.size() - floor < s.labels.size())
            throw std::runtime_error("Not enough arguments for " + tok);
        std::vector<int> a(stack.end() - s.labels.size(), stack.end());
        stack.resize(stack.size() - s.labels.size());
        for (size_t i = 0; i < a.size(); ++i) {
            auto &n = work.nodes.at(a[i]);
            if (n.op == "number" && n.label.empty()) {
                n.label = s.labels[i];
                n.unit = s.units[i];
                n.low = std::min(n.value, s.low[i]);
                n.high = std::max(n.value, s.high[i]);
            }
        }
        stack.push_back(work.call(tok, a));
    }
    if (!arrays.empty() || stack.size() != 1)
        throw std::runtime_error("Expected one complete postfix expression.");
    work.validate();
    *this = std::move(work);
    return stack[0];
}
void Graph::validate() const {
    if (nodes.empty() || nodes.size() > 20000 || roots.size() > 64)
        throw std::runtime_error("Invalid graph size.");
    int maxid = 0;
    std::map<int, int> marks, heights;
    std::function<void(int, int)> visit = [&](int id, int depth) {
        if (depth > 64)
            throw std::runtime_error("Expression nesting exceeds 64.");
        if (!nodes.count(id))
            throw std::runtime_error("Dangling expression reference.");
        if (marks[id] == 1)
            throw std::runtime_error("Expression cycle.");
        if (marks[id] == 2) {
            if (depth + heights[id] > 64)
                throw std::runtime_error("Expression nesting exceeds 64.");
            return;
        }
        marks[id] = 1;
        auto &n = nodes.at(id);
        if (id <= 0 || id != n.id)
            throw std::runtime_error("Invalid node identity.");
        maxid = std::max(maxid, id);
        if (!std::isfinite(n.value) || !std::isfinite(n.low) || !std::isfinite(n.high) || n.low >= n.high ||
            std::abs(n.value) > 1e9 || n.label.size() > 256 || n.unit.size() > 32)
            throw std::runtime_error("Invalid numeric control.");
        if (n.op == "number") {
            if (!n.args.empty() || n.implementation)
                throw std::runtime_error("Invalid number node.");
        } else {
            if (n.args.size() != spec(n.op).labels.size())
                throw std::runtime_error("Wrong arity.");
            if ((n.op == "bubbles" || n.op == "sinosc" || n.op == "lfsaw" || n.op == "nnhz" ||
                 n.op == "bi") != (n.implementation != 0))
                throw std::runtime_error("Invalid implementation.");
        }
        int height = 0;
        for (int c : n.args) {
            visit(c, depth + 1);
            height = std::max(height, 1 + heights[c]);
        }
        if (n.implementation) {
            visit(n.implementation, depth + 1);
            height = std::max(height, 1 + heights[n.implementation]);
        }
        if (height > 64)
            throw std::runtime_error("Expression nesting exceeds 64.");
        heights[id] = height;
        marks[id] = 2;
    };
    for (auto &[id, n] : nodes)
        visit(id, 0);
    for (int id : roots)
        visit(id, 0);
    if (next <= maxid || next > 100000000)
        throw std::runtime_error("Invalid next node identity.");
}
State initialState() {
    State s;
    int b = s.graph.create("bubbles");
    s.graph.roots.push_back(b);
    s.selected = b;
    return s;
}
static std::string now() {
    auto t = std::time(nullptr);
    std::ostringstream s;
    s << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    return s.str();
}
Document::Document() { history.push_back({-1, "Initial bubbles", now(), state}); }
bool Document::dirty() const { return json(state) != json(history.at(current).state); }
void Document::commit(const std::string &label) {
    if (!dirty())
        return;
    if (history.size() >= 2000)
        throw std::runtime_error("History limit reached (2000). Save a copy before starting a new document.");
    history.push_back({current, label.empty() ? "Edit" : label, now(), state});
    current = int(history.size()) - 1;
}
void Document::checkout(int index) {
    if (index < 0 || index >= int(history.size()))
        throw std::runtime_error("Invalid history entry.");
    state = history[index].state;
    current = index;
}
bool Document::undo() {
    if (dirty()) {
        state = history[current].state;
        return true;
    }
    if (history[current].parent < 0)
        return false;
    checkout(history[current].parent);
    return true;
}
bool Document::redo() {
    for (int i = int(history.size()) - 1; i > current; --i)
        if (history[i].parent == current) {
            checkout(i);
            return true;
        }
    return false;
}
void Document::save(const std::string &filename) const {
    json data = {{"format", "sapf-experiments"},
                 {"version", 1},
                 {"state", state},
                 {"current", current},
                 {"history", history}};
    auto tmp = filename + ".tmp-" + std::to_string(getpid());
    try {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        f.exceptions(std::ios::badbit | std::ios::failbit);
        f << data.dump(2) << '\n';
        f.close();
        std::filesystem::rename(tmp, filename);
    } catch (...) {
        std::error_code e;
        std::filesystem::remove(tmp, e);
        throw;
    }
}
static void validateState(const State &s) {
    s.graph.validate();
    if (!s.graph.nodes.count(s.selected) || s.page < 0 || s.page > 3 || s.style.syntax < 0 ||
        s.style.syntax > 4 || s.style.theme < 0 || s.style.theme > 3 || s.widgets.size() != 19 ||
        s.wave.size() != 4)
        throw std::runtime_error("Invalid document state.");
    for (float v : s.widgets)
        if (!std::isfinite(v) || std::abs(v) > 20000)
            throw std::runtime_error("Invalid widget value.");
    for (float v : s.wave)
        if (!std::isfinite(v) || std::abs(v) > 100)
            throw std::runtime_error("Invalid wave value.");
    auto &t = s.style;
    if (!(t.sliderWidth >= 60 && t.sliderWidth <= 400 && t.numberWidth >= 40 && t.numberWidth <= 200 &&
          t.labelWidth >= 30 && t.labelWidth <= 180 && t.rounding >= 0 && t.rounding <= 16 &&
          t.spacing >= 0 && t.spacing <= 20 && t.padding >= 0 && t.padding <= 20 && t.waveWidth >= 1 &&
          t.waveWidth <= 8))
        throw std::runtime_error("Invalid style dimensions.");
}
Document Document::load(const std::string &filename) {
    if (std::filesystem::file_size(filename) > 32 * 1024 * 1024)
        throw std::runtime_error("Document exceeds 32 MiB.");
    std::ifstream f(filename);
    json j;
    f >> j;
    if (j.at("format") != "sapf-experiments" || j.at("version") != 1)
        throw std::runtime_error("Unsupported document format.");
    Document d;
    d.state = j.at("state").get<State>();
    d.history = j.at("history").get<std::vector<Revision>>();
    d.current = j.at("current");
    if (d.history.empty() || d.history.size() > 2000 || d.current < 0 || d.current >= int(d.history.size()))
        throw std::runtime_error("Invalid history.");
    validateState(d.state);
    for (size_t i = 0; i < d.history.size(); ++i) {
        auto &h = d.history[i];
        if ((i == 0 && h.parent != -1) || (i > 0 && (h.parent < 0 || h.parent >= int(i))))
            throw std::runtime_error("Invalid history parent.");
        validateState(h.state);
    }
    return d;
}
} // namespace sapfui
