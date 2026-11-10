// Context graph by https://github.com/nirre1

#include "context_graph.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {

const std::unordered_set<std::string> kStop = {
    "a", "an", "the", "of", "and", "to", "in", "on", "for", "is", "it",
    "this", "that", "with", "as", "by", "from", "or", "be", "at", "are",
    "was", "were", "we", "you", "our", "not", "but", "if", "so", "its",
    "their", "than", "then", "into", "over", "under", "about", "can",
    "will", "just", "more", "each", "when", "what", "how", "which", "who",
    "does", "do", "did", "has", "have", "had", "they", "them", "these",
    "those", "only", "also", "using", "used", "use", "per", "your", "his",
    "her", "she", "him", "any", "all", "out", "up", "there", "here",
    "store", "reply", "word", "fact", "saved", "context", "question",
    "integer", "number", "follow", "follows", "please", "tell", "remember"
};

std::vector<std::string> words(const std::string& text) {
    std::vector<std::string> out;
    std::string current;

    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char character = static_cast<unsigned char>(text[i]);

        if (std::isalnum(character)) {
            current.push_back(static_cast<char>(std::tolower(character)));
        } else if ((character == '.' || character == ',') &&
                   !current.empty() &&
                   std::isdigit(static_cast<unsigned char>(current.back())) &&
                   i + 1 < text.size() &&
                   std::isdigit(static_cast<unsigned char>(text[i + 1]))) {
            current.push_back(static_cast<char>(character));
        } else if (!current.empty()) {
            out.push_back(current);
            current.clear();
        }
    }

    if (!current.empty()) {
        out.push_back(current);
    }

    return out;
}

std::vector<std::string> content_terms(const std::string& text, std::size_t limit) {
    std::unordered_map<std::string, int> counts;

    for (const auto& word : words(text)) {
        const bool numeric = std::any_of(word.begin(), word.end(), [](unsigned char character) {
            return std::isdigit(character);
        });

        if (kStop.count(word) || (!numeric && word.size() < 3)) {
            continue;
        }

        counts[word] += 1;
    }

    std::vector<std::pair<int, std::string>> ranked;
    ranked.reserve(counts.size());

    for (const auto& [term, count] : counts) {
        ranked.push_back({count, term});
    }

    std::sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
        if (left.first != right.first) {
            return left.first > right.first;
        }

        return left.second < right.second;
    });

    std::vector<std::string> terms;

    for (const auto& [count, term] : ranked) {
        (void)count;
        terms.push_back(term);

        if (terms.size() == limit) {
            break;
        }
    }

    return terms;
}

int term_hits(const std::vector<std::string>& terms, const std::string& text) {
    const auto tokens = words(text);
    std::unordered_set<std::string> present(tokens.begin(), tokens.end());
    int hits = 0;

    for (const auto& term : terms) {
        if (present.count(term)) {
            hits += 1;
        }
    }

    return hits;
}

double jaccard(const std::vector<std::string>& left, const std::vector<std::string>& right) {
    if (left.empty() || right.empty()) {
        return 0.0;
    }

    std::vector<std::string> sorted_left = left;
    std::vector<std::string> sorted_right = right;
    std::sort(sorted_left.begin(), sorted_left.end());
    std::sort(sorted_right.begin(), sorted_right.end());

    std::size_t intersection = 0;
    std::size_t right_index = 0;

    for (const auto& term : sorted_left) {
        while (right_index < sorted_right.size() && sorted_right[right_index] < term) {
            right_index += 1;
        }

        if (right_index < sorted_right.size() && sorted_right[right_index] == term) {
            intersection += 1;
        }
    }

    const std::size_t uni = left.size() + right.size() - intersection;
    return uni == 0 ? 0.0 : static_cast<double>(intersection) / static_cast<double>(uni);
}

bool is_identifier(const std::string& term) {
    return std::any_of(term.begin(), term.end(), [](unsigned char character) {
        return std::isdigit(character);
    });
}

bool has_digit(const std::string& text) {
    return std::any_of(text.begin(), text.end(), [](unsigned char character) {
        return std::isdigit(character);
    });
}

bool asks_unknown(const std::vector<std::string>& terms) {
    return std::find(terms.begin(), terms.end(), "unknown") != terms.end();
}

bool reply_unknown(const std::string& text) {
    return text.find("word unknown") != std::string::npos;
}

