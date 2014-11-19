//
// lindi
// Copyright (C) 2014 Mars J. Saxman
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#include "browser.h"
#include "control.h"
#include "dialog.h"
#include <cctype>

Browser *Browser::_instance;
static std::string kExpansionStateKey = "expanded_dirs";

void Browser::change_directory(std::string path)
{
	if (_instance) _instance->view(path);
}

void Browser::open(std::string path, UI::Shell &shell)
{
	if (_instance) {
		shell.make_active(_instance->_window);
	} else {
		std::unique_ptr<UI::View> view(new Browser(path));
		UI::Shell::Position pos = UI::Shell::Position::Left;
		_instance->_window = shell.open_window(std::move(view), pos);
	}
}

Browser::Browser(std::string path):
	_tree(path)
{
	_instance = this;
}

void Browser::activate(UI::Frame &ctx)
{
	ctx.set_title(_tree.path());
	if (_expanded_items.empty()) {
		std::vector<std::string> paths;
		ctx.app().get_config(kExpansionStateKey, paths);
		for (auto &path: paths) {
			_expanded_items.insert(path);
		}
		if (!paths.empty()) _rebuild_list = true;
	}

	if (_rebuild_list) ctx.repaint();
}

void Browser::deactivate(UI::Frame &ctx)
{
	std::vector<std::string> paths;
	for (auto &line: _expanded_items) {
		paths.push_back(line);
	}
	ctx.app().set_config(kExpansionStateKey, paths);
}

void Browser::check_rebuild(UI::Frame &ctx)
{
	if (!_rebuild_list) return;
	build_list();
	ctx.set_title(_tree.path());
	_rebuild_list = false;
	ctx.repaint();
}

void Browser::paint_into(WINDOW *view, bool active)
{
	int width;
	getmaxyx(view, _height, width);

	// adjust scrolling as necessary to keep the cursor visible
	size_t max_visible_row = _scrollpos + _height - 2;
	if (_selection < _scrollpos || _selection > max_visible_row) {
		size_t halfpage = (size_t)_height/2;
		_scrollpos = _selection - std::min(halfpage, _selection);
	}

	int row = 1;
	wmove(view, 0, 0);
	wclrtoeol(view);
	for (size_t i = _scrollpos; i < _list.size() && row < _height; ++i) {
		wmove(view, row, 0);
		whline(view, ' ', width);
		paint_row(view, row, _list[i], width);
		if (active && i == _selection) {
			mvwchgat(view, row, 0, width, A_REVERSE, 0, NULL);
		}
		row++;
	}
	while (row < _height) {
		wmove(view, row++, 0);
		wclrtoeol(view);
	}
}

bool Browser::process(UI::Frame &ctx, int ch)
{
	check_rebuild(ctx);
	switch (ch) {
		case Control::Find: ctl_find(ctx); break;
		case Control::Return: key_return(ctx); break;
		case Control::Close: return false; break;
		case Control::Escape: clear_filter(ctx); break;
		case Control::Tab: key_tab(ctx); break;
		case KEY_UP: key_up(ctx); break;
		case KEY_DOWN: key_down(ctx); break;
		case KEY_PPAGE: key_page_up(ctx); break;
		case KEY_NPAGE: key_page_down(ctx); break;
		case KEY_RIGHT: key_right(ctx); break;
		case KEY_LEFT: key_left(ctx); break;
		case ' ': key_space(ctx); break;
		default: {
			if(isprint(ch)) key_char(ctx, ch);
			else clear_filter(ctx);
		} break;
	}
	return true;
}

bool Browser::poll(UI::Frame &ctx)
{
	check_rebuild(ctx);
	if (!_name_filter.empty()) {
		if (_name_filter_time + 2 <= time(NULL)) {
			_name_filter.clear();
			ctx.repaint();
		}
	}
	return true;
}

