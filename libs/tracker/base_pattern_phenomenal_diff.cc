/*
 * Copyright (C) 2018-2019 Nil Geisweiller <ngeiswei@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sstream>

#include "base_pattern_phenomenal_diff.h"

using namespace Tracker;

BasePatternPhenomenalDiff::BasePatternPhenomenalDiff ()
	: full (true) // by default phenomenal diff is full to reduce the odds that
	              // some phenomenal differences are missed
{
}

BasePatternPhenomenalDiff::~BasePatternPhenomenalDiff ()
{
}

bool
BasePatternPhenomenalDiff::empty () const
{
	// The base class has no data, therefore, if it is not full, it is empty.
	return !full;
}

std::string
BasePatternPhenomenalDiff::to_string (const std::string& indent) const
{
	std::stringstream ss;
	ss << indent << "full = " << full;
	return ss.str ();
}
