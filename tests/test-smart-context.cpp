#include "context_graph.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string filler(const std::string& sentence, int copies) {
    std::string text;

    for (int i = 0; i < copies; ++i) {
        text += sentence;
        text.push_back(' ');
    }

    return text;
}

}

int main() {
    try {
        ContextGraph graph("/tmp/context-graph-test.dat");
        graph.clear();

        const std::string power = filler(
            "pascal watts clocks thermal firmware cap card",
            12
        );
        const std::string cooling = filler(
            "heatsink fan curve degrees airflow firmware shroud",
            12
        );
        const std::string wiki = filler(
            "wikitext perplexity chunks estimate qwen",
            12
        );

        graph.add(
            "power-cap",
            "The enforced GPU power cap is 151 watts and the hard ceiling is 155 watts. " + power
        );
        graph.add(
            "power-clocks",
            "Under that cap the card holds about 1830 MHz. " + power
        );
        graph.add(
            "cooling-fan",
            "The heatsink fan curve targets 70 degrees. " + cooling
        );
        graph.add(
            "wiki-score",
            "Qwen3.5 9B q8 perplexity on wikitext was 7.934. " + wiki
        );

        check(graph.topic_count() >= 3, "topics were merged that should stay apart");

        const ContextSlice slice = graph.query("What power cap is enforced on the GPU?", 480);
        const ContextSlice flat = graph.query_flat("What power cap is enforced on the GPU?", 2000);

        check(slice.text.find("151") != std::string::npos, "opened slice missed the power cap");
        check(slice.text.find("7.934") == std::string::npos, "opened slice reread the perplexity topic");
        check(slice.tokens <= 480, "opened slice exceeded the token budget");
        check(slice.topics_opened == 1, "power question followed a link it did not name");
        check(flat.tokens > slice.tokens, "flat walk was not larger than the topic slice");
        check(graph.passage_count() > slice.passages, "slice did not drop any passages");
        check(graph.full_text().find("7.934") != std::string::npos, "corpus lost the perplexity fact");

        ContextGraph numbers("/tmp/context-graph-numbers.dat");
        numbers.clear();
        numbers.add("wiki-score", "Qwen3.5 9B q8 perplexity on wikitext was 7.934.");
        const ContextSlice number_slice = numbers.query("What was the wikitext perplexity?", 480);
        check(number_slice.text.find("7.934") != std::string::npos, "decimal was stripped from the opened slice");
        check(number_slice.text.find("7 934") == std::string::npos, "decimal was rewritten as two words");

        ContextGraph wrapped("/tmp/context-graph-wrapped.dat");
        wrapped.clear();
        wrapped.add(
            "user",
            "Store this fact and reply with only the word ok: the enforced GPU power cap is 151 watts."
        );
        wrapped.add(
            "user",
            "Store this fact and reply with only the word ok: the wikitext perplexity was 7.934."
        );
        const ContextSlice wrapped_slice = wrapped.query(
            "Using only the saved context, what was the wikitext perplexity?",
            480
        );
        check(wrapped_slice.text.find("7.934") != std::string::npos, "boilerplate hid the perplexity fact");
        check(wrapped_slice.text.find("151") == std::string::npos, "boilerplate pulled in the power fact");

        const ContextSlice recent = graph.query("ok", 480);
        check(recent.topic == "recent", "a question with no content words did not fall back");
        check(recent.tokens <= 80, "recent fallback exceeded its cap");
        check(!recent.text.empty(), "recent fallback was empty");

        {
            std::ofstream corrupt("/tmp/context-graph-corrupt.dat", std::ios::trunc);
            corrupt << "not a row\n";
            corrupt << "P\tbad\t0\tbroken\tnope\n";
            corrupt << "P\t1\t9999999\tskip\ttoo many topics\n";
            corrupt << "P\t2\t0\twiki\tQwen perplexity was 7.934 on wikitext.\n";
        }

        ContextGraph corrupt("/tmp/context-graph-corrupt.dat");
        const ContextSlice recovered = corrupt.query("wikitext perplexity", 480);
        check(recovered.text.find("7.934") != std::string::npos, "corrupt graph file dropped the good row");

        ContextGraph buried("/tmp/context-graph-buried.dat");
        buried.clear();
        std::string long_note = "Store this note and reply with only the word ok. ";

        for (int i = 0; i < 80; ++i) {
            long_note += "airflow shroud housing ";
        }

        long_note += "The buried bench code is 4242.";
        buried.add("buried", long_note);
        const ContextSlice buried_slice = buried.query("What is the buried bench code?", 40);
        check(buried_slice.text.find("4242") != std::string::npos, "a fact at the end of a long note was cut off");
        check(buried_slice.tokens <= 40, "the buried-fact window exceeded its budget");

        const ContextSlice both = graph.query("What is the power cap and the heatsink fan target?", 480);
        check(both.text.find("151") != std::string::npos, "two-fact question missed the power cap");
        check(both.text.find("70") != std::string::npos, "two-fact question missed the fan target");
        check(both.tokens <= 480, "two-fact question exceeded the budget");
        check(slice.text.find("70") == std::string::npos, "a one-fact power question pulled in the fan note");

        graph.add(
            "user",
            "Using only the saved context, what is the power cap and the heatsink fan target?"
        );
        const ContextSlice echoed = graph.query("What is the power cap and the heatsink fan target?", 480);
        check(echoed.text.find("151") != std::string::npos, "a stored question hid the power cap");
        check(echoed.text.find("70") != std::string::npos, "a stored question hid the fan target");

        ContextGraph fan("/tmp/context-graph-fan.dat");
        fan.clear();
        fan.add("fan", "Store this fact and reply with only the word ok: the heatsink fan curve targets 70 degrees.");
        fan.add("user", "Heatsink fan curve target degrees from saved context? Reply with only the integer.");
        fan.add("user", "Store this fact and reply with only the word ok: the blue valve temperature is 44 degrees.");
        const ContextSlice fan_slice = fan.query(
            "Using only the saved context, what temperature does the heatsink fan curve target? Reply with only the integer.",
            480
        );
        check(fan_slice.text.find("70") != std::string::npos, "fan question missed 70");
        check(fan_slice.text.find("44") == std::string::npos, "fan question opened the blue valve");

        ContextGraph speed("/tmp/context-graph-speed.dat");
        speed.clear();
        speed.add("user", "Using only the saved context, what PCIe offload speed was stored? If it was not stored, reply with only the word unknown.");
        speed.add("user", "Store this fact and reply with only the word ok: the 2B generation speed was 93.5 tokens per second.");
        const ContextSlice speed_slice = speed.query(
            "Store this fact and reply with only the word ok: the spindle speed is 4400.",
            480
        );
        check(speed_slice.text.find("unknown") == std::string::npos, "a store followed the unknown instruction");

        const ContextSlice three = graph.query(
            "What is the GPU power cap, the heatsink fan target, and the wikitext perplexity?",
            480
        );
        check(three.text.find("151") != std::string::npos, "three-fact question missed the power cap");
        check(three.text.find("70") != std::string::npos, "three-fact question missed the fan target");
        check(three.text.find("7.934") != std::string::npos, "three-fact question missed the perplexity");
        check(three.tokens <= 480, "three-fact question exceeded the budget");

        ContextGraph tanks("/tmp/context-graph-tanks.dat");
        tanks.clear();
        tanks.add("user", "Store this fact and reply with only the word ok: the north tank level is 19.");
        tanks.add("user", "Store this fact and reply with only the word ok: the north tank level is 31.");
        const ContextSlice latest = tanks.query("What is the north tank level?", 480);
        check(latest.text.find("31") != std::string::npos, "the newer tank level was dropped");
        check(latest.text.find("19") == std::string::npos, "the older tank level outranked the update");

        ContextGraph chain("/tmp/context-graph-chain.dat");
        chain.clear();
        chain.add("fact", "site harbor04 contains building pier04.");
        chain.add("fact", "building pier04 contains rack drawer04.");
        chain.add("fact", "rack drawer04 holds server node04.");
        chain.add("fact", "server node04 exposes port 9104.");
        const ContextSlice walked = chain.query("What port number is inside site harbor04?", 480);
        check(walked.text.find("9104") != std::string::npos, "the port walk stopped before the server note");
        check(walked.tokens <= 480, "the port walk exceeded the budget");
        check(slice.topics_opened == 1, "the power question grew after the walk change");

        std::cout << "topics " << graph.topic_count()
                  << " slice_tokens " << slice.tokens
                  << " flat_tokens " << flat.tokens
                  << " opened " << slice.topics_opened
                  << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
