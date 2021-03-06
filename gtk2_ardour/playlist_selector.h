/*
 * Copyright (C) 2005-2009 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_playlist_selector_h__
#define __ardour_playlist_selector_h__

#include <boost/shared_ptr.hpp>

#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/button.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>

#include "ardour_dialog.h"
#include "ardour/playlist.h"
#include "ardour/session_handle.h"

namespace ARDOUR {
	class Session;
	class PluginManager;
	class Plugin;
}

class RouteUI;
class RouteTimeAxisView;

struct PlaylistSorterByID {
	bool operator() (boost::shared_ptr<ARDOUR::Playlist> a, boost::shared_ptr<ARDOUR::Playlist> b) const {
		if (a->pgroup_id().length() && b->pgroup_id().length()) {
			return (a->id() < b->id()); /*both plists have pgroup-id: use IDs which are sequentially generated */
		} else if (!a->pgroup_id().length() && !b->pgroup_id().length()) {
			return (a->sort_id() < b->sort_id()); /*old session: neither plist has a pgroup-id: use prior sort_id calculation */ /*DEPRECATED*/
		} else {
			return (a->pgroup_id().length() < b->pgroup_id().length()); /*mix of old & new: old ones go on top */
		}
	}
};

class PlaylistSelector : public ArdourDialog
{
public:
	PlaylistSelector ();
	~PlaylistSelector ();

	enum plMode {
		plSelect,
		plCopy,
		plShare,
		plSteal
	};

	void redisplay();
	void prepare(RouteUI*, plMode in);

protected:
	bool on_key_press_event (GdkEventKey*);

private:
	typedef std::map<PBD::ID,std::vector<boost::shared_ptr<ARDOUR::Playlist> >*> TrackPlaylistMap;

	Gtk::ScrolledWindow scroller;
	TrackPlaylistMap trpl_map;

	Gtk::HBox *_scope_box;
	Gtk::RadioButton *_scope_all_radio;
	Gtk::RadioButton *_scope_rec_radio;
	Gtk::RadioButton *_scope_grp_radio;

	RouteUI* _rui;

	plMode _mode;

	sigc::connection select_connection;
	PBD::ScopedConnectionList signal_connections;
	void pl_property_changed (PBD::PropertyChange const & what_changed);

	void add_playlist_to_map (boost::shared_ptr<ARDOUR::Playlist>);
	void playlist_added();
	void clear_map ();
	void ok_button_click ();
	void selection_changed ();

	struct ModelColumns : public Gtk::TreeModel::ColumnRecord
	{
		ModelColumns () {
			add (text);
			add (pgrp);
			add (playlist);
		}
		Gtk::TreeModelColumn<std::string> text;
		Gtk::TreeModelColumn<std::string> pgrp;
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Playlist> >   playlist;
	};

	ModelColumns columns;
	Glib::RefPtr<Gtk::TreeStore> model;
	Gtk::TreeView tree;

	boost::shared_ptr<ARDOUR::Playlist> current_playlist;

	bool _ignore_selection;
};

#endif // __ardour_playlist_selector_h__