bool covers_term(const std::unordered_set<std::string>& covered, const std::string& term) {
    if (covered.count(term)) {
        return true;
    }

    if (term.size() < 5) {
        return false;
    }

    for (const auto& word : covered) {
        if (word.size() < 5) {
            continue;
        }

        const bool word_extends_term = word.size() > term.size() && word.compare(0, term.size(), term) == 0;
        const bool term_extends_word = term.size() > word.size() && term.compare(0, word.size(), word) == 0;

        if (word_extends_term || term_extends_word) {
            const std::string& extra = word_extends_term ? word : term;
            const std::size_t stem = word_extends_term ? term.size() : word.size();
            const bool extra_digit = std::any_of(extra.begin() + static_cast<std::ptrdiff_t>(stem), extra.end(), [](unsigned char character) {
                return std::isdigit(character);
            });

            // "targets" covers "target". "kestrel" does not cover "kestrel99".
            if (!extra_digit) {
                return true;
            }
        }
    }

    return false;
}

bool has_fact_word(const std::string& text, const std::vector<std::string>& question) {
    const std::unordered_set<std::string> asked(question.begin(), question.end());

    for (const auto& word : words(text)) {
        if (asked.count(word) || kStop.count(word)) {
            continue;
        }

        const bool numeric = std::any_of(word.begin(), word.end(), [](unsigned char character) {
            return std::isdigit(character);
        });

        if (numeric || word.size() >= 3) {
            return true;
        }
    }

    return false;
}

std::vector<std::string> question_terms(const std::string& question) {
    std::vector<std::string> terms;

    for (const auto& word : words(question)) {
        const bool numeric = std::any_of(word.begin(), word.end(), [](unsigned char character) {
            return std::isdigit(character);
        });

        if (kStop.count(word) || (!numeric && word.size() < 3)) {
            continue;
        }

        if (std::find(terms.begin(), terms.end(), word) == terms.end()) {
            terms.push_back(word);
        }
    }

    return terms;
}

std::string join_terms(const std::vector<std::string>& terms) {
    std::string text;

    for (const auto& term : terms) {
        if (!text.empty()) {
            text.push_back(' ');
        }

        text += term;
    }

    return text;
}

bool keeps_mark(const std::string& text, std::size_t index) {
    const unsigned char mark = static_cast<unsigned char>(text[index]);

    if (mark != '.' && mark != ',' && mark != ':' && mark != '/' && mark != '%') {
        return false;
    }

    if (index == 0 || index + 1 >= text.size()) {
        return false;
    }

    return std::isdigit(static_cast<unsigned char>(text[index - 1])) &&
           std::isdigit(static_cast<unsigned char>(text[index + 1]));
}

struct WordSpan {
    std::string word;
    std::size_t begin = 0;
};

std::vector<WordSpan> word_spans(const std::string& text) {
    std::vector<WordSpan> spans;
    std::string current;
    std::size_t begin = 0;

    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char character = static_cast<unsigned char>(text[i]);

        if (std::isalnum(character)) {
            if (current.empty()) {
                begin = i;
            }

            current.push_back(static_cast<char>(std::tolower(character)));
        } else if ((character == '.' || character == ',') &&
                   !current.empty() &&
                   std::isdigit(static_cast<unsigned char>(current.back())) &&
                   i + 1 < text.size() &&
                   std::isdigit(static_cast<unsigned char>(text[i + 1]))) {
            current.push_back(static_cast<char>(character));
        } else if (!current.empty()) {
            spans.push_back({current, begin});
            current.clear();
        }
    }

    if (!current.empty()) {
        spans.push_back({std::move(current), begin});
    }

    return spans;
}

