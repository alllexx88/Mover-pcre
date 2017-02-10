/***********************************************************************
    Copyright (C) 2017 Alex Potapenko

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

***********************************************************************/

#define UNICODE
#define _UNICODE

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <list>
#include <iterator>
#include <pcrscpp.h>
#include <windows.h>
#include <boost/progress.hpp>

#if __cplusplus >= 201103L
// at least C++11
// for utf16<->utf8 conversion:
# include <locale>
# include <codecvt>
#else
// older than C++11
# define nullptr NULL
// will use MultiByteToWideChar()
#endif

#if __cplusplus < 201103L
// older than C++11
template <typename T>
std::string my_to_string (T a) {
    std::ostringstream oss;
    oss << a;
    return oss.str();
}
#else
#define my_to_string std::to_string
#endif

#define EXT_PATH_PREFIX L"\\\\?\\" // "\\?\" prefix for an extended-length path

std::wstring utf8_to_wchar(const std::string& str);
std::wstring utf8_to_wchar(const char* str);
std::string wchar_to_utf8(const std::wstring& wstr);
std::wstring last_system_error();
bool file_exists (const std::wstring& file);
bool file_exists (const std::string& file);
bool file_is_dir (const std::wstring& file);
std::wstring remove_path_prefix (const std::wstring& path);
std::wstring sanitize_path (const std::wstring& path);
std::wstring get_full_path (const std::wstring& relpath);
bool is_root_drive (const std::wstring& path);
std::wstring get_parent_dir (const std::wstring& path);
bool mkdirp (const std::wstring& dir);
bool intel_move (const std::wstring& source, const std::wstring& target);
template<typename charT>
std::list<std::basic_string<charT> > explode (const std::basic_string<charT>& str,
                                   const charT& delim);
std::list<std::wstring> files_to_move_from_ifs (const std::ifstream& file);
void wait();


std::wstring error (L"");     // store last error message here

#if __cplusplus >= 201103L
//transform input UTF8 std::string into Windows Unicode std::wstring
// using std::std::wstring_convert()
// (widestring on Windows is UTF16)
std::wstring utf8_to_wchar(const std::string& str) {
    static std::wstring_convert<std::codecvt_utf8<char16_t>,
                  char16_t> conv;
    std::u16string buff = conv.from_bytes(str);

    return std::wstring().assign(buff.begin(),buff.end());
}

// same, but take C string as input
std::wstring utf8_to_wchar(const char* str) {
    return utf8_to_wchar(std::string(str));
}
#else
//transform input UTF8 std::string into Windows Unicode std::wstring:
// wrapper around MultiByteToWideChar
std::wstring utf8_to_wchar(const std::string& str) {
    std::wstring converted_string (L"");
    int required_size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, 0, 0);
    if (required_size > 0) {
        converted_string.resize(required_size, L' ');
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.size(), &converted_string[0], required_size);
    }

    return converted_string;
}

// same, but take C string as input
std::wstring utf8_to_wchar(const char* str) {
    return utf8_to_wchar (std::string(str));
}
#endif

#if __cplusplus >= 201103L
//transform input Windows Unicode std::wstring into Utf8 std::string
// using std::std::wstring_convert()
// (widestring on Windows is UTF16)
std::string wchar_to_utf8(const std::wstring& wstr) {
    static std::wstring_convert<std::codecvt_utf8<char16_t>,
                  char16_t> conv;

    return conv.to_bytes(std::u16string().assign(wstr.begin(),wstr.end()));
}
#else
// transform input Windows Unicode std::wstring into Utf8 std::string:
// wrapper around MultiByteToWideChar
std::string wchar_to_utf8(const std::wstring& wstr) {
    std::string converted_string ("");
    int required_size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, 0, 0, 0, 0);
    if(required_size > 0) {
        converted_string.resize(required_size, ' ');
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &converted_string[0], required_size, 0, 0);
    }

    return converted_string;
}
#endif

// wrapper around GetLastError and FormatMessageW
std::wstring last_system_error() {
    wchar_t* buffer;
    DWORD dw = GetLastError();

    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
#ifdef __MINGW32__
        (LPWSTR)&buffer, // MinGW bug: should be LPTWSTR -- pointer to LPWSTR
#else
        &buffer,
#endif
        0, nullptr);

    std::wstring message (buffer);
    LocalFree (buffer);

    return message;
}

