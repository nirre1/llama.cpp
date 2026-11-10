// Smart Context by https://github.com/nirre1

#include "smart_context.h"

#include "context_graph.hpp"
#include "log.h"

#include <exception>
#include <memory>
#include <mutex>
#include <utility>

namespace {

std::mutex g_mutex;
std::unique_ptr<ContextGraph> g_graph;
int g_budget = 480;
bool g_enabled = false;

}

void smart_context_configure(const std::string & path, int budget) {
    const std::string file = path.empty() ? "data/context_graph.dat" : path;
    const int words = budget > 0 ? budget : 480;

    std::lock_guard<std::mutex> lock(g_mutex);
    g_budget = words;
    g_enabled = false;
    g_graph.reset();

    try {
        g_graph = std::make_unique<ContextGraph>(file);
        g_enabled = true;
        LOG_INF("smart context: enabled file=%s budget=%d passages=%d topics=%d\n",
                file.c_str(), g_budget, g_graph->passage_count(), g_graph->topic_count());
    } catch (const std::exception & err) {
        LOG_WRN("smart context: disabled, could not open %s: %s\n", file.c_str(), err.what());
    }
}

bool smart_context_enabled() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_enabled && g_graph != nullptr;
}

std::string smart_context_prefix(const std::string & question) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_enabled || g_graph == nullptr) {
        return {};
    }

    try {
        const ContextSlice slice = g_graph->query(question, g_budget);
        if (slice.text.empty()) {
            LOG_INF("%s", "smart context: no passage opened for this question\n");
            return {};
        }

        LOG_INF("smart context: opened topic='%s' words=%d passages=%d topics=%d\n",
                slice.topic.c_str(), slice.tokens, slice.passages, slice.topics_opened);
        return std::string("Saved context for this question follows. ")
            + "Other topics were left closed.\n\n"
            + slice.text
            + "\nUser question:\n";
    } catch (const std::exception & err) {
        LOG_WRN("smart context: query failed: %s\n", err.what());
        return {};
    }
}

void smart_context_store(const std::string & user, const std::string & assistant) {
    if (user.empty() || assistant.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_enabled || g_graph == nullptr) {
        return;
    }

    try {
        g_graph->add("user", user);
        g_graph->add("assistant", assistant);
        LOG_INF("smart context: stored turn, passages=%d topics=%d\n",
                g_graph->passage_count(), g_graph->topic_count());
    } catch (const std::exception & err) {
        LOG_WRN("smart context: store failed: %s\n", err.what());
    }
}
