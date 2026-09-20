#include "model.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
using namespace sapfui;
static int checks = 0;
static void check(bool b, const char *msg) {
    ++checks;
    if (!b)
        throw std::runtime_error(msg);
}
static void rejects(std::function<void()> f, const char *msg) {
    bool threw = false;
    try {
        f();
    } catch (const std::exception &) {
        threw = true;
    }
    check(threw, msg);
}
int main(int argc, char **argv) {
    if (argc == 2 && std::string(argv[1]) == "--bubbles") {
        auto s = initialState();
        std::cout << s.graph.code(s.selected) << '\n';
        return 0;
    }
    try {
        Document d;
        auto &g = d.state.graph;
        int root = g.roots[0], center = g.nodes.at(root).args[0];
        g.validate();
        auto reach = g.reachable(root);
        check(reach.size() > 20, "bubbles is a real expanded expression");
        check(g.code(root).find("combn") != std::string::npos, "bubbles compiles its implementation");
        check(g.code(root).find("bubbles") == std::string::npos, "macros lower to plain SAPF");
        int copy = g.clone(root);
        g.roots.push_back(copy);
        int copyCenter = g.nodes.at(copy).args[0];
        g.nodes.at(copyCenter).value = 55;
        check(g.nodes.at(center).value == 81, "cloned inputs independent");
        check(g.reachable(copy).count(copyCenter), "clone retains internal references");
        for (int a : g.reachable(copy))
            check(!reach.count(a), "clone shares no identities with original");
        g.nodes.at(root).open = false;
        d.commit("clone");
        int branchBase = d.current;
        g.nodes.at(center).value = 90;
        g.nodes.at(root).open = true;
        d.commit("bright");
        int bright = d.current;
        d.undo();
        check(d.current == branchBase, "undo parent");
        check(!g.nodes.at(root).open, "undo restores expansion");
        g.nodes.at(center).value = 60;
        d.commit("low");
        int low = d.current;
        check(d.history[bright].parent == d.history[low].parent, "history fork");
        d.checkout(bright);
        check(g.nodes.at(center).value == 90, "old future retained");
        d.checkout(branchBase);
        d.redo();
        check(d.current == low, "redo follows latest branch");
        g.nodes.at(center).value = 42;
        d.undo();
        check(g.nodes.at(center).value == 60, "undo uncommitted edits first");
        int x = g.parsePostfix("440 0 sinosc 0.04 *");
        check(g.render(x, 0) == "(sinosc(440, 0) * 0.04)", "algebraic projection");
        check(g.render(x, 1) == "(* (sinosc 440 0) 0.04)", "lisp projection");
        check(g.render(x, 2) == "440 0 sinosc 0.04 *", "postfix projection");
        check(g.render(x, 3).find("|>") != std::string::npos, "pipeline projection");
        check(g.render(x, 4) != g.render(x, 1), "Haskell differs from Lisp");
        auto before = json(g);
        rejects([&] { g.parsePostfix("440 unknown"); }, "unknown op rejected");
        check(json(g) == before, "parse failure atomic");
        rejects([&] { g.parsePostfix("1 2"); }, "extra stack values rejected");
        rejects([&] { g.parsePostfix("nan"); }, "NaN rejected");
        rejects([&] { g.parsePostfix("[1 2"); }, "unclosed array rejected");
        auto bad = g;
        bad.nodes.at(root).args[0] = root;
        rejects([&] { bad.validate(); }, "cycles rejected");
        auto deep = g;
        int deepId = deep.numeric(1);
        for (int i = 0; i < 66; ++i)
            deepId = deep.call("sin", {deepId});
        rejects([&] { deep.validate(); }, "depth limit also covers child-first node ordering");
        auto scalar = g.scalarNumbers(root);
        check(!scalar.count(center), "pitch supports live updates");
        int saw = g.create("lftri");
        check(g.scalarNumbers(saw).count(g.nodes.at(saw).args[1]), "phase remains scalar");
        check(g.code(saw, true).find(" zctl 0 lftri") != std::string::npos,
              "scalar phase compiled as number");
        int sine = g.create("sinosc");
        check(g.nodes.at(sine).implementation != 0, "sine has editable implementation");
        check(g.code(sine).find("lfsaw") != std::string::npos, "phase accumulation lowers to SAPF");
        check(g.code(sine).find("phac") == std::string::npos, "no private-language opcode sent to SAPF");
        auto file = (std::filesystem::temp_directory_path() / "sapf-experiment-model-test.json").string();
        d.save(file);
        Document loaded = Document::load(file);
        check(json(d.state) == json(loaded.state), "document round trip");
        check(json(d.history) == json(loaded.history), "all branches round trip");
        auto raw = json::parse(std::ifstream(file));
        raw["history"][1]["parent"] = 999;
        std::ofstream(file) << raw.dump();
        rejects([&] { Document::load(file); }, "invalid history rejected");
        std::filesystem::remove(file);
        std::cout << "PASS " << checks << " expression, projection, clone, history and persistence checks\n";
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
