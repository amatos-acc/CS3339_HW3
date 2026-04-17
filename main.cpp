// Homework 3 - Cache Simulator
// Name: Alexander Matos
// Date: 4/17/2026

#include <algorithm>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

enum class AccessResult {
    Hit,
    Miss
};

enum class MissType {
    None,
    Compulsory,
    Capacity,
    Conflict
};

struct CacheAccessOutcome {
    AccessResult result{AccessResult::Miss};
    MissType missType{MissType::None};
    int hitLevel{0};  // 0=all miss, 1=L1 hit, 2=L2 hit
};

struct CacheLevelConfig {
    std::size_t numEntries{0};
    std::size_t associativity{0};
    std::size_t blockWords{1};  // Extra credit: multi-word blocks
};

struct CacheSimulatorConfig {
    CacheLevelConfig l1{};
    std::optional<CacheLevelConfig> l2{};  // Extra credit: L2 cache
    bool enableMissClassification{true};   // Extra credit: miss categories
    std::string inputFile{};
    std::string outputFile{"cache_sim_output"};
};

struct ParseResult {
    std::optional<CacheSimulatorConfig> config{};
    std::string error{};
};

std::string_view toString(const AccessResult value) {
    return (value == AccessResult::Hit) ? "HIT" : "MISS";
}

std::string_view toString(const MissType value) {
    switch (value) {
        case MissType::None:
            return "NONE";
        case MissType::Compulsory:
            return "COMPULSORY";
        case MissType::Capacity:
            return "CAPACITY";
        case MissType::Conflict:
            return "CONFLICT";
        default:
            return "UNKNOWN";
    }
}

