#include "label_path.hpp"

std::vector<std::string> split_label(const std::string& label) {
    std::vector<std::string> out;
    std::string seg;
    auto flush = [&] {
        std::size_t b = seg.find_first_not_of(" \t");
        if (b == std::string::npos) {
            out.emplace_back();  // whitespace-only (or empty) -> empty segment
        } else {
            std::size_t e = seg.find_last_not_of(" \t");
            out.push_back(seg.substr(b, e - b + 1));
        }
        seg.clear();
    };
    for (char c : label) {
        if (c == '/') flush();
        else seg.push_back(c);
    }
    flush();  // always emit the final (leaf) segment, even for an empty label
    return out;
}
