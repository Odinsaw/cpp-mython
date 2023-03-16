#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>

using namespace std;

namespace parse {

	bool operator==(const Token& lhs, const Token& rhs) {
		using namespace token_type;

		if (lhs.index() != rhs.index()) {
			return false;
		}
		if (lhs.Is<Char>()) {
			return lhs.As<Char>().value == rhs.As<Char>().value;
		}
		if (lhs.Is<Number>()) {
			return lhs.As<Number>().value == rhs.As<Number>().value;
		}
		if (lhs.Is<String>()) {
			return lhs.As<String>().value == rhs.As<String>().value;
		}
		if (lhs.Is<Id>()) {
			return lhs.As<Id>().value == rhs.As<Id>().value;
		}
		return true;
	}

	bool operator!=(const Token& lhs, const Token& rhs) {
		return !(lhs == rhs);
	}

	std::ostream& operator<<(std::ostream& os, const Token& rhs) {
		using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

		VALUED_OUTPUT(Number);
		VALUED_OUTPUT(Id);
		VALUED_OUTPUT(String);
		VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

		UNVALUED_OUTPUT(Class);
		UNVALUED_OUTPUT(Return);
		UNVALUED_OUTPUT(If);
		UNVALUED_OUTPUT(Else);
		UNVALUED_OUTPUT(Def);
		UNVALUED_OUTPUT(Newline);
		UNVALUED_OUTPUT(Print);
		UNVALUED_OUTPUT(Indent);
		UNVALUED_OUTPUT(Dedent);
		UNVALUED_OUTPUT(And);
		UNVALUED_OUTPUT(Or);
		UNVALUED_OUTPUT(Not);
		UNVALUED_OUTPUT(Eq);
		UNVALUED_OUTPUT(NotEq);
		UNVALUED_OUTPUT(LessOrEq);
		UNVALUED_OUTPUT(GreaterOrEq);
		UNVALUED_OUTPUT(None);
		UNVALUED_OUTPUT(True);
		UNVALUED_OUTPUT(False);
		UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

		return os << "Unknown token :("sv;
	}

	LineOfCode::LineOfCode(istream& input)
		:input_(input)
	{
		ReadLine();
	}

	size_t LineOfCode::GetStartingSpaces() {
		return starting_spaces_;
	}

	vector<Token>& LineOfCode::GetTokens() {
		return line_tokens_;
	}

	size_t LineOfCode::GetSize() {
		return line_tokens_.size();
	}

	void SkipNewLinesOnly(istream& input) {
		while (input.peek() == '\n') input.get();
	}

	void LineOfCode::ReadLine() {
		SkipNewLinesOnly(input_);
		starting_spaces_ = ProcessSpaces();

		char ch;
		do {
			ch = input_.peek();
			if (ch == ' ') {
				ProcessSpaces();
			}
			else if (ch == '#') {
				ProcessComment();
			}
			else if (ch == EOF) {
				if (!IsEmpty() && !line_tokens_.back().Is<token_type::Newline>()) {
					line_tokens_.emplace_back(token_type::Newline{});
				}
				line_tokens_.emplace_back(token_type::Eof{});
			}
			else if (ch == '"' || ch == '\'') {
				ReadString();
			}
			else if (isdigit(ch)) {
				ReadNumber();
			}
			else if (isalpha(ch) || ch == '_') {
				ReadIdentifier();
			}
			else {
				char c = input_.get();
				if ((c == '!' || c == '=' || c == '<' || c == '>') && input_.peek() == '=') {
					ReadCompSymb(c);
				}
				else if (c == '\n') {
					line_tokens_.emplace_back(token_type::Newline{});
				}
				else {
					line_tokens_.emplace_back(token_type::Char{ static_cast<char>(c) });
				}
			}
		} while (!(ch == '\n' || ch == EOF));
	}

	size_t LineOfCode::ProcessSpaces() {
		size_t count = 0;
		while (input_.get() == ' ') ++count;
		input_.unget();
		return count;
	}

	void LineOfCode::ProcessComment() {
		for (; !(input_.get() == '\n' || input_.eof()););
		input_.unget();    // вернем последний считанный символ (\n) в поток
	}

	void LineOfCode::ReadNumber() {
		string parsed_num;
		while (std::isdigit(input_.peek())) {
			parsed_num += static_cast<char>(input_.get());
		}

		line_tokens_.emplace_back(token_type::Number{ stoi(parsed_num) });
	}

	void LineOfCode::ReadString() {
		auto it = std::istreambuf_iterator<char>(input_);
		auto end = std::istreambuf_iterator<char>();
		std::string s;
		char quote_type = input_.get();
		while (true) {
			if (it == end) {
				throw LexerError("String parsing error");
			}
			const char ch = *it;
			if (ch == quote_type) {
				++it;
				break;
			}
			else if (ch == '\\') {
				++it;
				if (it == end) {
					throw LexerError("String parsing error");
				}
				const char escaped_char = *(it);
				switch (escaped_char) {
				case 'n':
					s.push_back('\n');
					break;
				case 't':
					s.push_back('\t');
					break;
				case 'r':
					s.push_back('\r');
					break;
				case '"':
					s.push_back('"');
					break;
				case '\'':
					s.push_back('\'');
					break;
				case '\\':
					s.push_back('\\');
					break;
				default:
					throw LexerError("Unrecognized escape sequence \\"s + escaped_char);
				}
			}
			else if (ch == '\n' || ch == '\r') {
				throw LexerError("Unexpected end of line"s);
			}
			else {
				s.push_back(ch);
			}
			++it;
		}

		line_tokens_.emplace_back(token_type::String{ std::move(s) });
	}

