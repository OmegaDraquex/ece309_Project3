// src/tools/tool_registry.cpp — YOU IMPLEMENT (spec §4.3).
//
// A hand-written hash table using separate chaining.
// No std::unordered_map / std::map / std::set.

#include "tools/tool_registry.h"
#include <cstdint>

// Spec recommends power of two between 1 and 64 for initial buckets
static constexpr std::size_t kInitialBucketCount = 16;
// Keep max load factor at 0.75 to ensure average bucket chain is tiny
static constexpr double kMaxLoadFactorThreshold = 0.75;

ToolRegistry::ToolRegistry()
    : buckets_(kInitialBucketCount), toolCount_(0) {}

ToolRegistry::~ToolRegistry() = default;

// FNV-1a 64-bit hash function as given in spec Appendix A
std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    std::uint64_t hashValue = 1469598103934665603ULL; // FNV offset basis
    for (unsigned char ch : key) {
        hashValue ^= ch;
        hashValue *= 1099511628211ULL; // FNV prime
    }
    return static_cast<std::size_t>(hashValue);
}

void ToolRegistry::rehash(std::size_t newCapacity) {
    std::vector<std::vector<std::unique_ptr<Tool>>> newBuckets(newCapacity);

    // Re-insert every tool into the newly sized bucket array
    for (auto& oldBucket : buckets_) {
        for (auto& toolPtr : oldBucket) {
            std::string toolName = toolPtr->name();
            std::size_t newIdx = hash(toolName) % newCapacity;
            // Move unique_ptr directly: tool itself is not copied or moved on the heap!
            newBuckets[newIdx].push_back(std::move(toolPtr));
        }
    }

    buckets_ = std::move(newBuckets);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (tool == nullptr) {
        return false;
    }

    const std::string toolName = tool->name();

    // Check if tool with this name already exists in its bucket
    std::size_t currentIdx = hash(toolName) % buckets_.size();
    for (const auto& existing : buckets_[currentIdx]) {
        if (existing->name() == toolName) {
            // Duplicate found: spec specifies to destroy new tool and keep original
            tool.reset();
            return false;
        }
    }

    // Spec pitfall #3: check load factor BEFORE inserting to avoid exceeding threshold
    double futureLoadFactor = static_cast<double>(toolCount_ + 1) / static_cast<double>(buckets_.size());
    if (futureLoadFactor > kMaxLoadFactorThreshold) {
        rehash(buckets_.size() * 2);
        // Recalculate index after rehash because bucket count changed
        currentIdx = hash(toolName) % buckets_.size();
    }

    buckets_[currentIdx].push_back(std::move(tool));
    toolCount_++;
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    if (buckets_.empty()) {
        return nullptr;
    }

    std::size_t bucketIdx = hash(name) % buckets_.size();
    const auto& chain = buckets_[bucketIdx];

    for (const auto& toolPtr : chain) {
        if (toolPtr->name() == name) {
            return toolPtr.get();
        }
    }

    return nullptr;
}

std::size_t ToolRegistry::size() const noexcept {
    return toolCount_;
}

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const auto& chain : buckets_) {
        for (const auto& toolPtr : chain) {
            if (toolPtr != nullptr) {
                fn(*toolPtr);
            }
        }
    }
}

std::size_t ToolRegistry::bucket_count() const noexcept {
    return buckets_.size();
}

double ToolRegistry::load_factor() const noexcept {
    if (buckets_.empty()) {
        return 0.0;
    }
    return static_cast<double>(toolCount_) / static_cast<double>(buckets_.size());
}

std::size_t ToolRegistry::max_probe_length() const {
    // For separate chaining, probe length is the size of the longest chain
    std::size_t longestChainCount = 0;
    for (const auto& chain : buckets_) {
        if (chain.size() > longestChainCount) {
            longestChainCount = chain.size();
        }
    }
    return longestChainCount;
}
