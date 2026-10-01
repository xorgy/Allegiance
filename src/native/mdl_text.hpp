// Small data parser shared by sound graphs and authored slide data. It does
// not evaluate legacy UI objects or participate in frame rendering.
#pragma once
#include <charconv>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
namespace Native::Mdl {
struct Expr {
    enum Kind {Number,String,Symbol,Call,List} kind;
    std::string text;double number{};std::vector<Expr> args;
};
class Parser {
    std::string_view text_;size_t at_{};
public:
    explicit Parser(std::string_view text):text_(text){}
    void space(){for(;;){while(at_<text_.size() && std::isspace(uint8_t(text_[at_])))++at_;
        if(text_.substr(at_,2)=="//"){while(at_<text_.size() && text_[at_]!='\n')++at_;continue;}
        if(text_.substr(at_,2)=="/*"){auto end=text_.find("*/",at_+2);if(end==text_.npos)throw std::runtime_error("Unterminated MDL comment");at_=end+2;continue;}break;}}
    bool end(){space();return at_==text_.size();}
    bool take(char c){space();if(at_<text_.size() && text_[at_]==c){++at_;return true;}return false;}
    void need(char c){if(!take(c))throw std::runtime_error("Expected '"+std::string(1,c)+"' in MDL at "+std::to_string(at_));}
    std::string word(){space();size_t start=at_;while(at_<text_.size() && (std::isalnum(uint8_t(text_[at_])) || text_[at_]=='_'))++at_;
        if(at_==start)throw std::runtime_error("Expected MDL symbol at "+std::to_string(at_));return std::string(text_.substr(start,at_-start));}
    Expr expression(unsigned depth=0){
        Expr result=primary(depth);
        while(take('|')){Expr combined{Expr::Call,"BitOr"};combined.args.push_back(std::move(result));combined.args.push_back(primary(depth+1));result=std::move(combined);}
        return result;
    }
    Expr primary(unsigned depth=0){
        if(depth>128)throw std::runtime_error("MDL expression nesting limit");space();
        if(take('"')){std::string s;while(at_<text_.size() && text_[at_]!='"'){char c=text_[at_++];if(c=='\\' && at_<text_.size()){c=text_[at_++];if(c=='n')c='\n';else if(c=='r')c='\r';else if(c=='t')c='\t';}s+=c;}need('"');return {Expr::String,std::move(s)};}
        if(take('\'')){if(at_==text_.size())throw std::runtime_error("Invalid MDL character");unsigned char c=text_[at_++];need('\'');return {Expr::Number,{},double(c)};}
        if(text_.substr(at_,2)=="0x") {at_+=2;unsigned value;auto r=std::from_chars(text_.data()+at_,text_.data()+text_.size(),value,16);
            if(r.ec!=std::errc())throw std::runtime_error("Invalid MDL hex number");at_=r.ptr-text_.data();return {Expr::Number,{},double(value)};}
        if(at_<text_.size() && (text_[at_]=='-' || text_[at_]=='.' || std::isdigit(uint8_t(text_[at_])))){
            double number;auto r=std::from_chars(text_.data()+at_,text_.data()+text_.size(),number);if(r.ec!=std::errc())throw std::runtime_error("Invalid MDL number");at_=r.ptr-text_.data();return {Expr::Number,{},number};}
        char close=0;if(take('['))close=']';else if(take('('))close=')';
        Expr result{Expr::List,{}};
        if(!close){result.text=word();if(!take('(')){result.kind=Expr::Symbol;return result;}result.kind=Expr::Call;close=')';}
        if(!take(close)){do{result.args.push_back(expression(depth+1));}while(take(','));need(close);}return result;
    }
};
}