// Copy original text through the last kept word. A short note is copied from
// the start, so a normal hit stays the same. A long note starts at the first
// question word once that word sits past the budget, which keeps a fact at
// the end. A dot between digits stays, so 7.934 is not rewritten as "7 934".
std::string take_words(
    const std::string& text,
    int budget,
    int& used,
    const std::vector<std::string>& focus = {}
) {
    if (budget <= 0 || text.empty()) {
        return "";
    }

    std::size_t offset = 0;

    if (!focus.empty()) {
        const std::vector<WordSpan> spans = word_spans(text);

        if (static_cast<int>(spans.size()) > budget) {
            const std::unordered_set<std::string> wanted(focus.begin(), focus.end());

            for (std::size_t i = 0; i < spans.size(); ++i) {
                if (!wanted.count(spans[i].word)) {
                    continue;
                }

                if (static_cast<int>(i) >= budget) {
                    offset = spans[i].begin;
                }

                break;
            }
        }
    }

    int seen = 0;
    bool in_word = false;
    std::size_t end = offset;

    for (std::size_t i = offset; i < text.size(); ++i) {
        const unsigned char character = static_cast<unsigned char>(text[i]);
        const bool word = std::isalnum(character) || keeps_mark(text, i);

        if (word) {
            if (!in_word) {
                if (seen == budget) {
                    break;
                }

                seen += 1;
                in_word = true;
            }

            end = i + 1;
        } else {
            in_word = false;
        }
    }

    if (seen == 0) {
        return "";
    }

    used += seen;
    return text.substr(offset, end - offset);
}

std::string unescape(const std::string& value) {
    std::string out;
    out.reserve(value.size());

    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            const char next = value[++i];
            if (next == 'n') {
                out.push_back('\n');
            } else if (next == '\\') {
                out.push_back('\\');
            } else {
                out.push_back(next);
            }
        } else {
            out.push_back(value[i]);
        }
    }

    return out;
}

std::string escape(const std::string& value) {
    std::string out;
    out.reserve(value.size());

    for (char character : value) {
        if (character == '\\') {
            out += "\\\\";
        } else if (character == '\n') {
            out += "\\n";
        } else {
            out.push_back(character);
        }
    }

    return out;
}

}

ContextGraph::ContextGraph(std::string path)
    : path_(std::move(path)) {
    load();
}

void ContextGraph::add(const std::string& title, const std::string& text) {
    if (text.empty()) {
        return;
    }

    Passage passage;
    passage.id = next_id_++;
    passage.title = title.empty() ? "note" : title;
    passage.text = text;
    passage.terms = content_terms(title + " " + text, 12);

    const int existing = best_topic(passage.terms);

    if (existing < 0) {
        Topic topic;
        topic.id = static_cast<int>(topics_.size());
        merge_topic(topic, passage.terms);
        passage.topic = topic.id;
        topic.passage_ids.push_back(passage.id);
        topics_.push_back(std::move(topic));
    } else {
        passage.topic = existing;
        merge_topic(topics_[static_cast<std::size_t>(existing)], passage.terms);
        topics_[static_cast<std::size_t>(existing)].passage_ids.push_back(passage.id);
    }

    passages_.push_back(std::move(passage));
    relink();
    save();
}

void ContextGraph::clear() {
    passages_.clear();
    topics_.clear();
    links_.clear();
    passage_edges_.clear();
    next_id_ = 1;
    save();
}

