#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <set>

void PrintIncludeError(const string& inc_name, const path& current_file, int line_num) {
    cout << "unknown include file " << inc_name
         << " at file " << current_file.string()
         << " at line " << line_num << endl;
}

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool ProcessFileRecursive(const path& current_file, 
                          const vector<path>& include_dirs, 
                          ostream& out, 
                          set<path>& visited) {
    ifstream in(current_file);
    if (!in.is_open()) {
        return false;
    }

    visited.insert(current_file);

    regex re_quoted(R"re(\s*#\s*include\s*"([^"]*)"\s*)re");
    regex re_angle(R"(\s*#\s*include\s*<([^>]*)>\s*)");
                            
    string line = "";
    int line_num = 0;

    while (getline(in, line)) {
        ++line_num;
        smatch match;

        if (regex_match(line, match, re_quoted)) {
            string inc_name = match[1];
            
            path found_path = current_file.parent_path() / inc_name;
            if (!filesystem::exists(found_path)) {
                found_path = "";
                for (const auto& dir : include_dirs) {
                    path try_path = dir / inc_name;
                    if (filesystem::exists(try_path)) {
                        found_path = try_path;
                        break;
                    }
                }
            }

            if (found_path.empty()) {
                PrintIncludeError(inc_name, current_file, line_num);
                return false; 
            }

            if (!ProcessFileRecursive(found_path, include_dirs, out, visited)) {
                return false;
            }

        } 
        else if (regex_match(line, match, re_angle)) {
            string inc_name = match[1];
            path found_path;

            for (const auto& dir : include_dirs) {
                path try_path = dir / inc_name;
                if (filesystem::exists(try_path)) {
                    found_path = try_path;
                    break;
                }
            }

            if (found_path.empty()) {
                PrintIncludeError(inc_name, current_file, line_num);
                return false;
            }

            if (!ProcessFileRecursive(found_path, include_dirs, out, visited)) {
                return false;
            }
        } 
        else {
            if (line.find("//") != 0) {
                out << line << "\n";
            }
        }
    }
    return true;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    if (!filesystem::exists(in_file)) {
        return false;
    }

    ofstream out(out_file);
    if (!out.is_open()) {
        return false;
    }

    set<path> visited;

    return ProcessFileRecursive(in_file, include_directories, out, visited);
}

string GetFileContents(string file) {
    ifstream stream(file);

    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}