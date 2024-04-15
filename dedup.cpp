#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

#include <openssl/evp.h>

static std::unordered_set<std::string> processed_paths {};

static std::optional<std::string> compute_file_hash(EVP_MD_CTX* context,
                                                    const std::string& fpath)
{
    EVP_MD_CTX_reset(context);
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len {0};
    std::ifstream file {fpath};

    if (EVP_DigestInit_ex(context, EVP_blake2b512(), nullptr) == 0 || !file) {
        return std::nullopt;
    }

    constexpr std::size_t bufsize {65536};
    char buf[bufsize];

    while (file) {
        file.read(buf, bufsize);

        const std::size_t count {static_cast<std::size_t>(file.gcount())};

        if (count == 0) {
            break;
        }

        if (EVP_DigestUpdate(context, buf, count) == 0) {
            return std::nullopt;
        }
    }

    if (file.bad() || EVP_DigestFinal_ex(context, hash, &hash_len) == 0) {
        return std::nullopt;
    }

    return std::string {reinterpret_cast<char*>(hash), hash_len};
}

static void populate_size_map(
    std::unordered_multimap<std::uintmax_t, std::string>& size_map,
    const std::string_view& dir)
{
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(dir)) {
        if (processed_paths.find(entry.path().string()) !=
            processed_paths.end()) {
            continue;
        }
        processed_paths.insert(entry.path().string());

        if (entry.is_regular_file()) {
            std::error_code ecode {};
            const std::uintmax_t fsize =
                std::filesystem::file_size(entry.path(), ecode);

            if (ecode) {
                std::cerr << "error: failed to process "
                          << std::quoted(entry.path().string())
                          << std::error_code {errno, std::generic_category()}
                                 .message()
                          << "\n";
            }
            else {
                size_map.emplace(fsize, entry.path().string());
            }
        }
        else {
            std::cerr << "Skipping entry: "
                      << std::quoted(entry.path().string()) << "\n";
        }
    }
}

static bool populate_hash_map(
    const std::unordered_multimap<std::uintmax_t, std::string>& size_map,
    std::unordered_multimap<std::string, std::string>& hash_map)
{
    EVP_MD_CTX* const context {EVP_MD_CTX_new()};

    if (context == nullptr) {
        std::cerr << "error: "
                  << std::error_code {errno, std::generic_category()}.message()
                  << "\n";
        return false;
    }

    for (auto it {size_map.begin()}; it != size_map.end(); ++it) {
        const auto res {size_map.equal_range(it->first)};

        if (std::distance(res.first, res.second) > 1) {
            const auto hash {compute_file_hash(context, it->second)};

            if (hash != std::nullopt) {
                hash_map.emplace(hash.value(), it->second);
            }
            else {
                std::cerr << "error: failed to process "
                          << std::quoted(it->second) << "\n";
            }
        }
    }

    EVP_MD_CTX_free(context);
    return true;
}

static std::string str_to_hex(const std::string& str)
{
    std::basic_string<unsigned char> s {str.begin(), str.end()};
    std::stringstream ss {};

    for (unsigned int i = 0; i < s.size(); ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(s[i]);
    }

    return ss.str();
}

static void
print_duplicates(std::unordered_multimap<std::string, std::string>& hash_map)
{
    std::string curr_key {""};

    for (auto it {hash_map.begin()}; it != hash_map.end(); ++it) {
        if (curr_key == it->first) {
            continue;
        }
        else {
            curr_key = it->first;
        }
        const auto res {hash_map.equal_range(it->first)};

        if (std::distance(res.first, res.second) > 1) {
            std::cout << str_to_hex(it->first) << "\n";

            for (auto it {res.first}; it != res.second; ++it) {
                std::cout << "\t" << it->second << "\n";
            }
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Error: no argument provided.\n"
                  << "Usage: " << (argv[0] ? argv[0] : "dedup ")
                  << "file1 file2 ...\n";
        return EXIT_FAILURE;
    }

    std::unordered_multimap<std::uintmax_t, std::string> size_map {};
    std::unordered_multimap<std::string, std::string> hash_map {};

    for (int i {1}; i < argc; ++i) {
        populate_size_map(size_map, argv[i]);

        if (!populate_hash_map(size_map, hash_map)) {
            return EXIT_FAILURE;
        }
    }

    print_duplicates(hash_map);
}
