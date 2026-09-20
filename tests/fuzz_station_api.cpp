// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 PocketSSTV contributors
//
// Seeded fuzz pass over the JSON parser and API dispatch.
//
// The control plane parses input that may arrive from a network (R-CLI-6),
// so it is attack surface. This runs a fixed seed in CI as a smoke test;
// the nightly job runs it longer, under sanitizers, per the test strategy.
//
// Invariants: dispatch never crashes or hangs on any input, and whatever it
// returns is always valid JSON.
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include "station/api.h"

int main(int argc, char **argv) {
    unsigned seed = argc > 1 ? (unsigned)atoi(argv[1]) : 1;
    long iters = argc > 2 ? atol(argv[2]) : 200000;
    std::mt19937 rng(seed);

    // Seed corpus: valid-ish messages that mutation can break in useful ways.
    const char *corpus[] = {
        R"({"jsonrpc":"2.0","id":1,"method":"hello","params":{"api":"1.0"}})",
        R"({"jsonrpc":"2.0","id":2,"method":"system.info"})",
        R"({"a":[1,2,{"b":"é"}],"c":true,"d":null,"e":-1.5e3})",
        R"({"s":"😀 emoji surrogate pair"})",
        "{}", "[]", "null", "\"x\"", "{\"deep\":{\"deep\":{\"deep\":1}}}",
    };
    const int ncorpus = (int)(sizeof corpus / sizeof corpus[0]);
    long crashes = 0, parsed = 0;

    for (long i = 0; i < iters; i++) {
        std::string s = corpus[rng() % ncorpus];
        int mutations = 1 + (int)(rng() % 6);
        for (int m = 0; m < mutations && !s.empty(); m++) {
            switch (rng() % 5) {
            case 0: s[rng() % s.size()] = (char)(rng() % 256); break;          // flip byte
            case 1: s.erase(rng() % s.size(), 1 + rng() % 3); break;           // delete
            case 2: s.insert(rng() % s.size(), 1, (char)(rng() % 256)); break; // insert
            case 3: s.insert(rng() % s.size(), std::string(1 + rng() % 40, '[')); break; // nest
            case 4: s += s.substr(0, std::min<size_t>(s.size(), 32)); break;   // duplicate
            }
        }
        station::Api api;
        std::string out = api.handle_line(s);     // must never crash or hang
        if (!out.empty()) {
            station::json::Value v; std::string err;
            if (station::json::parse(out, &v, &err)) parsed++;
            else { std::printf("INVALID RESPONSE from input: %s\n -> %s\n", s.c_str(), out.c_str()); crashes++; }
        }
    }
    std::printf("fuzz: %ld iterations, responses always valid JSON: %s (%ld parsed, %ld bad)\n",
                iters, crashes ? "NO" : "yes", parsed, crashes);
    return crashes ? 1 : 0;
}
