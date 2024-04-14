#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

#include <openssl/evp.h>

static std::optional<std::string> compute_file_hash(EVP_MD_CTX *context,
                                                    const std::string& fpath)
{
    EVP_MD_CTX_reset(context);
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len{0};

    if (EVP_DigestInit_ex(context, EVP_blake2b512(), nullptr) == 0) {
        return std::nullopt;
    }

    std::ifstream file{fpath};

    if (!file) {
        std::cerr << "error: failed to read: " << fpath << " :"
                  << std::error_code{errno, std::generic_category()}.message()
                  << "\n";
        return std::nullopt;
    }

    constexpr std::size_t bufsize{65536};
    char buf[bufsize];

    while (file) {
        file.read(buf, bufsize);

        const std::size_t count{static_cast<std::size_t>(file.gcount())};

        if (count == 0) {
            break;
        }

        if (EVP_DigestUpdate(context, buf, count) == 0) {
            return std::nullopt;
        }
    }

    if (EVP_DigestFinal_ex(context, hash, &hash_len) == 0) {
        return std::nullopt;
    }

    return std::string{reinterpret_cast<char *>(hash), hash_len};
}

static bool find_duplicates(std::unordered_multimap<std::string, std::string>& file_hashes,
                            const std::string_view& dir)
{
    EVP_MD_CTX *const context{EVP_MD_CTX_new()};

    if (context == nullptr) {
        std::cerr << "error: "
                  << std::error_code{errno, std::generic_category()}.message()
                  << "\n";
        return false;
    }

    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(dir)) {
        std::string entry_path{entry.path().string()};

        if (entry.is_regular_file() && !entry.is_symlink()) {
            const auto res{compute_file_hash(context, entry_path)};

            if (res != std::nullopt) {
                file_hashes.emplace(res.value(), entry_path);
            } else {
                std::cerr << "error: failed to process file: " << entry_path
                          << "\n";
            }
        }     
    }
    EVP_MD_CTX_free(context);
    return true;
}

static std::string str_to_hex(const std::string& str)
{
    std::basic_string<unsigned char> s{str.begin(), str.end()};
    std::stringstream ss{};

    for (unsigned int i = 0; i < s.size(); ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(s[i]);
    }

    return ss.str();
}

static void print_duplicates(std::unordered_multimap<std::string, std::string>& file_hashes)
{
    std::string curr_key{""};

    for (auto it{file_hashes.begin()}; it != file_hashes.end(); ++it) {
        if (curr_key == it->first) {
            continue;
        }
        else {
            curr_key = it->first;
        }

        const auto res{file_hashes.equal_range(it->first)};

        if (std::distance(res.first, res.second) > 1) {
            std::cout << str_to_hex(it->first) << "\n";

            for (auto it{res.first}; it != res.second; ++it) {
                std::cout << "\t" << it->second << "\n";
            }
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "Error: no argument provided.\n"
                  << "Usage: " << (argv[0] ? argv[0] : "dedup ")
                  << "file1 file2 ...\n";
        return EXIT_FAILURE;
    }

    std::unordered_multimap<std::string, std::string> file_hashes;

    for (int i{1}; i < argc; ++i) {
        if (!find_duplicates(file_hashes, argv[i])) {
            return EXIT_FAILURE;
        }
    }
    print_duplicates(file_hashes);
}
