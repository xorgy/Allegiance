#include "ui_script.hpp"
#include <charconv>
#include <cctype>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace Native {
namespace {
class Parser {
    std::string_view source_,file_;size_t at_{};
    using Expr=std::shared_ptr<const UiExpression>;
    [[noreturn]] void error(std::string_view message)const {
        size_t line=1;for(size_t i=0;i<at_;++i)line+=source_[i]=='\n';
        throw std::runtime_error(std::string(file_)+":"+std::to_string(line)+": "+std::string(message));
    }
    void space() {
        for(;;) {
            while(at_<source_.size() && std::isspace(static_cast<unsigned char>(source_[at_])))++at_;
            if(source_.substr(at_,2)=="//") {while(at_<source_.size() && source_[at_]!='\n')++at_;}
            else if(source_.substr(at_,2)=="/*") {auto end=source_.find("*/",at_+2);if(end==source_.npos)error("unterminated comment");at_=end+2;}
            else break;
        }
    }
    bool take(char ch){space();if(at_<source_.size() && source_[at_]==ch){++at_;return true;}return false;}
    void expect(char ch){if(!take(ch))error(std::string("expected '")+ch+"'");}
    std::string identifier() {
        space();auto begin=at_;
        if(at_==source_.size() || (!std::isalpha(static_cast<unsigned char>(source_[at_])) && source_[at_]!='_'))error("expected a name");
        while(at_<source_.size() && (std::isalnum(static_cast<unsigned char>(source_[at_])) || source_[at_]=='_'))++at_;
        return std::string(source_.substr(begin,at_-begin));
    }
    std::string string() {
        expect('"');std::string result;
        while(at_<source_.size()) {
            char ch=source_[at_++];if(ch=='"')return result;
            if(ch=='\\') {if(at_==source_.size())error("unterminated string");ch=source_[at_++];if(ch=='n')ch='\n';else if(ch=='t')ch='\t';else if(ch=='r')ch='\r';}
            result+=ch;
        }
        error("unterminated string");
    }
    Expr node(UiExpression::Kind kind,std::string name={},double number=0,std::vector<Expr> args={}) {
        return std::make_shared<UiExpression>(UiExpression{kind,std::move(name),number,std::move(args)});
    }
    std::vector<Expr> sequence(char close) {
        std::vector<Expr> result;if(take(close))return result;
        do {result.push_back(expression());if(take(close))return result;}while(take(','));
        error("expected separator or closing delimiter");
    }
    Expr atom() {
        space();if(at_==source_.size())error("expected expression");
        if(source_[at_]=='"')return node(UiExpression::Kind::String,string());
        if(take('['))return node(UiExpression::Kind::List,{},0,sequence(']'));
        if(take('(')) {auto args=sequence(')');if(args.size()==1)return args[0];return node(UiExpression::Kind::Tuple,{},0,std::move(args));}
        if(take('-'))return node(UiExpression::Kind::Operator,"negate",0,{atom()});
        if(std::isdigit(static_cast<unsigned char>(source_[at_])) || source_[at_]=='.') {
            if(source_.substr(at_,2)=="0x" || source_.substr(at_,2)=="0X") {
                at_+=2;uint64_t value{};auto first=source_.data()+at_;auto result=std::from_chars(first,source_.data()+source_.size(),value,16);
                if(result.ec!=std::errc{} || result.ptr==first)error("invalid hexadecimal number");at_=size_t(result.ptr-source_.data());return node(UiExpression::Kind::Number,{},double(value));
            }
            double value{};auto first=source_.data()+at_;auto result=std::from_chars(first,source_.data()+source_.size(),value);
            if(result.ec!=std::errc{})error("invalid number");at_=size_t(result.ptr-source_.data());return node(UiExpression::Kind::Number,{},value);
        }
        auto name=identifier();if(take('('))return node(UiExpression::Kind::Call,std::move(name),0,sequence(')'));
        return node(UiExpression::Kind::Reference,std::move(name));
    }
    Expr expression(int minimum=0) {
        auto left=atom();
        for(;;) {
            space();if(at_==source_.size())break;char op=source_[at_];
            int precedence=op=='|'?1:op=='&'?2:(op=='+' || op=='-')?3:(op=='*' || op=='/')?4:0;
            if(!precedence || precedence<minimum)break;++at_;
            left=node(UiExpression::Kind::Operator,std::string(1,op),0,{left,expression(precedence+1)});
        }
        return left;
    }
public:
    Parser(std::string_view source,std::string_view name):source_(source),file_(name){}
    UiScript parse() {
        UiScript script;
        for(;;) {
            space();if(at_==source_.size())return script;
            auto name=identifier();
            if(name=="use")script.imports.push_back(string());
            else {expect('=');if(!script.definitions.emplace(name,expression()).second)error("duplicate definition");}
            expect(';');
        }
    }
};
}
UiScript UiScript::parse(std::string_view source,std::string_view name){return Parser(source,name).parse();}
UiScript UiScript::read(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Cannot open UI script: "+path.string());
    std::string source((std::istreambuf_iterator<char>(input)),{});if(source.size()>1024*1024)throw std::runtime_error("UI script too large");
    return parse(source,path.filename().string());
}
}