ContextSlice ContextGraph::query(const std::string& question, int token_budget) const {
    ContextSlice slice;
    slice.topics_stored = static_cast<int>(topics_.size());

    if (token_budget <= 0 || passages_.empty()) {
        return slice;
    }

    const auto terms = question_terms(question);
    std::vector<int> included;
    bool opened_fact = false;
    bool slice_has_digit = false;
    auto append = [&](const Passage& passage, int budget) {
        if (budget <= 0) {
            return;
        }

        int piece_used = 0;
        const std::string piece = take_words(passage.text, budget, piece_used, terms);

        if (piece.empty()) {
            return;
        }

        slice.text += "[" + passage.title + "]\n" + piece + "\n\n";
        slice.tokens += piece_used;
        slice.passages += 1;
        included.push_back(passage.id);

        if (has_fact_word(passage.text, terms)) {
            opened_fact = true;
        }

        if (has_digit(passage.text)) {
            slice_has_digit = true;
        }
    };

    // No usable words: one recent note, not the whole graph.
    if (terms.empty()) {
        append(passages_.back(), std::min(token_budget, 80));
        slice.topic = "recent";
        slice.topics_opened = slice.passages > 0 ? 1 : 0;
        return slice;
    }

    std::unordered_map<std::string, int> frequency;

    for (const auto& passage : passages_) {
        std::unordered_set<std::string> seen;

        for (const auto& term : words(passage.text)) {
            if (seen.insert(term).second) {
                frequency[term] += 1;
            }
        }
    }

    auto matches = [&](const std::string& text, const std::string& term) {
        const auto seen_words = words(text);
        const std::unordered_set<std::string> seen(seen_words.begin(), seen_words.end());
        return covers_term(seen, term);
    };

    auto weight = [&](const std::string& term) {
        const int count = frequency[term];

        if (count <= 3) {
            return 8;
        }

        if (count <= 12) {
            return 2;
        }

        return 1;
    };

    auto answer_number = [&](const std::string& text) {
        for (const auto& word : words(text)) {
            if (!is_identifier(word)) {
                continue;
            }

            if (std::find(terms.begin(), terms.end(), word) == terms.end()) {
                return true;
            }
        }

        return false;
    };

    auto shares_asked_identifier = [&](const std::string& text) {
        bool asked = false;

        for (const auto& term : terms) {
            if (!is_identifier(term)) {
                continue;
            }

            asked = true;

            if (matches(text, term)) {
                return true;
            }
        }

        return !asked;
    };

    struct Scored {
        int score = 0;
        int hits = 0;
        bool digit = false;
        bool answer = false;
        bool question = false;
        bool assistant = false;
        const Passage* passage = nullptr;
    };

    std::vector<Scored> scored;
    bool any_fact = false;

    for (const auto& passage : passages_) {
        if (reply_unknown(passage.text) && !asks_unknown(terms)) {
            continue;
        }

        int hits = 0;
        int score = 0;

        for (const auto& term : terms) {
            if (!matches(passage.text, term)) {
                continue;
            }

            hits += 1;
            score += weight(term);
        }

        if (hits == 0) {
            continue;
        }

        if (has_fact_word(passage.text, terms)) {
            any_fact = true;
        }

        if (passage.title == "assistant") {
            score -= 3;
        }

        scored.push_back({
            score,
            hits,
            has_digit(passage.text),
            answer_number(passage.text),
            passage.text.find('?') != std::string::npos,
            passage.title == "assistant",
            &passage
        });
    }

    if (any_fact) {
        std::vector<Scored> facts;

        for (const auto& item : scored) {
            if (has_fact_word(item.passage->text, terms)) {
                facts.push_back(item);
            }
        }

        if (!facts.empty()) {
            scored = std::move(facts);
        }
    }

    std::sort(scored.begin(), scored.end(), [](const Scored& left, const Scored& right) {
        if (left.answer != right.answer) {
            return left.answer;
        }

        if (left.score != right.score) {
            return left.score > right.score;
        }

        if (left.question != right.question) {
            return !left.question;
        }

        if (left.digit != right.digit) {
            return left.digit;
        }

        if (left.assistant != right.assistant) {
            return !left.assistant;
        }

        return left.passage->id > right.passage->id;
    });

    auto term_set = [&](const Passage& passage) {
        std::unordered_set<std::string> hit;

        for (const auto& term : terms) {
            if (matches(passage.text, term)) {
                hit.insert(term);
            }
        }

        return hit;
    };

    auto already_covered = [&](const std::vector<const Passage*>& seeds, const Passage& candidate) {
        const auto mine = term_set(candidate);

        for (const Passage* seed : seeds) {
            const auto theirs = term_set(*seed);
            bool all = true;

            for (const auto& term : mine) {
                if (!theirs.count(term)) {
                    all = false;
                    break;
                }
            }

            if (all) {
                return true;
            }
        }

        return false;
    };

    std::vector<const Passage*> seeds;
    std::unordered_set<int> seeded;

    bool fact_exists = false;

    for (const auto& item : scored) {
        if (!item.assistant && item.answer && shares_asked_identifier(item.passage->text)) {
            fact_exists = true;
            break;
        }
    }

    if (!scored.empty()) {
        const int top = std::max(scored.front().score, 1);

        for (const auto& item : scored) {
            if (static_cast<int>(seeds.size()) >= 3) {
                break;
            }

            // Graphify drops the tail once the score falls under 20% of the best.
            if (!seeds.empty() && item.score * 5 < top) {
                break;
            }

            // A one-word overlap is not a second fact, and a note with no new
            // number is a repeat of the question.
            if (!seeds.empty() && (item.hits < 2 || !item.answer)) {
                continue;
            }

            if (already_covered(seeds, *item.passage)) {
                continue;
            }

            if (!shares_asked_identifier(item.passage->text)) {
                continue;
            }

            if (item.assistant && fact_exists) {
                continue;
            }

            seeded.insert(item.passage->id);
            seeds.push_back(item.passage);
        }

        for (const auto& term : terms) {
            if (static_cast<int>(seeds.size()) >= 4) {
                break;
            }

            const Passage* best_passage = nullptr;
            int best_score = 0;
            bool best_digit = false;

            for (const auto& item : scored) {
                if (!matches(item.passage->text, term)) {
                    continue;
                }

                if (best_passage == nullptr || item.score > best_score ||
                    (item.score == best_score && item.digit && !best_digit) ||
                    (item.score == best_score && item.digit == best_digit &&
                     item.passage->id > best_passage->id)) {
                    best_passage = item.passage;
                    best_score = item.score;
                    best_digit = item.digit;
                }
            }

            if (best_passage == nullptr || seeded.count(best_passage->id)) {
                continue;
            }

            // Graphify seats one seed per word. That pulled in the blue valve
            // on "temperature". An extra seed needs two question words, or
            // the word itself is an identifier such as harbor04.
            int matched = 0;

            for (const auto& asked : terms) {
                if (matches(best_passage->text, asked)) {
                    matched += 1;
                }
            }

            if ((matched < 2 && !is_identifier(term)) || !answer_number(best_passage->text) ||
                (best_passage->title == "assistant" && fact_exists) ||
                !shares_asked_identifier(best_passage->text)) {
                continue;
            }

            if (already_covered(seeds, *best_passage)) {
                continue;
            }

            seeded.insert(best_passage->id);
            seeds.push_back(best_passage);
        }
    }

    const std::unordered_set<std::string> ask_words = {
        "those", "two", "three", "numbers", "please", "just"
    };

    auto covered_by = [&](const std::string& blob) {
        const auto seen_words = words(blob);
        const std::unordered_set<std::string> seen(seen_words.begin(), seen_words.end());

        for (const auto& term : terms) {
            if (ask_words.count(term) || covers_term(seen, term)) {
                continue;
            }

            return false;
        }

        return true;
    };

    std::string seed_blob;

    for (const Passage* seed : seeds) {
        seed_blob += " ";
        seed_blob += seed->text;
    }

    const bool seeds_cover = !seeds.empty() && covered_by(seed_blob);

    struct Hop {
        const Passage* passage = nullptr;
        int hop = 0;
    };

    std::vector<Hop> ordered;
    std::unordered_set<int> visited;

    for (const Passage* seed : seeds) {
        visited.insert(seed->id);
        ordered.push_back({seed, 0});
    }

    // One note that already answers the question stays one note.
    if (!seeds_cover) {
        std::vector<const Passage*> frontier = seeds;

        for (int hop = 1; hop <= 3 && !frontier.empty(); ++hop) {
            std::vector<const Passage*> next;

            for (const Passage* passage : frontier) {
                const auto edges = passage_edges_.find(passage->id);

                if (edges == passage_edges_.end()) {
                    continue;
                }

                if (hop > 1 && edges->second.size() >= 50) {
                    continue;
                }

                for (const auto& edge : edges->second) {
                    if (!visited.insert(edge.neighbor).second) {
                        continue;
                    }

                    const Passage* neighbor = find_passage(edge.neighbor);

                    if (neighbor == nullptr) {
                        continue;
                    }

                    if (neighbor->title == "assistant" || neighbor->text.find('?') != std::string::npos ||
                        !answer_number(neighbor->text)) {
                        continue;
                    }

                    if (reply_unknown(neighbor->text) && !asks_unknown(terms)) {
                        continue;
                    }

                    ordered.push_back({neighbor, hop});
                    next.push_back(neighbor);
                }
            }

            frontier = std::move(next);
            std::string walked;

            for (const auto& hop_item : ordered) {
                walked += " ";
                walked += hop_item.passage->text;
            }

            if (covered_by(walked)) {
                break;
            }
        }
    }

    std::unordered_set<int> opened_topics;

    for (const auto& hop_item : ordered) {
        if (slice.tokens >= token_budget) {
            break;
        }

        const int before = slice.passages;
        append(*hop_item.passage, token_budget - slice.tokens);

        if (slice.passages == before) {
            continue;
        }

        if (opened_topics.insert(hop_item.passage->topic).second) {
            slice.topics_opened += 1;
        }

        if (slice.topic.empty()) {
            for (const auto& topic : topics_) {
                if (topic.id == hop_item.passage->topic) {
                    slice.topic = topic.label;
                    break;
                }
            }
        }
    }

    if (slice.text.empty()) {
        bool needs_identifier = false;

        for (const auto& term : terms) {
            if (is_identifier(term)) {
                needs_identifier = true;
                break;
            }
        }

        if (!needs_identifier) {
            append(passages_.back(), std::min(token_budget, 80));
            slice.topic = "recent";
            slice.topics_opened = slice.passages > 0 ? 1 : 0;
        }
    }

    return slice;
}