// check if file exists
bool file_exists (const std::wstring& file) {
    return (_waccess(file.c_str(), 0) == 0);
}

bool file_exists (const std::string& file) {
    return file_exists(utf8_to_wchar(file));
}

// check if file is a dir
bool file_is_dir (const std::wstring& file) {
    if (!file_exists(file))
        return false;

    DWORD attr = GetFileAttributesW(file.c_str());

    return (bool)(attr & FILE_ATTRIBUTE_DIRECTORY);
}

// remove "\\?\" prefix if present
std::wstring remove_path_prefix (const std::wstring& path){
    std::wstring stripped_path;

    if (path.compare(0,((std::wstring)EXT_PATH_PREFIX).size(),
                              EXT_PATH_PREFIX) == 0)
        stripped_path.assign(path.begin() + ((std::wstring)EXT_PATH_PREFIX).size(),
                             path.end());
    else
        stripped_path = path;

    return stripped_path;
}

// sanitize path: use single "\" as path separators,
// remove trailing "\"
std::wstring sanitize_path (const std::wstring& path) {
    bool ext_path_prefix_removed = false;

    // get rid of "\\?\" prefix if present
    std::wstring sanitized_path = remove_path_prefix (path);
    if (sanitized_path.compare(path) != 0)
        ext_path_prefix_removed = true;

    // replace all present double/triple/etc slashes with single ones
    {
        std::wstring tmp;
        tmp.reserve(sanitized_path.size());
        for (std::wstring::iterator it=sanitized_path.begin();
             it!=sanitized_path.end(); ++it)
            if ((*it != L'/') && (*it != L'\\'))
                tmp.push_back(*it);
            else if ((tmp.empty()) || (*(tmp.end() - 1) != L'\\'))
                tmp.push_back (L'\\');

        // if not "\" -- delete trailing slash
        if ((tmp.size() > 1) && (*(tmp.end() - 1) == L'\\'))
            tmp.resize(tmp.size() - 1);

        sanitized_path = tmp;
    }

    if (ext_path_prefix_removed)
            sanitized_path.insert(0, EXT_PATH_PREFIX);

    return sanitized_path;
}

// get full path from relative,
// wrapper around GetFullPathNameW,
// allowing "\\?\" prefix for relative paths,
// (unlike WinAPI does)
std::wstring get_full_path (const std::wstring& relpath) {
    std::wstring abs_path (L"");
    bool ext_path_prefix_removed = false;

    // get rid of "\\?\" prefix if present
    std::wstring relpathname = remove_path_prefix (relpath);
    if (relpathname.compare(relpath) != 0)
        ext_path_prefix_removed = true;

    // sanitize path:
    // replace all present double/triple/etc slashes with single ones
    // and remove trailing slash if present
    relpathname = sanitize_path (relpathname);

    // now we have a "decent" relative path
    // first find out how required buffer length
    int required_size = GetFullPathNameW(relpathname.c_str(), 0, nullptr, nullptr);
    if (required_size > 0) {
        std::vector<wchar_t> buffer(required_size);
        // now store absolute path value in the buffer,
        // unless error occurred and (required_size == 0)
        GetFullPathNameW(relpathname.c_str(), required_size, &buffer[0], nullptr);
        // copy buffer to abs_path std::wstring
        abs_path.assign(buffer.begin(), buffer.end() - 1);

        // if we removed "\\?\", bring it back
        if (ext_path_prefix_removed)
            abs_path.insert(0, EXT_PATH_PREFIX);
    }

    return abs_path;
}

bool is_root_drive (const std::wstring& path) {
    std::wstring stripped_path = sanitize_path(remove_path_prefix(path));
    // now it is "[A-Za-z]:", if given path is a root drive path

    return ((stripped_path.length() == 2) &&
            (stripped_path[1] == L':') &&
            (toupper(stripped_path[0]) >= L'A') &&
            (toupper(stripped_path[0]) <= L'Z'));
}

std::wstring get_parent_dir (const std::wstring& path) {
    // first get proper full path
    std::wstring abs_path = get_full_path (path);


    if ((abs_path.empty()) ||       // if get_full_path () failed
        (is_root_drive (abs_path))) // don't go up from a root drive,
        return L"";
    else {
        size_t found = abs_path.find_last_of(L'\\');
        return abs_path.substr(0,found);
    }
}

