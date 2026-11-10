#pragma once

// Context graph by https://github.com/nirre1

#include <string>
#include <unordered_map>
#include <vector>

// Topic index plus one opened subgraph.
// Graphify scores every node, walks neighbors, and returns up to 2000 tokens.
// This index scores topics only, then loads the original text of one topic.
struct ContextSlice {
    std::string text;
    std::string topic;
    int tokens = 0;
    int passages = 0;
    int topics_stored = 0;
    int topics_opened = 0;
};

class ContextGraph {
public:
    explicit ContextGraph(std::string path = "");

    void add(const std::string& title, const std::string& text);
    void clear();

    // Open the best topic. Follow a cross-topic link only when the question
    // names that link and the budget still has room.
    ContextSlice query(const std::string& question, int token_budget = 480) const;

    // Graphify-shaped baseline: label match, then a 2-hop walk, capped at the
    // budget. Used to measure the difference, not as the chat path.
    ContextSlice query_flat(const std::string& question, int token_budget = 2000) const;

    std::string full_text() const;
    int passage_count() const;
    int topic_count() const;
    int link_count() const;

private:
    struct Passage {
        int id = 0;
        int topic = -1;
        std::string title;
        std::string text;
        std::vector<std::string> terms;
    };

    struct Topic {
        int id = 0;
        std::string label;
        std::vector<std::string> terms;
        std::vector<int> passage_ids;
    };

    struct Link {
        int from = 0;
        int to = 0;
        std::string term;
    };

    struct PassageEdge {
        int neighbor = 0;
        std::string term;
    };

    std::string path_;
    int next_id_ = 1;
    std::vector<Passage> passages_;
    std::vector<Topic> topics_;
    std::vector<Link> links_;
    std::unordered_map<int, std::vector<PassageEdge>> passage_edges_;

    int best_topic(const std::vector<std::string>& terms) const;
    void merge_topic(Topic& topic, const std::vector<std::string>& terms);
    void relink();
    const Passage* find_passage(int id) const;
    void load();
    void save() const;
};
