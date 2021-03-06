// ozette
// Copyright (C) 2014-2016 Mars J. Saxman
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

#ifndef APP_REGEX_H
#define APP_REGEX_H

#include <regex.h>
#include <list>
#include <string>

class Regex {
public:
	explicit Regex(std::string pattern);
	Regex(const Regex &);
	Regex &operator=(const Regex &);
	~Regex();
	struct Match {
		bool empty() const { return begin == end; }
		size_t begin = std::string::npos;
		size_t end = std::string::npos;
	};
	Match find(const std::string &text, size_t pos = 0) const;
	typedef std::list<Match> Matches;
	Matches find_all(const std::string &text) const;
private:
	void compile();
	std::string _pattern;
	regex_t _re;
	int _comp_err = 0;
};

#endif //APP_REGEX_H