void Browser::set_help(UI::HelpBar::Panel &panel)
{
	using namespace UI::HelpBar;
	panel.label[0][0] = Label('O', true, "Open");
	panel.label[0][1] = Label('N', true, "New File");
	panel.label[0][5] = Label('F', true, "Find");
	panel.label[1][0] = Label('Q', true, "Quit");
	panel.label[1][1] = Label('E', true, "Execute");
	panel.label[1][2] = Label('D', true, "Directory");
	panel.label[1][5] = Label('?', true, "Help");
}

void Browser::view(std::string path)
{
	if (path != _tree.path()) {
		_list.clear();
		_tree = DirTree(path);
		_rebuild_list = true;
	}
}

void Browser::paint_row(WINDOW *view, int vpos, row_t &display, int width)
{
	int rowchars = width;
	for (unsigned tab = 0; tab < display.indent; tab++) {
		waddnstr(view, "    ", rowchars);
		rowchars -= 4;
	}
	bool isdir = display.entry->is_directory();
	waddnstr(view, display.expanded? "- ": (isdir? "+ ": "  "), rowchars);
	rowchars -= 2;
	std::string name = display.entry->name();
	if (name.substr(0, _name_filter.size()) == _name_filter) {
		wattron(view, A_UNDERLINE);
		waddnstr(view, _name_filter.c_str(), rowchars - 1);
		rowchars -= _name_filter.size();
		wattroff(view, A_UNDERLINE);
		name = name.substr(_name_filter.size());
	}
	waddnstr(view, name.c_str(), rowchars - 1);
	rowchars -= std::min(rowchars, (int)name.size());
	waddnstr(view, isdir? "/": " ", rowchars);
	rowchars--;
	// The rest of the status info only applies to files.
	if (!display.entry->is_file()) return;

	char buf[256];
	time_t mtime = display.entry->mtime();
	struct tm mtm = *localtime(&mtime);
	time_t nowtime = time(NULL);
	struct tm nowtm = *localtime(&nowtime);
	// Pick a format string which highlights the most relevant information
	// about this file's modification date. Format strings end with an extra
	// space because it looks prettier that way.
	const char *format = "%c ";
	if (mtm.tm_year != nowtm.tm_year) {
		format = "%b %Y "; // month of year
	} else if (nowtm.tm_yday - mtm.tm_yday > 6) {
		format = "%e %b "; // day of month
	} else if (nowtm.tm_yday - mtm.tm_yday > 1) {
		format = "%a "; // day of week
	} else if (nowtm.tm_yday != mtm.tm_yday) {
		format = "yesterday ";
	} else {
		format = "%X ";	// time
	}
	size_t dchars = strftime(buf, 255, format, &mtm);
	int drawch = std::min((int)dchars, rowchars);
	mvwaddnstr(view, vpos, width-drawch, buf, drawch);
}

void Browser::ctl_find(UI::Frame &ctx)
{
	std::string prompt = "Find";
	auto action = [this](UI::Frame &ctx, std::string text)
	{
		std::vector<std::string> args = {"-H", "-n", "-I", text};
		auto receiver = [&args](DirTree &item)
		{
			if (item.is_file()) {
				args.push_back(item.path());
			}
			return true;
		};
		_tree.recurse(receiver);
		ctx.app().exec("find: " + text, "grep", args);
	};
	auto dialog = new UI::Dialog::Find(prompt, action);
	std::unique_ptr<UI::View> dptr(dialog);
	ctx.show_dialog(std::move(dptr));
}

void Browser::key_return(UI::Frame &ctx)
{
	switch (sel_entry()->type()) {
		case DirTree::Type::Directory: toggle(ctx); break;
		case DirTree::Type::File: edit_file(ctx); break;
		default: break;
	}
}

void Browser::key_up(UI::Frame &ctx)
{
	// Move to previous line in the listbox.
	clear_filter(ctx);
	if (0 == _selection) return;
	_selection--;
	ctx.repaint();
}

void Browser::key_down(UI::Frame &ctx)
{
	clear_filter(ctx);
	// Move to next line in the listbox.
	if (_selection + 1 == _list.size()) return;
	_selection++;
	ctx.repaint();
}

