#include "native_agent_hooks.h"
#include "native_text.h"
#include <shellapi.h>
#include <behaviortree_cpp/contrib/json.hpp>
#include <toml++/toml.hpp>
#include <algorithm>
#include <array>
#include <fstream>
#include <optional>
#include <sstream>
#include <vector>

namespace NativeAgentHooks {
namespace {
using Path=std::filesystem::path;
using Json=nlohmann::json;
constexpr std::array events{"UserPromptSubmit","Stop","Interrupt"};
constexpr std::array timeouts{5,5,3};

Path environment(const wchar_t* name){
    std::wstring value(32768,L'\0');
    const DWORD length=GetEnvironmentVariableW(name,value.data(),DWORD(value.size()));
    if(length>=value.size())throw std::runtime_error("Agent 配置目录路径过长。");
    value.resize(length);
    return Path(value);
}

std::optional<std::string> read(const Path& file){
    if(!std::filesystem::exists(file))return {};
    std::ifstream stream(file,std::ios::binary);
    if(!stream)throw std::runtime_error("无法读取 Agent 配置文件。");
    std::ostringstream text;
    text<<stream.rdbuf();
    if(stream.bad()||text.bad())throw std::runtime_error("无法读取完整的 Agent 配置文件。");
    return text.str();
}

void save(const Path& file,const std::optional<std::string>& original,const std::string& next){
    if(original&&next==*original)return;
    const auto folder=file.parent_path();
    std::filesystem::create_directories(folder);
    wchar_t name[MAX_PATH]{};
    if(!GetTempFileNameW(folder.c_str(),L"bch",0,name))
        throw std::runtime_error("无法在 Agent 配置目录中创建文件。");
    struct Temporary {
        const wchar_t* name;
        ~Temporary(){DeleteFileW(name);}
    } temporary{name};
    {
        std::ofstream stream(Path(name),std::ios::binary|std::ios::trunc);
        stream.write(next.data(),std::streamsize(next.size()));
        stream.close();
        if(!stream)throw std::runtime_error("无法写入 Agent 配置文件。");
    }
    if(read(file)!=original)throw std::runtime_error("Agent 配置已被其他程序更新，请重新连接。");
    const DWORD flags=MOVEFILE_WRITE_THROUGH|(original?MOVEFILE_REPLACE_EXISTING:0);
    if(!MoveFileExW(name,file.c_str(),flags))throw std::runtime_error("无法保存 Agent 配置文件，请检查目录权限。");
}

bool same(std::wstring_view a,std::wstring_view b){
    return CompareStringOrdinal(a.data(),int(a.size()),b.data(),int(b.size()),TRUE)==CSTR_EQUAL;
}

Path hookExecutable(const std::string& command){
    if(command.empty())return {};
    int count=0;
    auto args=CommandLineToArgvW(wide(command).c_str(),&count);
    if(!args)return {};
    Path file;
    if(count==2&&std::wstring_view(args[1])==L"--codex-hook")file=args[0];
    LocalFree(args);
    if(!file.is_absolute())return {};
    return file.lexically_normal().make_preferred();
}

bool owned(const Path& hook,const Path& executable){
    if(hook.empty())return false;
    const auto stem=hook.stem().wstring();
    constexpr std::wstring_view prefix=L"BA-Cafe-Pets-";
    return same(hook.filename().native(),executable.filename().native())||
        (same(hook.extension().native(),L".exe")&&
         (same(stem,L"BA-Cafe-Pets")||(stem.size()>prefix.size()&&same(std::wstring_view(stem).substr(0,prefix.size()),prefix))));
}

std::string commandFor(const Path& executable){
    const auto& path=executable.native();
    if(path.find_first_of(L"%!\r\n")!=std::wstring::npos)
        throw std::runtime_error("请将桌宠移动到路径不含 % 或 ! 的文件夹后重新连接。");
    const auto quoted=path.find_first_of(L" \t&()^")!=std::wstring::npos?L"\""+path+L"\"":path;
    return utf8(quoted+L" --codex-hook");
}

// TOML columns count Unicode code points. Edit only the existing string literal.
size_t offset(const std::string& text,toml::source_position where){
    size_t at=text.starts_with("\xef\xbb\xbf")?3:0;
    for(toml::source_index line=1;line<where.line;++line)at=text.find('\n',at)+1;
    for(toml::source_index column=1;column<where.column;++column){
        ++at;
        while(at<text.size()&&(static_cast<unsigned char>(text[at])&0xc0)==0x80)++at;
    }
    return at;
}

struct InlineHooks {
    std::string text;
    std::array<bool,events.size()> installed{};
    bool configured=false;
};

InlineHooks updateInline(const std::string& original,const Path& executable,const std::string& command){
    const auto config=toml::parse(original);
    InlineHooks result{original};
    if(const auto* hooks=config["hooks"].as_table()){
        for(const auto& [name,group]:*hooks)
            if(name.str()!="state"&&group.is_array())result.configured=true;
    }
    std::vector<toml::source_region> edits;
    for(size_t i=0;i<events.size();++i){
        const auto* groups=config["hooks"][events[i]].as_array();
        if(!groups)continue;
        for(const auto& group:*groups){
            const auto* handlers=toml::node_view<const toml::node>(group)["hooks"].as_array();
            if(!handlers)continue;
            for(const auto& handler:*handlers){
                const auto config=toml::node_view<const toml::node>(handler);
                if(config["type"].value_or(std::string_view{})!="command")continue;
                auto value=config["command_windows"];
                if(!value)value=config["commandWindows"];
                if(!value)value=config["command"];
                const auto file=hookExecutable(value.value_or(std::string{}));
                if(!owned(file,executable))continue;
                result.installed[i]=true;
                if(!same(file.native(),executable.native()))edits.push_back(value.node()->source());
            }
        }
    }
    if(edits.empty())return result;
    std::ranges::sort(edits,[](const auto& a,const auto& b){return a.begin>b.begin;});
    std::ostringstream literal;
    literal<<toml::value<std::string>(command);
    for(const auto& region:edits){
        const auto begin=offset(original,region.begin),end=offset(original,region.end);
        result.text.replace(begin,end-begin,literal.str());
    }
    static_cast<void>(toml::parse(result.text));
    return result;
}

void addInline(std::string& text,size_t i,const std::string& command){
    std::ostringstream block;
    block<<"\n[[hooks."<<events[i]<<"]]\n[[hooks."<<events[i]<<".hooks]]\n"
        <<"type = \"command\"\ncommand = "<<toml::value<std::string>(command)<<"\ntimeout = "<<timeouts[i]<<"\n";
    text+=block.str();
}

bool mergeJson(Json& document,const InlineHooks& inlineHooks,const Path& executable,const std::string& command){
    if(!document.is_object()||(document.contains("hooks")&&!document["hooks"].is_object()))
        throw std::runtime_error("hooks.json 中的 hooks 必须是对象。");
    bool changed=false;
    for(size_t i=0;i<events.size();++i){
        auto& hooks=document["hooks"];
        const auto entry=hooks.find(events[i]);
        if(inlineHooks.installed[i]&&entry==hooks.end())continue;
        if(hooks.is_null())hooks=Json::object();
        auto& groups=hooks[events[i]];
        if(groups.is_null())groups=Json::array();
        if(!groups.is_array())throw std::runtime_error("hooks.json 中的事件配置必须是数组。");
        bool installed=inlineHooks.installed[i];
        for(auto group=groups.begin();group!=groups.end();){
            if(!group->is_object()||!group->contains("hooks")||!(*group)["hooks"].is_array())
                throw std::runtime_error("hooks.json 中的命令组格式不正确。");
            auto& handlers=(*group)["hooks"];
            bool removed=false;
            for(auto h=handlers.begin();h!=handlers.end();){
                if(h->is_object()&&h->value("type",std::string{})=="command"){
                    const auto key=h->contains("commandWindows")?"commandWindows":"command";
                    const auto file=hookExecutable(h->value(key,std::string{}));
                    if(owned(file,executable)){
                        if(installed){h=handlers.erase(h);changed=removed=true;continue;}
                        installed=true;
                        if(!same(file.native(),executable.native())){(*h)[key]=command;changed=true;}
                    }
                }
                ++h;
            }
            if(removed&&handlers.empty())group=groups.erase(group);
            else ++group;
        }
        if(!installed){
            const Json handler={{"type","command"},{"command",command},{"timeout",timeouts[i]}};
            groups.push_back({{"hooks",Json::array({handler})}});
            changed=true;
        }
    }
    return changed;
}

std::string powerShellArgument(const Path& executable){
    std::string quoted="'";
    for(char c:utf8(executable.native())){if(c=='\'')quoted+='\'';quoted+=c;}
    return quoted+'\'';
}

constexpr std::string_view cursorPrefix="& { $OutputEncoding = [System.Text.UTF8Encoding]::new($false); "
    "$lines = @($input); if ($lines.Count -eq 0) { $lines = "
    "[IO.StreamReader]::new([Console]::OpenStandardInput(), [Text.Encoding]::UTF8).ReadToEnd() }; $lines | & ";
constexpr std::string_view cursorSuffix=" --agent-hook cursor }";

bool ownsAgentHook(const Json& hook,const Path& executable,Preset preset){
    if(!hook.is_object())return false;
    const auto command=hook.find("command");
    if(command==hook.end()||!command->is_string())return false;
    const auto& value=command->get_ref<const std::string&>();
    if(preset==Preset::ClaudeCode){
        const auto args=hook.find("args");
        return hook.value("type",std::string{})=="command"&&args!=hook.end()&&
            *args==Json::array({"--agent-hook","claude-code"})&&owned(Path(wide(value)),executable);
    }
    if(value.size()<cursorPrefix.size()+cursorSuffix.size()+2||
        !value.starts_with(cursorPrefix)||!value.ends_with(cursorSuffix))return false;
    const auto quoted=std::string_view(value).substr(cursorPrefix.size(),value.size()-cursorPrefix.size()-cursorSuffix.size());
    if(quoted.front()!='\''||quoted.back()!='\'')return false;
    std::string path(quoted.substr(1,quoted.size()-2));
    for(size_t at=0;(at=path.find("''",at))!=path.npos;++at)path.erase(at,1);
    return owned(Path(wide(path)),executable);
}

void connectAgent(const Path& directory,const Path& executable,Preset preset){
    const bool cursor=preset==Preset::Cursor;
    const auto file=directory/(cursor?L"hooks.json":L"settings.json");
    const auto original=read(file);
    auto document=original?Json::parse(*original,nullptr,false):Json::object();
    if(!document.is_object()||(document.contains("hooks")&&!document["hooks"].is_object()))
        throw std::runtime_error("Agent 配置格式有误，请检查原配置后重新连接。");
    const auto before=document;
    if(cursor&&!document.contains("version"))document["version"]=1;
    const Json hook=cursor?Json{{"command",std::string(cursorPrefix)+powerShellArgument(executable)+std::string(cursorSuffix)},
        {"timeout",5}}:
        Json{{"type","command"},{"command",utf8(executable.native())},
            {"args",Json::array({"--agent-hook","claude-code"})},{"timeout",5}};
    const auto names=cursor?std::array{"beforeSubmitPrompt","afterAgentResponse","stop"}:
        std::array{"UserPromptSubmit","Stop","StopFailure"};
    for(const char* name:names){
        auto& groups=document["hooks"][name];
        if(groups.is_null())groups=Json::array();
        if(!groups.is_array())throw std::runtime_error("Agent Hook 的事件配置必须是数组。");
        bool installed=false;
        const auto assign=[&](Json& handlers){
            for(auto it=handlers.begin();it!=handlers.end();){
                if(!ownsAgentHook(*it,executable,preset)){++it;continue;}
                if(installed){it=handlers.erase(it);continue;}
                it->update(hook);
                installed=true;
                ++it;
            }
        };
        if(cursor)assign(groups);
        else for(auto it=groups.begin();it!=groups.end();){
            if(!it->is_object()||!it->contains("hooks")||!it->at("hooks").is_array())
                throw std::runtime_error("Claude Code Hook 的命令组格式不正确。");
            auto& handlers=it->at("hooks");
            const auto count=handlers.size();
            assign(handlers);
            if(count&&handlers.empty())it=groups.erase(it);
            else ++it;
        }
        if(!installed)groups.push_back(cursor?hook:Json{{"hooks",Json::array({hook})}});
    }
    if(document!=before)save(file,original,document.dump(2)+'\n');
}
}

Path home(Preset preset){
    const auto custom=preset==Preset::Cursor?Path{}:environment(preset==Preset::Codex?L"CODEX_HOME":L"CLAUDE_CONFIG_DIR");
    if(!custom.empty())return std::filesystem::absolute(custom);
    const auto profile=environment(L"USERPROFILE");
    if(profile.empty())throw std::runtime_error("无法定位当前用户的 Agent 配置目录。");
    return profile/(preset==Preset::Codex?L".codex":preset==Preset::ClaudeCode?L".claude":L".cursor");
}

std::wstring notificationExample(const Path& executable){
    return L"$OutputEncoding = [System.Text.UTF8Encoding]::new($false)\r\n"
        L"@{source='自定义'; run_id='test'; phase='message'; text='你好，桌宠！'} | ConvertTo-Json -Compress | & "+
        wide(powerShellArgument(executable))+L" --notify --stdin";
}

void connect(const Path& directory,const Path& file,Preset preset){
    const auto executable=std::filesystem::absolute(file).lexically_normal().make_preferred();
    if(!std::filesystem::is_regular_file(executable))throw std::runtime_error("找不到桌宠启动程序。");
    if(preset!=Preset::Codex){connectAgent(directory,executable,preset);return;}
    const auto command=commandFor(executable);
    const auto configPath=directory/L"config.toml",hooksPath=directory/L"hooks.json";
    const auto config=read(configPath),hooks=read(hooksPath);
    InlineHooks inlineHooks;
    if(config){
        try{inlineHooks=updateInline(*config,executable,command);}
        catch(const toml::parse_error&){throw std::runtime_error("config.toml 格式有误，请修复后重新连接 Codex。");}
    }
    if(!hooks&&inlineHooks.configured){
        auto candidate=inlineHooks.text;
        for(size_t i=0;i<events.size();++i)if(!inlineHooks.installed[i])addInline(candidate,i,command);
        try{
            // Existing inline arrays cannot always be reopened as array-of-tables.
            static_cast<void>(toml::parse(candidate));
            save(configPath,config,candidate);
            return;
        }catch(const toml::parse_error&){}
    }
    auto document=Json::object();
    bool updateJson=false;
    try{
        if(hooks)document=Json::parse(*hooks);
        updateJson=mergeJson(document,inlineHooks,executable,command);
    }catch(const Json::exception&){throw std::runtime_error("hooks.json 格式有误，请修复后重新连接 Codex。");}
    if(config)save(configPath,config,inlineHooks.text);
    if(updateJson)save(hooksPath,hooks,document.dump(2)+'\n');
}
}
