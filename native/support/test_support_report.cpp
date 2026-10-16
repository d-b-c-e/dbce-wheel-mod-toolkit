// SPDX-License-Identifier: MIT
// Behavioral port of tests/SupportReport/Program.cs; extra native IO/UTF cases.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "support_report.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace dbce::support;
namespace fs=std::filesystem;
namespace {
int checks=0;
void check(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); ++checks; }
template<class F> void refuses(F action) {
    try { action(); } catch (const std::exception&) { ++checks; return; }
    throw std::runtime_error("Expected refusal");
}
void write(const fs::path& path,const std::string& bytes) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc);
    file.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    if (!file) throw std::runtime_error("Fixture write failed");
}
std::string read(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);
    if (!file) throw std::runtime_error("Fixture read failed");
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
std::string u16(const std::wstring& text,bool be=false) {
    std::string result=be ? std::string("\xfe\xff",2) : std::string("\xff\xfe",2);
    for (auto c:text) {
        auto lo=static_cast<char>(c&255),hi=static_cast<char>((c>>8)&255);
        result += be ? hi:lo; result += be ? lo:hi;
    }
    return result;
}
bool contains(const std::string& text,const std::string& value) { return text.find(value)!=std::string::npos; }
void run(const fs::path& root) {
    auto redacted=redact("C:\\Users\\fixture\\game\\log.txt\nuser@example.com\nAuthorization: Bearer abc\n"
                         "{\"password\":\"hidden\"}\nghp_abcdefghijklmnopqrstuv","C:\\Users\\fixture");
    for (const auto& secret:{"fixture","user@example.com","Bearer abc","hidden","ghp_"})
        check(!contains(redacted,secret),"Existing redaction case");
    check(contains(redacted,"%USERPROFILE%") && contains(redacted,"game\\log.txt"),"Useful path suffix");
    check(valid_repository("owner/repo-name"),"Valid repository");
    for (const auto& bad:{"https://github.com/owner/repo","owner/..","owner/repo\n","owner/repo?x=1","owner/repo/extra",
                         "owner/.repo","owner/-repo","/repo","owner/","ow ner/repo"})
        check(!valid_repository(bad),"Routing injection refused");
    refuses([] { ReportProduct p("Game","1","owner/repo",std::nullopt,"bad/url?x"); });
    refuses([] { ReportProduct p("Game","1","owner/repo\n"); });
    refuses([] { ReportProduct p(" ","1","owner/repo"); });
    auto path=root/L"mod.log";
    write(path,"earlier\nLatest steering sample\n");
    std::vector<fs::path> paths={path};
    ReportProduct product("Fixture game","1.2.3","owner/repo");
    check(product.mod_page()=="https://github.com/owner/repo" &&
          product.wheelkit_page()=="https://github.com/d-b-c-e/dbce-wheelkit","Standard Help links");
    auto prepared=prepare(product,"Steering & camera?","Steps to reproduce",paths);
    paths[0]=root/L"missing.log"; write(path,"changed after preview");
    check(contains(prepared.body(),"Latest steering sample") && !contains(prepared.body(),"changed after preview"),"Immutable snapshot");
    check(!contains(prepared.body(),"Wheelkit"),"No Wheelkit assumption");
    refuses([&] { (void)prepared.create_browser_submission(false); });
    auto browser=prepared.create_browser_submission(true);
    check(browser.url.find("https://github.com/owner/repo/issues/new?")==0,"Exact issue route");
    check(contains(browser.url,"title=Steering%20%26%20camera%3F"),"Escaped title");
    check(!browser.needs_attachment && contains(browser.url,"Latest%20steering%20sample"),"Small complete body");
    auto report=root/L"support-report.txt";
    prepared.write_review_file(report);
    check(read(report)==prepared.preview(),"Exact UTF8 snapshot export");
    refuses([&] { prepared.write_review_file(report); });
    check(read(report)==prepared.preview(),"No overwrite");
    refuses([&] { prepared.write_review_file(root/L"config.json"); });
    refuses([&] { prepared.write_review_file(root/L"NUL.txt"); });
    refuses([&] { prepared.write_review_file(root/L"missing"/L"report.txt"); });
    check(!fs::exists(root/L"missing"),"No directories created");
    refuses([] { (void)selected_logs({}); });
    refuses([&] { (void)selected_logs({path,path}); });
    refuses([&] { (void)selected_logs({path,path,path,path,path,path}); });
    refuses([&] { (void)selected_logs({root/L"save.exe"}); });
    refuses([&] { (void)prepare(product,"Title","Description",{root/L"missing.log"}); });
    auto unity=root/L"unity.log";
    write(unity,u16(std::wstring(5000,L'x')+L"\nLatest UTF16 sample\n"));
    auto tail=log_tail(unity,4800);
    check(contains(tail,"Latest UTF16 sample") && !contains(tail,"xxxxx") && tail.find('\0')==std::string::npos,"Aligned UTF16 LE tail");
    write(unity,u16(std::wstring(5000,L'x')+L"\nLatest BE sample\n",true));
    check(contains(log_tail(unity,4800),"Latest BE sample"),"UTF16 BE tail");
    std::string many; for (int i=0;i<5000;++i) many += "\xc3\xa9";
    write(path,many+"\nLatest UTF8 sample\n");
    check(contains(log_tail(path,257),"Latest UTF8 sample"),"Split UTF8 discarded before decoding");
    write(path,std::string(10000,'s'));
    check(!contains(log_tail(path,256),"ssss"),"Partial credential line omitted");
    for (const auto& bytes: {std::string("\0\1\2\3",4),std::string("\xff\xff\x61",3),
            std::string("\xff\xfe\0\0",4),std::string("\0\0\xfe\xff",4),std::string("\xef\xbb\xbf\xc0\xaf",5),
            u16(std::wstring(1,static_cast<wchar_t>(0xd800))),std::string("\xff\xfe\x61",3)}) {
        write(path,bytes); refuses([&] { (void)log_tail(path); });
    }
    write(path,"\xef\xbb\xbfUTF8 BOM\n"); check(log_tail(path)=="UTF8 BOM\n","UTF8 BOM removed");
    write(path,u16(L"\xd83d\xde80 launch\n")); check(contains(log_tail(path),"\xf0\x9f\x9a\x80"),"UTF16 surrogate pair");
    write(path,"line\xc2\x85" "control"); refuses([&] { (void)log_tail(path); });
    refuses([&] { (void)log_tail(path,255); }); refuses([&] { (void)log_tail(path,24001); });
    for (const auto& unsafe:{L"\\\\server\\share\\mod.log",L"//server/share/mod.log",L"\\/server/share/mod.log",L"\\\\?\\C:\\mod.log",L"C:\\file.log:extra.txt"})
        refuses([&] { (void)log_tail(unsafe); });
    refuses([&] { prepared.write_review_file(L"\\\\server\\share\\report.txt"); });
    refuses([&] { prepared.write_review_file(root/L"support-report.txt:extra.txt"); });
    auto large=prepare(product,"Camera issue","Camera loses center after restart. "+std::string(9000,'d'));
    auto fallback=large.create_browser_submission(true);
    check(fallback.needs_attachment && fallback.url.size()<=browser_url_limit,"Bounded fallback");
    for (const auto& fragment:{"Camera loses center","Fixture game","1.2.3","support-report.txt"})
        check(contains(fallback.issue_body,fragment),"Useful fallback summary");
    check(contains(fallback.instructions,"uploads the file immediately"),"Honest attachment timing");
    check(contains(large.body(),std::string(9000,'d')),"Full local report retained");
    auto secret=prepare(product,"Issue","password="+std::string(1000,'a'));
    check(!contains(secret.body(),"aaaa"),"Redact before summary truncation");
    std::vector<fs::path> five;
    for (int i=0;i<5;++i) { five.push_back(root/("log-"+std::to_string(i)+".log")); write(five.back(),std::string(25000,'x')+"\nLatest "+std::to_string(i)+"\n"); }
    auto selected=selected_logs(five);
    for (int i=0;i<5;++i) check(contains(selected,"Latest "+std::to_string(i)),"Each log gets bounded share");
    check(selected.size()<25000,"Shared byte budget");
    check(!contains(selected,root.u8string()),"Basenames only");
    write(path,"```<private>\n");
    auto safe=prepare(product,"% & # ?","Escaped",{path});
    check(contains(safe.body(),"[code fence]&lt;private&gt;"),"Log markup neutralized");
    check(contains(safe.create_browser_submission(true).url,"title=%25%20%26%20%23%20%3F"),"All query punctuation escaped");
    auto unicode=prepare(product,u8"Café 🚀",u8"First\nSecond");
    check(contains(unicode.create_browser_submission(true).url,"Caf%C3%A9%20%F0%9F%9A%80"),"UTF8 URL escaping");
    check(!contains(redact("api_key=hidden\naccess-token: other\nsecret='private'"),"hidden"),"Credential variants");
    check(!contains(redact("GITHUB_PAT_abcdef_123 GHO_abcdefghijklmnop"),"abcdef"),"Case-insensitive tokens");
    check(!contains(redact("first@bad.second@example.com"),"second@example.com"),"Embedded valid email remains redacted");
    check(redact(std::string(20000,'a')+".")==std::string(20000,'a')+".","Long non-email remains bounded");
    auto linked=root/L"linked.log";
    if (CreateSymbolicLinkW(linked.c_str(),path.c_str(),2)) {
        refuses([&] { (void)log_tail(linked); });
        refuses([&] { (void)selected_logs({linked}); });
        fs::remove(linked);
    } else std::cout << "SKIP symlink creation unavailable (" << GetLastError() << ")\n";
}
}
int main() {
    // Create only our fresh inert fixture directory. Cleanup never leaves it.
    auto root=fs::temp_directory_path()/("dbce-native-support-fixture-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    try {
        if (!fs::create_directory(root)) throw std::runtime_error("Fresh fixture required");
        run(root); fs::remove_all(root);
        std::cout << "PASS native support report: " << checks << " checks\n"; return 0;
    } catch(const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << " (inert fixture retained at " << root.u8string() << ")\n"; return 1;
    }
}
