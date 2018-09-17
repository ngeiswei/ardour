/*
    Copyright (C) 2018 Nil Geisweiller

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_tracker_tracker_types_h_
#define __ardour_tracker_tracker_types_h_

namespace Tracker {

// Map Parameter to AutomationControl
typedef std::map<Evoral::Parameter, boost::shared_ptr<ARDOUR::AutomationControl> > Parameter2AutomationControl;

} // ~namespace tracker

#endif /* __ardour_tracker_tracker_types_h_ */