void Browser::key_page_up(UI::Frame &ctx)
{
	// Move to last line of previous page.
	clear_filter(ctx);
	_selection = _scrollpos - std::min(_scrollpos, 1U);
	ctx.repaint();
}

void Browser::key_page_down(UI::Frame &ctx)
{
	// Move to first line of next page.
	_selection = std::min(_scrollpos + _height, _list.size()-1);
	ctx.repaint();
}

void Browser::key_left(UI::Frame &ctx)
{
	// Move to previous match for filename filter.
	if (_selection == 0) return;
	for (size_t i = _selection; i > 0; --i) {
		if (matches_filter(i-1)) {
			_selection = i-1;
			ctx.repaint();
			return;
		}
	}
}

void Browser::key_right(UI::Frame &ctx)
{
	// Move to next match for filename filter.
	for (size_t i = _selection + 1; i < _list.size(); ++i) {
		if (matches_filter(i)) {
			_selection = i;
			ctx.repaint();
			return;
		}
	}
}

void Browser::key_space(UI::Frame &ctx)
{
	toggle(ctx);
}

void Browser::key_tab(UI::Frame &ctx)
{
	// If there is a name filter, collect all the names which match,
	// then extend the name filter to the common prefix of all names
	if (_name_filter.empty()) return;
	std::string prefix;
	for (size_t i = 0; i < _list.size(); ++i) {
		if (!matches_filter(i)) continue;
		std::string name = _list[i].entry->name();
		if (prefix.empty()) {
			prefix = name;
		} else while (name.substr(0, prefix.size()) != prefix) {
			prefix.pop_back();
		}
	}
	_name_filter = prefix;
	_name_filter_time = time(NULL);
	ctx.repaint();
}

void Browser::key_char(UI::Frame &ctx, char ch)
{
	size_t start = _name_filter.empty()? 0: _selection;
	_name_filter.push_back(ch);
	_name_filter_time = time(NULL);
	for (size_t i = start; i < _list.size(); ++i) {
		if (matches_filter(i)) {
			_selection = i;
			break;
		}
	}
	ctx.repaint();
}

void Browser::clear_filter(UI::Frame &ctx)
{
	if (_name_filter.empty()) return;
	_name_filter.clear();
	ctx.repaint();
}

bool Browser::matches_filter(size_t index)
{
	if (index >= _list.size()) return false;
	std::string name = _list[index].entry->name();
	return name.substr(0, _name_filter.size()) == _name_filter;
}

void Browser::build_list()
{
	_list.clear();
	insert_rows(0, 0, &_tree);
	if (_list.empty()) {
		row_t display = {0, 0, &_tree};
		_list.insert(_list.begin(), display);
	}
	_selection = std::min(_selection, _list.size());
}

void Browser::toggle(UI::Frame &ctx)
{
	clear_filter(ctx);
	auto &display = _list[_selection];
	auto entry = display.entry;
	if (!entry->is_directory()) return;
	if (display.expanded) {
		// collapse it
		_expanded_items.erase(display.entry->path());
		display.expanded = false;
		remove_rows(_selection + 1, display.indent + 1);
	} else {
		// expand it
		_expanded_items.insert(display.entry->path());
		display.expanded = true;
		insert_rows(_selection + 1, display.indent + 1, entry);
	}
	ctx.repaint();
}

void Browser::edit_file(UI::Frame &ctx)
{
	clear_filter(ctx);
	auto &display = _list[_selection];
	ctx.app().edit_file(display.entry->path());
}

size_t Browser::insert_rows(size_t index, unsigned indent, DirTree *entry)
{
	for (auto &item: entry->items()) {
		bool expand = _expanded_items.count(item.path()) > 0;
		row_t display = {indent, expand, &item};
		_list.insert(_list.begin() + index++, display);
		if (expand) {
			index = insert_rows(index, indent + 1, &item);
		}
	}
	return index;
}

void Browser::remove_rows(size_t index, unsigned indent)
{
	auto begin = _list.begin() + index;
	auto iter = begin;
	while (iter != _list.end() && iter->indent >= indent) {
		iter++;
	}
	_list.erase(begin, iter);
}
