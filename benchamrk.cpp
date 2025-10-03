#include <iostream>
#include <fstream>
#include <string>
#include <chrono>

// Forward declarations of your two parsers.
// They should return true if parsing succeeded, false otherwise.
bool parseNorm(const std::string &json);
bool parseSimd(const std::string &json);

using Clock = std::chrono::high_resolution_clock;

struct Result {
    double total_time = 0.0;
    int iterations = 0;
    bool success = true;
};

Result benchmark_parser(const std::string &name,
                        bool (*parser)(const std::string &),
                        const std::string &json,
                        int iterations) {
    Result result;
    result.iterations = iterations;

    for (int i = 0; i < iterations; i++) {
        auto start = Clock::now();
        if (!parser(json)) {
            std::cerr << "[" << name << "] Parsing failed on iteration " << i << "\n";
            result.success = false;
            break;
        }
        auto end = Clock::now();
        double elapsed = std::chrono::duration<double>(end - start).count();
        result.total_time += elapsed;
    }

    if (result.success) {
        double avg = result.total_time / iterations;
        double throughput = (json.size() * iterations / (1024.0 * 1024.0)) / result.total_time;
        std::cout << "[" << name << "] Total: " << result.total_time << "s, "
                  << "Avg: " << avg << "s per iteration, "
                  << "Throughput: " << throughput << " MB/s\n";
    }

    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./benchmark <json_file> [iterations]\n";
        return 1;
    }

    std::string filename = argv[1];
    int iterations = (argc > 2) ? std::stoi(argv[2]) : 10;

    // Load JSON file
    std::ifstream in(filename);
    if (!in) {
        std::cerr << "Failed to open file: " << filename << "\n";
        return 1;
    }
    std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    std::cout << "Loaded JSON file: " << filename 
              << " (" << json.size() << " bytes)\n";
    std::cout << "Iterations: " << iterations << "\n\n";

    // Benchmark normal parser
    auto norm_result = benchmark_parser("Normal Parser", parseNorm, json, iterations);

    // Benchmark SIMD parser
    auto simd_result = benchmark_parser("SIMD Parser", parseSimd, json, iterations);

    // Speedup factor
    if (norm_result.success && simd_result.success && simd_result.total_time > 0) {
        std::cout << "\nSpeedup (Normal/Simd): "
                  << (norm_result.total_time / simd_result.total_time) << "x\n";
    }

    return 0;
}