bool isPositiveInteger(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    try {
        (void)std::stoull(text);
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

bool isPowerOfTwo(const std::size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

std::size_t parsePositiveValue(const std::string& text) {
    return static_cast<std::size_t>(std::stoull(text));
}

std::string usage() {
    return "Usage:\n"
           "  ./cache_sim <num_entries> <associativity> <memory_reference_file> "
           "[--block-words N] [--l2-entries N --l2-assoc N] "
           "[--disable-miss-classification]";
}

ParseResult parseArgs(int argc, char* argv[]) {
    if (argc < 4) {
        return ParseResult{.config = std::nullopt,
                           .error = "Expected: num_entries associativity "
                                    "memory_reference_file"};
    }

    const std::string numEntriesText = argv[1];
    const std::string associativityText = argv[2];
    const std::string inputPath = argv[3];

    if (!isPositiveInteger(numEntriesText) ||
        !isPositiveInteger(associativityText)) {
        return ParseResult{.config = std::nullopt,
                           .error = "num_entries and associativity must be "
                                    "positive integers"};
    }

    CacheSimulatorConfig config{};
    config.l1.numEntries = parsePositiveValue(numEntriesText);
    config.l1.associativity = parsePositiveValue(associativityText);
    config.inputFile = inputPath;

    if (!isPowerOfTwo(config.l1.numEntries)) {
        return ParseResult{
            .config = std::nullopt, .error = "num_entries must be power-of-two"};
    }
    if (config.l1.numEntries % config.l1.associativity != 0) {
        return ParseResult{
            .config = std::nullopt,
            .error = "associativity must evenly divide num_entries"};
    }

    for (int i = 4; i < argc; ++i) {
        const std::string_view flag = argv[i];
        if (flag == "--block-words") {
            if (i + 1 >= argc || !isPositiveInteger(argv[i + 1])) {
                return ParseResult{
                    .config = std::nullopt,
                    .error = "--block-words requires a positive integer"};
            }
            config.l1.blockWords = parsePositiveValue(argv[++i]);
            continue;
        }
        if (flag == "--l2-entries") {
            if (i + 1 >= argc || !isPositiveInteger(argv[i + 1])) {
                return ParseResult{
                    .config = std::nullopt,
                    .error = "--l2-entries requires a positive integer"};
            }
            if (!config.l2.has_value()) {
                config.l2 = CacheLevelConfig{};
                config.l2->blockWords = config.l1.blockWords;
            }
            config.l2->numEntries = parsePositiveValue(argv[++i]);
            continue;
        }
        if (flag == "--l2-assoc") {
            if (i + 1 >= argc || !isPositiveInteger(argv[i + 1])) {
                return ParseResult{
                    .config = std::nullopt,
                    .error = "--l2-assoc requires a positive integer"};
            }
            if (!config.l2.has_value()) {
                config.l2 = CacheLevelConfig{};
                config.l2->blockWords = config.l1.blockWords;
            }
            config.l2->associativity = parsePositiveValue(argv[++i]);
            continue;
        }
        if (flag == "--disable-miss-classification") {
            config.enableMissClassification = false;
            continue;
        }
        return ParseResult{
            .config = std::nullopt, .error = "Unknown flag: " + std::string{flag}};
    }

    if (config.l2.has_value()) {
        if (config.l2->numEntries == 0 || config.l2->associativity == 0) {
            return ParseResult{
                .config = std::nullopt,
                .error = "L2 needs both --l2-entries and --l2-assoc"};
        }
        if (!isPowerOfTwo(config.l2->numEntries)) {
            return ParseResult{.config = std::nullopt,
                               .error = "L2 entries must be power-of-two"};
        }
        if (config.l2->numEntries % config.l2->associativity != 0) {
            return ParseResult{
                .config = std::nullopt,
                .error = "L2 associativity must evenly divide L2 entries"};
        }
    }

    return ParseResult{.config = config, .error = ""};
}

std::vector<std::uint64_t> loadReferences(const std::string& inputPath) {
    std::ifstream inFile{inputPath};
    if (!inFile) {
        throw std::runtime_error("Unable to open file: " + inputPath);
    }

    std::vector<std::uint64_t> refs{};
    std::uint64_t address = 0;
    while (inFile >> address) {
        refs.push_back(address);
    }

    if (inFile.bad()) {
        throw std::runtime_error("Read error for file: " + inputPath);
    }
    return refs;
}

class CacheLevel {
   public:
    explicit CacheLevel(CacheLevelConfig config) : config_{config} {
        if (config_.numEntries == 0 || config_.associativity == 0) {
            throw std::invalid_argument("Cache config values must be positive");
        }
        if (config_.numEntries % config_.associativity != 0) {
            throw std::invalid_argument(
                "Associativity must divide number of entries");
        }
        if (config_.blockWords == 0) {
            throw std::invalid_argument("Block size must be at least one word");
        }
        sets_.resize(numSets());
    }

    CacheAccessOutcome access(const std::uint64_t wordAddress) {
        const std::uint64_t blockAddress = computeBlockAddress(wordAddress);
        const std::size_t setIndex = computeSetIndex(blockAddress);
        const std::uint64_t tag = computeTag(blockAddress);
        std::vector<std::uint64_t>& setTags = sets_.at(setIndex);

        const auto setHit = std::find(setTags.begin(), setTags.end(), tag);
        if (setHit != setTags.end()) {
            touchSetLru(setTags, tag);
            touchFullyAssociativeModel(blockAddress);
            return CacheAccessOutcome{
                .result = AccessResult::Hit,
                .missType = MissType::None,
                .hitLevel = 1};
        }

        const MissType missType = classifyMiss(blockAddress);
        seenBlocks_.insert(blockAddress);
        touchSetLru(setTags, tag);
        touchFullyAssociativeModel(blockAddress);

        return CacheAccessOutcome{
            .result = AccessResult::Miss, .missType = missType, .hitLevel = 0};
    }

    void insertOnly(const std::uint64_t wordAddress) {
        const std::uint64_t blockAddress = computeBlockAddress(wordAddress);
        const std::size_t setIndex = computeSetIndex(blockAddress);
        const std::uint64_t tag = computeTag(blockAddress);
        std::vector<std::uint64_t>& setTags = sets_.at(setIndex);
        touchSetLru(setTags, tag);
        touchFullyAssociativeModel(blockAddress);
        seenBlocks_.insert(blockAddress);
    }

    void reset() {
        for (auto& set : sets_) {
            set.clear();
        }
        seenBlocks_.clear();
        fullAssocModelLru_.clear();
    }

   private:
    [[nodiscard]] std::size_t numSets() const {
        return config_.numEntries / config_.associativity;
    }

    [[nodiscard]] std::uint64_t computeBlockAddress(
        const std::uint64_t wordAddress) const {
        return wordAddress / config_.blockWords;
    }

    [[nodiscard]] std::size_t computeSetIndex(
        const std::uint64_t blockAddress) const {
        return static_cast<std::size_t>(blockAddress % numSets());
    }

    [[nodiscard]] std::uint64_t computeTag(const std::uint64_t blockAddress) const {
        return blockAddress / numSets();
    }

    [[nodiscard]] MissType classifyMiss(const std::uint64_t blockAddress) const {
        if (!seenBlocks_.contains(blockAddress)) {
            return MissType::Compulsory;
        }

        const auto it = std::find(fullAssocModelLru_.begin(),
                                  fullAssocModelLru_.end(), blockAddress);
        if (it != fullAssocModelLru_.end()) {
            return MissType::Conflict;
        }
        return MissType::Capacity;
    }

    void touchSetLru(std::vector<std::uint64_t>& setTags, const std::uint64_t tag) {
        auto it = std::find(setTags.begin(), setTags.end(), tag);
        if (it != setTags.end()) {
            setTags.erase(it);
        } else if (setTags.size() == config_.associativity) {
            // LRU eviction: remove oldest (front), append newest (back).
            setTags.erase(setTags.begin());
        }
        setTags.push_back(tag);
    }

    void touchFullyAssociativeModel(const std::uint64_t blockAddress) {
        auto it = std::find(fullAssocModelLru_.begin(), fullAssocModelLru_.end(),
                            blockAddress);
        if (it != fullAssocModelLru_.end()) {
            fullAssocModelLru_.erase(it);
        } else if (fullAssocModelLru_.size() == config_.numEntries) {
            fullAssocModelLru_.erase(fullAssocModelLru_.begin());
        }
        fullAssocModelLru_.push_back(blockAddress);
    }

    CacheLevelConfig config_{};
    std::unordered_set<std::uint64_t> seenBlocks_{};
    std::vector<std::vector<std::uint64_t>> sets_{};
    std::vector<std::uint64_t> fullAssocModelLru_{};
};

class CacheSimulator {
   public:
    explicit CacheSimulator(CacheSimulatorConfig config)
        : config_{std::move(config)}, l1_{config_.l1} {
        if (config_.l2.has_value()) {
            l2_.emplace(*config_.l2);
        }
    }

    std::vector<CacheAccessOutcome> run(
        const std::vector<std::uint64_t>& references) {
        std::vector<CacheAccessOutcome> outcomes{};
        outcomes.reserve(references.size());
        for (const std::uint64_t address : references) {
            outcomes.push_back(processReference(address));
        }
        return outcomes;
    }

   private:
    CacheAccessOutcome processReference(const std::uint64_t wordAddress) {
        CacheAccessOutcome l1Result = l1_.access(wordAddress);
        if (l1Result.result == AccessResult::Hit) {
            l1Result.hitLevel = 1;
            return l1Result;
        }

        if (!l2_.has_value()) {
            return l1Result;
        }

        CacheAccessOutcome l2Result = l2_->access(wordAddress);
        if (l2Result.result == AccessResult::Hit) {
            l1_.insertOnly(wordAddress);  // promote into L1
            return CacheAccessOutcome{
                .result = AccessResult::Hit,
                .missType = MissType::None,
                .hitLevel = 2};
        }

        return CacheAccessOutcome{
            .result = AccessResult::Miss,
            .missType = l1Result.missType,
            .hitLevel = 0};
    }

    CacheSimulatorConfig config_{};
    CacheLevel l1_;
    std::optional<CacheLevel> l2_{};
};

int main(int argc, char* argv[]) {
    const ParseResult parsed = parseArgs(argc, argv);
    if (!parsed.config.has_value()) {
        std::cerr << "Input error: " << parsed.error << '\n';
        std::cerr << usage() << '\n';
        return 1;
    }

    std::vector<std::uint64_t> references{};
    try {
        references = loadReferences(parsed.config->inputFile);
    } catch (const std::exception& ex) {
        std::cerr << "Failed to read reference file: " << ex.what() << '\n';
        return 1;
    }

    CacheSimulator simulator{*parsed.config};
    const std::vector<CacheAccessOutcome> outcomes = simulator.run(references);

    std::ofstream outFile{parsed.config->outputFile};
    if (!outFile) {
        std::cerr << "Failed to open output file: " << parsed.config->outputFile
                  << '\n';
        return 1;
    }

    for (std::size_t i = 0; i < references.size(); ++i) {
        outFile << references[i] << " : " << toString(outcomes[i].result);
        if (outcomes[i].result == AccessResult::Hit) {
            outFile << " (L" << outcomes[i].hitLevel << ")";
        } else if (parsed.config->enableMissClassification) {
            outFile << " (" << toString(outcomes[i].missType) << ")";
        }
        outFile << '\n';
    }

    std::cout << "Wrote " << outcomes.size() << " lines to "
              << parsed.config->outputFile << '\n';
    return 0;
}