// make dir recursively, similarly to unix `mkdir -p`
bool mkdirp (const std::wstring& dir) {
    std::wstring abs_path = get_full_path(dir);

    if (abs_path.empty()) {
        ::error = (std::wstring)L"Failed to get absolute path for \"" + dir + L"\": "
                + last_system_error();
        return false;
    }

    if (file_is_dir(abs_path))
        return true;

    if (file_exists(abs_path)) {
        ::error = (std::wstring)L"Cannot create folder \"" + dir + L"\": "
                + L"file of the same name exists, but is not a folder";
        return false;
    }

    if (is_root_drive (abs_path)) {
        ::error = (std::wstring)L"Cannot create a new drive \"" + abs_path + L"\"";
        return false;
    }

    // here're we're already sure abs_path isn't a root drive,
    // so we can go up a level
    std::wstring parent_path = get_parent_dir (abs_path);

    if (!mkdirp(parent_path))
        return false;

    if (file_is_dir(abs_path)) // make double sure directory doesn't exist already
        return true;

    if (!CreateDirectoryW (abs_path.c_str(), nullptr)) {
        ::error = (std::wstring)L"Failed to create folder \"" + dir + L"\": "
                + last_system_error();
        return false;
    }

    return true;
}

bool intel_move (const std::wstring& source, const std::wstring& target) {
    std::wstring source_abs = get_full_path(source),
        target_abs = get_full_path(target);

    if (source_abs.empty()) {
        ::error = (std::wstring)L"Failed to get absolute path for \"" + source + L"\": "
                + last_system_error();
        return false;
    }

    if (target_abs.empty()) {
        ::error = (std::wstring)L"Failed to get absolute path for \"" + target + L"\": "
                + last_system_error();
        return false;
    }

    if (!file_exists(source_abs)) {
        ::error = (std::wstring)L"File \"" + source_abs + L"\" does not exist\nsource: \""
                + source + L"\"";
        return false;
    }

    std::wstring target_path = get_parent_dir (target_abs);

    if (!mkdirp(target_path))
        return false;

    if (!MoveFileW(source_abs.c_str(), target_abs.c_str())) {
        ::error = last_system_error();
        return false;
    }

    return true;
}

// split std::basic_string by delimiter and store to list
template<typename charT>
std::list<std::basic_string<charT> > explode (const std::basic_string<charT>& str,
                                   const charT& delim){
    std::basic_istringstream<charT> iss (str);
    std::basic_string<charT> line;
    std::list<std::basic_string<charT> > out;

    while (getline(iss, line, delim)) {
        if (!line.empty())
            out.push_back(line);
    }

    return out;
}

#define IFS "[ \\t]" // internal fields separator: space/tab
#define IFS0 IFS "*" // any number of IFS symbols
#define IFS1 IFS "+" // 1+ of IFS symbols

// get source-target files list:
// odd entries are source files, even -- target files
std::list<std::wstring> files_to_move_from_ifs (std::ifstream& file) {
    std::wistringstream ifs_content;
    // regex to extract source and target files from "mv" commands
    // of the expected syntax.
    // Note that "D" (discard) flag in combination with "m" (multiline)
    // strips off all newlines, so we add extra newline
    // at the end
    pcrscpp::replace
         move_rx("^"   // line start
                 IFS0               // ignore IFS at the beginning
                 "mv"               // expected "mv" command
                 IFS1               // 1+ IFS
                 "(?:-f)?"          // allow/ignore "-f" (force) move flag here
                 IFS1               // 1+ IFS
                 "\"(.+)\""         // source file to move: "$1" backreference
                 IFS1               // 1+ IFS
                 "\"(.+)\""         // target file to move: "$2" backreference
                 IFS0               // ignore IFS at the end
                 "$", // line end
                "$1\\n$2\\n",
                "nmg");             // discard non-matching, multiline, global,
                                    // Unicode mode
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0);
    std::list<std::wstring> src_trgt_list;
    if (size > 0) {
        std::string buffer;
        buffer.resize(size);
        file.read(&buffer[0], size);
        buffer.resize(size - 1); // strip '\0'
        move_rx.replace_inplace(buffer); // transform to "<src>\n<trgt>\n" entries
        if (!buffer.empty())
            buffer.resize(buffer.size() - 1); // Remove last "\n"

        src_trgt_list = explode(utf8_to_wchar(buffer), L'\n');
    }

    ifs_content.seekg(0);

    return src_trgt_list;
}

