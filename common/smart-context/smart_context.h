#pragma once

#include <string>

// Smart Context by https://github.com/nirre1
// This fork compiles Smart Context in by default. --smart-context turns it on.
// -DLLAMA_SMART_CONTEXT=OFF leaves these calls as empty inlines.

#ifdef LLAMA_SMART_CONTEXT

void smart_context_configure(const std::string & path, int budget);
bool smart_context_enabled();

// Text placed in front of the user question. Empty when nothing was opened.
// The returned text already ends with "User question:\n".
std::string smart_context_prefix(const std::string & question);

// Store the original user text and the assistant reply. No-op when disabled.
void smart_context_store(const std::string & user, const std::string & assistant);

#else

inline void smart_context_configure(const std::string &, int) {}
inline bool smart_context_enabled() { return false; }
inline std::string smart_context_prefix(const std::string &) { return {}; }
inline void smart_context_store(const std::string &, const std::string &) {}

#endif
