# Smart Context

Built by [nirre1](https://github.com/nirre1).

Smart Context is an optional graph retrieval step inside llama-server. It uses the same `ContextGraph` already used by autonomous-ai (`core/context_graph.hpp` and `core/context_graph.cpp`). Those two files are copied here unchanged. The server does not start a Python service and it does not shell out to Graphify.

## Install

This fork compiles Smart Context into `llama-common` by default, and `cmake --install` installs that library with `llama-server`. A fresh build does not need an extra CMake flag:

```
cmake -B build
cmake --build build --target llama-server
cmake --install build
```

`-DLLAMA_SMART_CONTEXT=OFF` leaves the graph out of that install.

## Switch

The installed server still leaves the graph unloaded until runtime configuration turns it on:

1. Build: `LLAMA_SMART_CONTEXT` defaults to ON for this fork. OFF compiles the calls out.
2. Run: `--smart-context` loads the graph in a model server. `--no-smart-context` leaves it unloaded. `llama-cli` accepts the same flags and copies them into the `llama-server` it starts, so a terminal chat uses the graph too. `--server-base` connects to a server that is already running; the graph follows that server.

Related flags:

- `--smart-context-file PATH` (env `LLAMA_ARG_SMART_CONTEXT_FILE`). Default `data/context_graph.dat`, the same relative path the existing client uses.
- `--smart-context-budget N` (env `LLAMA_ARG_SMART_CONTEXT_BUDGET`). Default 480 words, the same budget as `LLMClient::chat`.

A request may set `"smart_context": false` to skip retrieval and storage for that call. The field is removed before the rest of the request is parsed, including when the feature is off, so an unknown parameter is not forwarded into sampling.

Router mode proxies `/v1/chat/completions` to a child process. The router does not open the graph. The flags use the preset env names, so a router started with `--smart-context` forwards them to every child. The child loads the graph only when it has a model path.

## Request path

`server_routes::post_chat_completions` runs before `oaicompat_chat_params_parse`:

1. Read the last user message.
2. `ContextGraph::query` with the word budget.
3. If a slice comes back, put it in front of that user message with the same wording `LLMClient::chat` uses. The original user text is what gets stored later.
4. The chat template and inference then run on the augmented message.

After a successful chat completion, the final assistant text is stored with `add("user", ...)` and `add("assistant", ...)`. Failures and cancelled streams that never produce a final message do not write. Streaming stores once, on the final chunk. Other endpoints (completions, embeddings, transcription) are unchanged.

The graph is guarded by one mutex. `ContextGraph` itself is not modified.

## Disabled behavior

Without the CMake option, the header calls compile to empty inlines and the graph sources are not linked. Without `--smart-context`, a build that includes the feature never opens the file and never rewrites the prompt.
