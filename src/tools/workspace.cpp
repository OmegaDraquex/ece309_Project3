// src/tools/workspace.cpp — YOU IMPLEMENT (spec §4.4).
//
// Workspace::resolve: a purely lexical check, steps 1–6 of the spec.
// Do not touch the filesystem here (no exists(), canonical(), ...).
// The constructor, root() and check_symlinks() are provided in
// workspace_provided.cpp.

#include "tools/workspace.h"

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
    // Step 1: empty path check per spec §4.4
    if (user_path.empty()) {
        if (reason != nullptr) {
            *reason = "empty path";
        }
        return false;
    }

    // Step 2: check for embedded NUL byte. string_view can hold '\0' but OS would truncate it
    if (user_path.find('\0') != std::string_view::npos) {
        if (reason != nullptr) {
            *reason = "path contains NUL byte";
        }
        return false;
    }

    // Step 3: convert to fs::path and verify it is workspace-relative
    fs::path inputPath{std::string(user_path)};
    if (inputPath.is_absolute() || inputPath.has_root_path()) {
        if (reason != nullptr) {
            *reason = "absolute paths are not allowed";
        }
        return false;
    }

    // Step 4: lexical normalization folds dots and redundant slashes
    fs::path normalizedPath = inputPath.lexically_normal();

    // Step 5: check if first component is ".."
    // If it starts with .., the path climbed outside the workspace root
    if (normalizedPath.begin() != normalizedPath.end() && *normalizedPath.begin() == "..") {
        if (reason != nullptr) {
            *reason = "path escapes workspace root via '..'";
        }
        return false;
    }

    // Step 6: "." resolves to root() directly, otherwise root() / normalizedPath
    if (out != nullptr) {
        if (normalizedPath == ".") {
            *out = root();
        } else {
            *out = root() / normalizedPath;
        }
    }

    return true;
}
