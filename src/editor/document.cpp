#include "document.h"
#include <fstream>
#include <sstream>
#include <assert.h>

namespace {
class BlankLine : public Editor::Line
{
public:
	virtual std::string text() const override { return std::string(); }
	virtual size_t size() const override { return 0; }
};

class StrLine : public Editor::Line
{
public:
	StrLine(std::string text): _text(text) {}
	virtual std::string text() const override { return _text; }
	virtual size_t size() const override { return _text.size(); }
private:
	std::string _text;
};
} // namespace

Editor::Document::Document():
	_blank(new BlankLine)
{
}

Editor::Document::Document(std::string targetpath):
	_blank(new BlankLine)
{
	std::string str;
	std::ifstream file(targetpath);
	while (std::getline(file, str)) {
		_maxline = append_line(str);
	}
}

Editor::Line &Editor::Document::line(line_t index)
{
	// Get the line at the specified index.
	// If no such line exists, return a blank.
	return index < _lines.size() ? *_lines[index].get() : *_blank.get();
}

Editor::location_t Editor::Document::home()
{
	location_t loc = {0,0};
	return loc;
}

Editor::location_t Editor::Document::end()
{
	location_t loc = {_maxline, line(_maxline).size()};
	return loc;
}

Editor::position_t Editor::Document::position(const location_t &loc)
{
	// Compute the screen position for this document location.
	position_t out;
	out.v = std::min(_maxline, loc.line);
	out.h = line(loc.line).column(loc.offset);
	return out;
}

Editor::location_t Editor::Document::location(const position_t &loc)
{
	// Locate the character in the document corresponding to the
	// given screen position.
	location_t out;
	out.line = loc.v;
	out.offset = line(out.line).offset(loc.h);
	return out;
}

Editor::location_t Editor::Document::next(location_t loc)
{
	if (loc.offset < line(loc.line).size()) {
		loc.offset++;
	} else if (loc.line < _maxline) {
		loc.offset = 0;
		loc.line++;
	}
	return loc;
}

Editor::location_t Editor::Document::prev(location_t loc)
{
	if (loc.offset > 0) {
		loc.offset--;
	} else if (loc.line > 0) {
		loc.line--;
		loc.offset = line(loc.line).size();
	}
	return loc;
}

std::string Editor::Document::text(const Range &span)
{
	std::stringstream out;
	location_t loc = span.begin();
	location_t end = span.end();
	std::string chunk = substr_to_end(loc);
	while (loc.line < end.line) {
		out << chunk << '\n';
		chunk = line(++loc.line).text();
		loc.offset = 0;
	}
	out << chunk.substr(0, end.offset - loc.offset);
	return out.str();
}

Editor::location_t Editor::Document::erase(const Range &chars)
{
	if (_lines.empty()) return home();
	location_t begin = sanitize(chars.begin());
	std::string prefix = substr_from_home(begin);
	location_t end = sanitize(chars.end());
	std::string suffix = substr_to_end(end);
	size_t index = begin.line;
	auto iter = _lines.begin();
	_lines.erase(iter + begin.line + 1, iter + end.line + 1);
	update_line(index, prefix + suffix);
	return location_t(index, prefix.size());
}

Editor::location_t Editor::Document::insert(location_t loc, char ch)
{
	sanitize(loc);
	if (loc.line < _lines.size()) {
		std::string text = _lines[loc.line]->text();
		text.insert(loc.offset, 1, ch);
		update_line(loc.line, text);
		loc.offset++;
	} else {
		loc.line = append_line(std::string(1, ch));
		loc.offset = 1;
	}
	return loc;
}

Editor::location_t Editor::Document::insert(location_t loc, std::string text)
{
	// Split this line apart around the insertion point. We will insert
	// the new text in between these halves. We will temporarily delete
	// the suffix from its line, since we're likely to be appending more
	// text for a while, but we'll append the suffix back on at the end.
	std::string suffix = substr_to_end(loc);
	update_line(loc.line, substr_from_home(loc));

	// Search the text for linebreaks. Every time we find one, we'll
	// cut all the chars from our search position to the linebreak and
	// append them to the current line. Then we'll insert a new, blank
	// line, which will become the new current line. When we're
	// finally out of linebreaks, we'll concatenate whatever is left
	// of the text with our original suffix and append that to the
	// current line. If there were no linebreaks at all, this will
	// simply be the original line we started on.
	size_t startoff = 0, endoff = 0;
	while ((endoff = text.find('\n', startoff)) != std::string::npos) {
		append_to_line(loc.line++, text.substr(startoff, endoff-startoff));
		insert_line(loc.line, "");
		loc.offset = 0;
		startoff = endoff + 1;
	}

	append_to_line(loc.line, text.substr(startoff, endoff));
	loc.offset = line(loc.line).size();
	append_to_line(loc.line, suffix);
	return loc;
}

Editor::location_t Editor::Document::split(location_t loc)
{
	sanitize(loc);
	std::string text = line(loc.line).text();
	update_line(loc.line, text.substr(0, loc.offset));
	loc.line++;
	insert_line(loc.line, text.substr(loc.offset, std::string::npos));
	loc.offset = 0;
	return loc;
}

std::string Editor::Document::substr_from_home(const location_t &loc)
{
	std::string text = line(loc.line).text();
	return text.substr(0, loc.offset);
}

std::string Editor::Document::substr_to_end(const location_t &loc)
{
	std::string text = line(loc.line).text();
	return text.substr(std::min(text.size(), loc.offset), std::string::npos);
}

void Editor::Document::update_line(line_t index, std::string text)
{
	if (index < _lines.size()) {
		_lines[index].reset(new StrLine(text));
	} else {
		_lines.emplace_back(new StrLine(text));
	}
}

void Editor::Document::insert_line(line_t index, std::string text)
{
	_lines.emplace(_lines.begin() + index, new StrLine(text));
	_maxline = _lines.size() - 1;
}

Editor::line_t Editor::Document::append_line(std::string text)
{
	line_t index = _lines.size();
	_lines.emplace_back(new StrLine(text));
	_maxline = _lines.size()-1;
	return index;
}

void Editor::Document::append_to_line(line_t index, std::string suffix)
{
	update_line(index, line(index).text() + suffix);
}

void Editor::Document::push_to_line(line_t index, std::string prefix)
{
	update_line(index, prefix + line(index).text());
}

Editor::location_t Editor::Document::sanitize(const location_t &loc)
{
	// Verify that this location refers to a real place.
	// Fix it if either of its dimensions would be out-of-bounds.
	line_t index = std::min(loc.line, _lines.size());
	offset_t offset = _lines.empty() ? 0 : std::min(loc.offset, _lines[loc.line]->size());
	return location_t(index, offset);
}

void Editor::Document::sanitize(location_t *loc)
{
	*loc = sanitize(*loc);
}