ContextSlice ContextGraph::query_flat(const std::string& question, int token_budget) const {
    ContextSlice slice;
    slice.topics_stored = static_cast<int>(topics_.size());

    if (token_budget <= 0 || passages_.empty()) {
        return slice;
    }

    const auto terms = question_terms(question);
    std::vector<std::pair<int, int>> scored;

    for (const auto& passage : passages_) {
        const int score = term_hits(terms, passage.title + " " + join_terms(passage.terms));
        scored.push_back({score, passage.id});
    }

    std::sort(scored.begin(), scored.end(), [](const auto& left, const auto& right) {
        if (left.first != right.first) {
            return left.first > right.first;
        }

        return left.second < right.second;
    });

    std::vector<int> starts;

    for (const auto& [score, id] : scored) {
        if (score <= 0) {
            break;
        }

        starts.push_back(id);

        if (starts.size() == 3) {
            break;
        }
    }

    if (starts.empty() && !scored.empty()) {
        starts.push_back(scored.front().second);
    }

    std::unordered_set<int> seen;
    std::vector<std::pair<int, int>> queue;

    for (int id : starts) {
        queue.push_back({id, 0});
    }

    std::vector<int> order;

    for (std::size_t index = 0; index < queue.size(); ++index) {
        const int id = queue[index].first;
        const int depth = queue[index].second;

        if (!seen.insert(id).second) {
            continue;
        }

        order.push_back(id);

        if (depth == 2) {
            continue;
        }

        const Passage* passage = find_passage(id);

        if (passage == nullptr) {
            continue;
        }

        for (const auto& other : passages_) {
            if (seen.count(other.id)) {
                continue;
            }

            const bool shares = std::find_first_of(
                passage->terms.begin(),
                passage->terms.end(),
                other.terms.begin(),
                other.terms.end()
            ) != passage->terms.end();

            if (shares) {
                queue.push_back({other.id, depth + 1});
            }
        }
    }

    std::string body;
    int used = 0;

    for (int id : order) {
        if (used >= token_budget) {
            break;
        }

        const Passage* passage = find_passage(id);

        if (passage == nullptr) {
            continue;
        }

        int piece_used = 0;
        const std::string piece = take_words(passage->text, token_budget - used, piece_used);

        if (piece.empty()) {
            break;
        }

        body += "NODE " + passage->title + "\n" + piece + "\n\n";
        used += piece_used;
        slice.passages += 1;
    }

    slice.tokens = used;
    slice.text = body;
    slice.topics_opened = slice.passages > 0 ? 1 : 0;
    slice.topic = "flat";
    return slice;
}

