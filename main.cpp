#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;
    
const static regex include_local(R"/(\s*#\s*include\s*"([^"]*)"s*)/");
const static regex include_global(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories);
bool CheckGlobal(const vector<path>& include_directories, const path& in_file, const path& out_file, int line_number, const smatch& match);

bool CheckGlobal(
    const vector<path>& include_directories,
    const path& in_file,
    const path& out_file,
    int line_number,
    const smatch& match
) {
    bool is_found = false;
               
    for (const auto& include_dir : include_directories) {
        path global_include = include_dir / match[1].str();
                    
        if (filesystem::exists(global_include) && filesystem::is_regular_file(global_include)) {
            if (!Preprocess(global_include, out_file, include_directories)) {
                cout << "unknown include file " << global_include << " at file " << static_cast<string>(in_file) << " at line " << line_number << endl;
                return false;
            }

            is_found = true;
            break;
        }
    }
                
    if (!is_found) {
        cout << "unknown include file " << match[1].str() << " at file " << static_cast<string>(in_file) << " at line " << line_number << endl;
        return false;
    }
    
    return is_found;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream input(in_file);
    if (!input.is_open()) {
        return false; // не удалось открыть поток чтения.
    }
    
    ofstream output(out_file, ofstream::app);
    if (!output.is_open()) {
        return false; // не получилось открыть поток записи
    }

    string line = "";
    int line_number = 1;

    while (getline(input, line)) {
        smatch match;
                
        if (regex_match(line, match, include_local)) {
            path include_file = in_file.parent_path() / match[1].str();
            
            if (filesystem::exists(include_file) && filesystem::is_regular_file(include_file)) {
                if (!Preprocess(include_file, out_file, include_directories)) {
                    cout << "unknown include file " << include_file << " at file " << static_cast<string>(in_file) << " at line " << line_number << endl;
                    return false;
                }
            } else {
                if (!CheckGlobal(include_directories, in_file, out_file, line_number, match)) {
                    return false;
                }
            }
        } else if (regex_match(line, match, include_global)) {
            if (!CheckGlobal(include_directories, in_file, out_file, line_number, match)) {
                return false;
            }
        } else {
            output << line << endl;
        }
        
        line_number++;
    }
    
    return true;
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
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