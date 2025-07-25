#include <iostream>
#include <fstream>
#include <regex>
#include <string>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <set>
bool debug = false;
std::set<std::string> supportedLanguages = {
    "cpp", "c", "java", "go", "javascript", "typescript", "rust", "python", "ruby", "shell"
};
std::pair<std::string, std::string> getPatternStuff(const std::string& language) {
    std::string singleLine, multiLine;
    if (language == "python") {
        singleLine = R"(#.*)";
        multiLine = R"("""[\s\S]*?"""|'''[\s\S]*?''')";
    } else if (language == "ruby") {
        singleLine = R"(#.*)";
        multiLine = R"(=begin[\s\S]*?=end)";
    } else if (language == "shell") {
        singleLine = R"(#.*)";
    } else if (language == "rust") {
        singleLine = R"(//.*)";
        multiLine = R"(/\*[\s\S]*?\*/|//!.*)";
    } else {
        singleLine = R"(//.*)";
        multiLine = R"(/\*[\s\S]*?\*/)";
    }
    if (debug) std::cerr << "[DEBUG] using regex patterns for " << language << "\n";
    return {singleLine, multiLine};
}
std::pair<std::string, std::unordered_map<std::string, std::string>> makeSureStringStuffIsSafeLOL(const std::string& code) {
    std::regex stringRegex(R"((""".*?"""|'''.*?'''|"(\\.|[^"\\])*"|'(\\.|[^'\\])*'))");
    std::string maskedCode = code;
    std::unordered_map<std::string, std::string> maskMap;
    auto words_begin = std::sregex_iterator(code.begin(), code.end(), stringRegex);
    auto words_end = std::sregex_iterator();
    int offset = 0, i = 0;
    for (std::sregex_iterator it = words_begin; it != words_end; ++it, ++i) {
        std::smatch match = *it;
        std::string placeholder = "__STR_" + std::to_string(i) + "__";
        maskMap[placeholder] = match.str();
        int start = match.position() + offset;
        int len = match.length();
        maskedCode.replace(start, len, placeholder);
        offset += placeholder.length() - len;
    }
    if (debug) std::cerr << "[DEBUG] masked " << maskMap.size() << " strings\n";
    return {maskedCode, maskMap};
}
std::string unmakeSureStringStuffIsSafeLOL(const std::string& code, const std::unordered_map<std::string, std::string>& maskMap) {
    std::string unmasked = code;
    for (const auto& pair : maskMap) {
        size_t pos;
        while ((pos = unmasked.find(pair.first)) != std::string::npos) {
            unmasked.replace(pos, pair.first.length(), pair.second);
        }
    }
    return unmasked;
}
std::string unescapeHtml(const std::string& code) {
    std::unordered_map<std::string, std::string> htmlEntities = {
        {"&lt;", "<"}, {"&gt;", ">"}, {"&amp;", "&"}, {"&quot;", "\""}, {"&apos;", "'"},
        {"&#60;", "<"}, {"&#62;", ">"}, {"&#38;", "&"}, {"&#34;", "\""}, {"&#39;", "'"}
    };
    std::string result = code;
    for (const auto& entity : htmlEntities) {
        size_t pos = 0;
        while ((pos = result.find(entity.first, pos)) != std::string::npos) {
            result.replace(pos, entity.first.length(), entity.second);
            pos += entity.second.length();
        }
    }
    if (debug) std::cerr << "[DEBUG] unescaped html stuff\n";
    return result;
}
std::string removeComments(const std::string& code, const std::string& language) {
    auto [maskedCode, maskMap] = makeSureStringStuffIsSafeLOL(code);
    auto [singleLine, multiLine] = getPatternStuff(language);
    try {
        if (!multiLine.empty())
            maskedCode = std::regex_replace(maskedCode, std::regex(multiLine), "");
        maskedCode = std::regex_replace(maskedCode, std::regex(singleLine), "");
    } catch (const std::regex_error& e) {
        std::cerr << "regex error while removing comments: " << e.what() << std::endl;
        return code;
    }
    if (debug) std::cerr << "[DEBUG] removed comments\n";
    return unmakeSureStringStuffIsSafeLOL(maskedCode, maskMap);
}
void processFile(const std::string& filename, const std::string& language = "cpp",
                bool unescapeHtmlFlag = true, bool noFormat = false,
                bool noRemoveComments = false, const std::string& outputFile = "") {
    if (supportedLanguages.find(language) == supportedLanguages.end()) {
        std::cerr << "unsupported language: " << language << std::endl;
        return;
    }
    if (debug) std::cerr << "[DEBUG] processing file: " << filename << " with language: " << language << "\n";
    std::ifstream input(filename);
    if (!input.is_open()) {
        std::cerr << "error opening file: " << filename << std::endl;
        return;
    }
    std::stringstream buffer;
    buffer << input.rdbuf();
    std::string code = buffer.str();
    input.close();
    std::string cleanedCode = noRemoveComments ? code : removeComments(code, language);
    if (unescapeHtmlFlag) {
        cleanedCode = unescapeHtml(cleanedCode);
    }
    std::stringstream outputBuffer;
    if (noFormat) {
        outputBuffer << cleanedCode;
    } else {
        std::istringstream iss(cleanedCode);
        std::string line;
        while (std::getline(iss, line)) {
            line.erase(line.find_last_not_of(" \t\n\r") + 1);
            if (!line.empty()) {
                outputBuffer << line << '\n';
            }
        }
    }
    std::string outFile = outputFile;
    if (outFile.empty()) {
        std::filesystem::path p(filename);
        outFile = p.stem().string() + "_formatted" + p.extension().string();
    }
    std::ofstream output(outFile);
    if (!output.is_open()) {
        std::cerr << "error writing to file: " << outFile << std::endl;
        return;
    }
    output << outputBuffer.str();
    output.close();
    std::cout << "processed file saved to: " << outFile << std::endl;
    if (debug) std::cerr << "[DEBUG] wrote output to: " << outFile << "\n";
}
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <filename> [--language=lang] [--no-unescape] [--no-format] [--no-remove-comments] [-o output] [--debug]" << std::endl;
        return 1;
    }
    std::string filename;
    std::string language = "cpp";
    std::string output;
    bool unescapeHtml = true;
    bool noFormat = false;
    bool noRemoveComments = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--no-unescape") {
            unescapeHtml = false;
        } else if (arg == "--no-format") {
            noFormat = true;
        } else if (arg == "--no-remove-comments") {
            noRemoveComments = true;
        } else if (arg == "--debug") {
            debug = true;
        } else if (arg.rfind("--language=", 0) == 0) {
            language = arg.substr(11);
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output = argv[++i];
        } else if (filename.empty()) {
            filename = arg;
        } else {
            std::cerr << "unknown argument: " << arg << std::endl;
        }
    }
    if (filename.empty()) {
        std::cerr << "error: no input file provided" << std::endl;
        return 1;
    }
    processFile(filename, language, unescapeHtml, noFormat, noRemoveComments, output);
    return 0;
}