std::string ContextGraph::full_text() const {
    std::string text;

    for (const auto& passage : passages_) {
        text += "[" + passage.title + "]\n" + passage.text + "\n\n";
    }

    return text;
}

int ContextGraph::passage_count() const {
    return static_cast<int>(passages_.size());
}

int ContextGraph::topic_count() const {
    return static_cast<int>(topics_.size());
}

int ContextGraph::link_count() const {
    return static_cast<int>(links_.size());
}

int ContextGraph::best_topic(const std::vector<std::string>& terms) const {
    int best = -1;
    double best_score = 0.2;

    for (const auto& topic : topics_) {
        const double score = jaccard(terms, topic.terms);

        if (score >= best_score) {
            best_score = score;
            best = topic.id;
        }
    }

    return best;
}

void ContextGraph::merge_topic(Topic& topic, const std::vector<std::string>& terms) {
    for (const auto& term : terms) {
        if (std::find(topic.terms.begin(), topic.terms.end(), term) == topic.terms.end()) {
            topic.terms.push_back(term);
        }
    }

    std::sort(topic.terms.begin(), topic.terms.end());

    if (topic.terms.size() > 24) {
        topic.terms.resize(24);
    }

    topic.label.clear();

    for (std::size_t i = 0; i < terms.size() && i < 3; ++i) {
        if (!topic.label.empty()) {
            topic.label.push_back(' ');
        }

        topic.label += terms[i];
    }
}