	void LineOfCode::ReadIdentifier() {
		string parsed_ident;
		for (char ch = input_.get(); (isalpha(ch) || ch == '_' || isdigit(ch)); ch = input_.get()) {
			parsed_ident += static_cast<char>(ch);
		}
		input_.unget();

		if (parsed_ident == "class"sv) {
			line_tokens_.emplace_back(token_type::Class{});
		}
		else if (parsed_ident == "return"sv) {
			line_tokens_.emplace_back(token_type::Return{});
		}
		else if (parsed_ident == "if"sv) {
			line_tokens_.emplace_back(token_type::If{});
		}
		else if (parsed_ident == "else"sv) {
			line_tokens_.emplace_back(token_type::Else{});
		}
		else if (parsed_ident == "def"sv) {
			line_tokens_.emplace_back(token_type::Def{});
		}
		else if (parsed_ident == "print"sv) {
			line_tokens_.emplace_back(token_type::Print{});
		}
		else if (parsed_ident == "and"sv) {
			line_tokens_.emplace_back(token_type::And{});
		}
		else if (parsed_ident == "or"sv) {
			line_tokens_.emplace_back(token_type::Or{});
		}
		else if (parsed_ident == "not"sv) {
			line_tokens_.emplace_back(token_type::Not{});
		}
		else if (parsed_ident == "None"sv) {
			line_tokens_.emplace_back(token_type::None{});
		}
		else if (parsed_ident == "True"sv) {
			line_tokens_.emplace_back(token_type::True{});
		}
		else if (parsed_ident == "False"sv) {
			line_tokens_.emplace_back(token_type::False{});
		}
		else {
			line_tokens_.emplace_back(token_type::Id{ std::move(parsed_ident) });
		}
	}

	void LineOfCode::ReadCompSymb(char c) {
		if (c == '!') {
			line_tokens_.emplace_back(token_type::NotEq{});
		}
		else if (c == '=') {
			line_tokens_.emplace_back(token_type::Eq{});
		}
		else if (c == '<') {
			line_tokens_.emplace_back(token_type::LessOrEq{});
		}
		else if (c == '>') {
			line_tokens_.emplace_back(token_type::GreaterOrEq{});
		}
		input_.get(); //remove next '='
	}

	bool LineOfCode::IsEmpty() const {
		return line_tokens_.empty() || std::all_of(line_tokens_.cbegin(), line_tokens_.cend(), [](const auto& t) {
			return t.template Is<token_type::Newline>();
			});
	}

	bool LineOfCode::IsAllEof() const {
		return line_tokens_.empty()
			|| std::all_of(line_tokens_.cbegin(), line_tokens_.cend(), [](const auto& t) {
			return t.template Is<token_type::Eof>();
				});
	}

	Lexer::Lexer(std::istream& input)
		:input_(input)
	{
		ParseLine();
	}

	const Token& Lexer::CurrentToken() const {
		assert(!tokens_.empty() && cur_token_ < tokens_.size());
		return tokens_[cur_token_];
	}

	Token Lexer::NextToken() {
		if (cur_token_ == tokens_.size() - 1) {
			return ParseLine();
		}
		return tokens_[++cur_token_];
	}

	Token Lexer::ParseLine() {
		LineOfCode line(input_);

		if (line.GetStartingSpaces() % 2 != 0) throw LexerError("Incorrect indent.");

		if (!line.IsAllEof() && !line.IsEmpty()) {

			cur_token_ = tokens_.size();

			if (line.GetStartingSpaces() / 2 > cur_indent_) {
				size_t indents = line.GetStartingSpaces() / 2 - cur_indent_;
				cur_indent_ = line.GetStartingSpaces() / 2;
				for (size_t i = 0; i < indents; ++i) {
					tokens_.emplace_back(token_type::Indent{});
				}
			}
			else if (line.GetStartingSpaces() / 2 < cur_indent_) {
				size_t dedents = cur_indent_ - line.GetStartingSpaces() / 2;
				cur_indent_ = line.GetStartingSpaces() / 2;
				for (size_t i = 0; i < dedents; ++i) {
					tokens_.emplace_back(token_type::Dedent{});
				}
			}

			for (const auto& t : line.GetTokens()) {
				tokens_.emplace_back(t);
			}
			return CurrentToken();
		}
		else if(line.IsAllEof()) {
			cur_token_ = tokens_.size();
			if (cur_indent_ > 0) {
				for (size_t i = 0; i < cur_indent_; ++i) {
					tokens_.emplace_back(token_type::Dedent{});
				}
			}
			tokens_.emplace_back(token_type::Eof{});
			return CurrentToken();
		}
		else {
			return ParseLine();
		}
	}
}  // namespace parse
