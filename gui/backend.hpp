#pragma once
#include "model.hpp"
#include <sys/types.h>
namespace sapfui {
// A separate interpreter process isolates expensive evaluations from the GUI.
class Backend {
    pid_t pid_ = -1;
    int input_ = -1, output_ = -1;
    std::string pending_, launcher_;
    std::map<int, double> values_;
    std::set<int> scalar_;
    std::string topology_;
    unsigned serial_ = 0;
    std::string awaiting_;
    bool scoped_ = false;
    void closeProcess();
    void append(const std::string &s);

  public:
    std::string log, error;
    bool playing = false;
    int root = 0;
    explicit Backend(std::string launcher);
    ~Backend();
    Backend(const Backend &) = delete;
    Backend &operator=(const Backend &) = delete;
    bool start();
    void poll();
    void send(const std::string &text);
    void play(const Graph &, int id);
    void sync(const Graph &);
    void stop();
    void reset();
    bool alive() const { return pid_ > 0; }
};
std::string findLauncher();
} // namespace sapfui