void ContextGraph::relink() {
    links_.clear();
    passage_edges_.clear();

    for (std::size_t left = 0; left < topics_.size(); ++left) {
        for (std::size_t right = left + 1; right < topics_.size(); ++right) {
            for (const auto& term : topics_[left].terms) {
                if (std::find(topics_[right].terms.begin(), topics_[right].terms.end(), term) != topics_[right].terms.end()) {
                    links_.push_back({topics_[left].id, topics_[right].id, term});
                    break;
                }
            }
        }
    }

    // Identifier edges are the walk Graphify would BFS. A shared rare token
    // such as pier04 links two notes. A word used everywhere does not.
    std::unordered_map<std::string, std::vector<int>> postings;

    for (const auto& passage : passages_) {
        std::unordered_set<std::string> seen;

        for (const auto& term : words(passage.text)) {
            if (!is_identifier(term) || !seen.insert(term).second) {
                continue;
            }

            postings[term].push_back(passage.id);
        }
    }

    for (auto& [term, ids] : postings) {
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

        if (ids.size() < 2 || ids.size() > 8) {
            continue;
        }

        for (std::size_t i = 0; i < ids.size(); ++i) {
            for (std::size_t j = i + 1; j < ids.size(); ++j) {
                passage_edges_[ids[i]].push_back({ids[j], term});
                passage_edges_[ids[j]].push_back({ids[i], term});
            }
        }
    }
}

const ContextGraph::Passage* ContextGraph::find_passage(int id) const {
    for (const auto& passage : passages_) {
        if (passage.id == id) {
            return &passage;
        }
    }

    return nullptr;
}

void ContextGraph::load() {
    if (path_.empty()) {
        return;
    }

    std::ifstream input(path_);

    if (!input) {
        return;
    }

    std::string line;

    while (std::getline(input, line)) {
        if (line.rfind("P\t", 0) != 0) {
            continue;
        }

        std::vector<std::string> fields;
        std::string field;
        std::stringstream stream(line);

        while (std::getline(stream, field, '\t')) {
            fields.push_back(field);
        }

        if (fields.size() < 5) {
            continue;
        }

        Passage passage;

        try {
            passage.id = std::stoi(fields[1]);
            passage.topic = std::stoi(fields[2]);
        } catch (const std::exception&) {
            continue;
        }

        if (passage.topic < 0 || passage.topic > 100000) {
            continue;
        }

        passage.title = unescape(fields[3]);
        passage.text = unescape(fields[4]);

        for (std::size_t i = 5; i < fields.size(); ++i) {
            passage.text.push_back('\t');
            passage.text += unescape(fields[i]);
        }

        passage.terms = content_terms(passage.title + " " + passage.text, 12);
        next_id_ = std::max(next_id_, passage.id + 1);
        passages_.push_back(std::move(passage));
    }

    // Group again from the text. Stored topic ids stay as a fallback if the
    // new grouping cannot place a note.
    std::vector<Passage> loaded = std::move(passages_);
    passages_.clear();
    std::sort(loaded.begin(), loaded.end(), [](const Passage& left, const Passage& right) {
        return left.id < right.id;
    });
    bool changed = false;

    for (auto& passage : loaded) {
        const int previous = passage.topic;
        const int existing = best_topic(passage.terms);

        if (existing < 0) {
            Topic topic;
            topic.id = static_cast<int>(topics_.size());
            merge_topic(topic, passage.terms);
            passage.topic = topic.id;
            topic.passage_ids.push_back(passage.id);
            topics_.push_back(std::move(topic));
        } else {
            passage.topic = existing;
            merge_topic(topics_[static_cast<std::size_t>(existing)], passage.terms);
            topics_[static_cast<std::size_t>(existing)].passage_ids.push_back(passage.id);
        }

        changed = changed || passage.topic != previous;
        passages_.push_back(std::move(passage));
    }

    relink();

    if (changed) {
        save();
    }
}

void ContextGraph::save() const {
    if (path_.empty()) {
        return;
    }

    const std::string temporary = path_ + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);

    if (!output) {
        return;
    }

    for (const auto& passage : passages_) {
        output << "P\t" << passage.id << '\t' << passage.topic << '\t'
               << escape(passage.title) << '\t' << escape(passage.text) << '\n';
    }

    output.close();

    if (output) {
        std::rename(path_.c_str(), (path_ + ".bak").c_str());
        std::rename(temporary.c_str(), path_.c_str());
    }
}