void wait() {
    DWORD count, mode;
    char ch;
    HANDLE hstdin = GetStdHandle (STD_INPUT_HANDLE);

    // Switch to raw mode
    GetConsoleMode (hstdin, &mode);
    SetConsoleMode (hstdin, 0);

    // Wait for the user's response
    WaitForSingleObject(hstdin, INFINITE);

    // Read the (single) key pressed
    ReadConsoleA (hstdin, &ch, 1, &count, nullptr);

    // Restore the console to its previous state
    SetConsoleMode (hstdin, mode);
}

int main () {
    std::wstring linew, source, target, dir;
    std::string line;
    bool error_log_writeable = true,
         errors = false;
    std::wstring error_log (L""); // accumulate error log here,
                             // to print error report to stdout,
                             // in case we can't write to a log file
    const std::string script_file_name ("sm_move.sh");
    std::ifstream move_script_ifs (script_file_name.c_str());

    if (!move_script_ifs.is_open ()) {
        std::cerr << "Unable to open sm_move.sh";
        return 1;
    }

    std::string error_log_filename ("Mover_error");

    if (file_exists(error_log_filename + ".log")) {
        unsigned i = 0;
        for (; file_exists(error_log_filename + my_to_string(i)  + ".log"); i++);
             error_log_filename.append(my_to_string(i));
    }

    error_log_filename.append(".log");

    std::ofstream error_log_stream (error_log_filename.c_str());

    if (!error_log_stream.is_open()){
        error_log_writeable = false;
        std::cout << "Warning: cannot create a log file:" << std::endl
             << "In case of error(s) report will be printed" << std::endl
             << "to stdout on execution end" << std::endl;
    }

    std::cout << "Running regular expression on "
         << script_file_name  << std::endl << "to extract "
         << "source and target file names ... ";

    std::list<std::wstring> src_trgt_files = files_to_move_from_ifs (move_script_ifs);

    move_script_ifs.close();

    std::cout << "Done" << std::endl;
 /*   std::cout << "Running away to debug" << std::endl;
    return 0;*/

    size_t size = src_trgt_files.size() / 2;

    if (size == 0) {
        std::cout << "Couldn't load any source&target files: "
             << " check if " << script_file_name
             << " is right" << std::endl;
    } else {
        std::cout << "Script file \"" << script_file_name << "\" loaded: "
             << std::endl << "Attempting to move " << size << " files"
             << std::endl;

        boost::progress_display show_progress(size);

        for (std::list<std::wstring>::const_iterator
             iterator = src_trgt_files.begin(),
             end = src_trgt_files.end();
             iterator != end;
             ++iterator) {

            std::wstring source = *iterator,
                    target = *(++iterator);

            // replace all multiple 1+ slashes with "\" and add "\\?\" prefix
            source = (sanitize_path (source)).insert(0, EXT_PATH_PREFIX);
            target = (sanitize_path (target)).insert(0, EXT_PATH_PREFIX);

            if (!intel_move(source, target)) {
                errors = true;
                std::wstring message =
                        (std::wstring)L"Failed to move"
                                 L"\"" + source + L"\""
                                 L" to "
                                 L"\"" + target + L"\""
                                 L":\n" +
                                 ::error + L"\n";
                if (error_log_writeable)
                    error_log_stream << wchar_to_utf8(message);
                else
                    error_log.append(message);
            }

            ++show_progress;
        }

        std::cout << "Done." << std::endl;

        if (!errors) {
            std::cout << "All files successfully moved." << std::endl;
            if (error_log_writeable) {
                error_log_stream.close();
                remove (error_log_filename.c_str());
            }
        }
        else {
            if (error_log_writeable) {
                std::cout << "There were some errors: "
                     "see " << error_log_filename << " "
                     "for details" << std::endl;
            } else
            std::wcout << error_log;
        }
    }

    std::cout << "Press any key to exit...";

    wait();

    return 0;
}
