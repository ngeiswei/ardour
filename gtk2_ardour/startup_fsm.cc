/*
 * Copyright (C) 2019 Paul Davis <paul@linuxaudiosystems.com>
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

#include <vector>

#include <ytkmm/dialog.h>
#include <ytkmm/liststore.h>
#include <ytkmm/messagedialog.h>
#include <ytkmm/stock.h>

#include "pbd/basename.h"
#include "pbd/file_archive.h"
#include "pbd/file_utils.h"

#include "ardour/audioengine.h"
#include "ardour/audio_backend.h"
#include "ardour/filename_extensions.h"
#include "ardour/filesystem_paths.h"
#include "ardour/profile.h"
#include "ardour/recent_sessions.h"
#include "ardour/rc_configuration.h"
#include "ardour/search_paths.h"
#include "ardour/session.h"
#include "ardour/session_utils.h"
#include "ardour/template_utils.h"

#include "gtkmm2ext/application.h"
#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/keyboard.h>

#include "ardour_message.h"
#include "ardour_ui.h"
#include "debug.h"
#include "engine_dialog.h"
#include "new_user_wizard.h"
#include "opts.h"
#include "plugin_scan_dialog.h"
#include "session_dialog.h"
#include "splash.h"
#include "startup_fsm.h"

#ifdef WAF_BUILD
#include "gtk2ardour-version.h"
#endif

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;

using std::string;
using std::vector;

StartupFSM::StartupFSM (EngineControl& amd)
	: session_domain (Config->get_preferred_time_domain())
	, session_existing_sample_rate (0)
	, session_engine_hints ("EngineHints")
	, session_is_new (false)
	, session_name_edited (false)
	, session_loaded (false)
	, new_user (NewUserWizard::required())
	, new_session_required (ARDOUR_COMMAND_LINE::new_session || new_user)
	, _state (new_user ? WaitingForNewUser : WaitingForSessionPath)
	, audiomidi_dialog (amd)
	, new_user_dialog (0)
	, session_dialog (0)
	, pre_release_dialog (0)
	, plugin_scan_dialog (0)
{
	/* note that our initial state can be any of:
	 *
	 * WaitingForPreRelease:  if this is a pre-release build of Ardour and the user has not testified to their fidelity to our creed
	 * WaitingForNewUser:     if this is the first time any version appears to have been run on this machine by this user
	 * WaitingForSessionPath: if the previous two conditions are not true
	 */

	if (string (VERSIONSTRING).find (".pre0") != string::npos) {
		string fn = Glib::build_filename (user_config_directory(), ".i_swear_that_i_will_heed_the_guidelines_stated_in_the_pre_release_dialog");
		if (!Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
			set_state (WaitingForPreRelease);
		}
	}

	Application* app = Application::instance ();

	app_quit_connection = app->ShouldQuit.connect (sigc::mem_fun (*this, &StartupFSM::queue_finish));
	hide_quit_connection = Gtkmm2ext::Keyboard::HideMightMeanQuit.connect (sigc::mem_fun (*this, &StartupFSM::dialog_hidden));
}

StartupFSM::~StartupFSM ()
{
	delete session_dialog;
	delete pre_release_dialog;
	delete plugin_scan_dialog;
	delete new_user_dialog;
}

void
StartupFSM::set_complete ()
{
	app_quit_connection.disconnect ();
	hide_quit_connection.disconnect ();

	_state = NotWaiting;
}

void
StartupFSM::dialog_hidden (Gtk::Window* /* ignored */)
{
	/* since this object only exists during startup, any attempt to close
	 * any dialog that we manage with Ctrl/Cmd-w is assumed to indicate a
	 * desire to quit on the part of the user.
	 */
	queue_finish ();
}

void
StartupFSM::queue_finish ()
{
	_signal_response (QuitProgram);
}

void
StartupFSM::start ()
{
	DEBUG_TRACE (DEBUG::GuiStartup, string_compose (X_("State at startup: %1\n"), enum_2_string (_state)));

	switch (_state) {
	case NotWaiting:
		abort();
		break;

	case WaitingForPreRelease:
		show_pre_release_dialog ();
		break;
	case WaitingForNewUser:
		show_new_user_dialog ();
		break;
	case WaitingForSessionPath:
		handle_waiting_for_session_path ();
		break;
	case WaitingForEngineParams:
		start_audio_midi_setup ();
		break;
	default:
		fatal << string_compose (_("Programming error: %1"), string_compose (X_("impossible starting state in StartupFSM (%1)"), enum_2_string (_state))) << endmsg;
		std::cerr << string_compose (_("Programming error: %1"), string_compose (X_("impossible starting state in StartupFSM (%1)"), enum_2_string (_state))) << std::endl;
		/* NOTREACHED */
		abort ();
	}

	DEBUG_TRACE (DEBUG::GuiStartup, string_compose (X_("State after startup: %1\n"), enum_2_string (_state)));
}

void
StartupFSM::reset()
{
	show_session_dialog (new_session_required);
}

void
StartupFSM::set_state (MainState ms)
{
	DEBUG_TRACE (DEBUG::GuiStartup, string_compose (X_("new state: %1\n"), enum_2_string (ms)));
	_state = ms;
}

template<typename T> void
StartupFSM::end_dialog (T** d)
{
	assert (d);
	assert (*d);

	end_dialog (**d);
	delete_when_idle (*d);
	*d = 0;
}

template<typename T> void
StartupFSM::end_dialog (T& d)
{
	d.hide ();
	current_dialog_connection.disconnect ();
}

void
StartupFSM::dialog_response_handler (int response, StartupFSM::DialogID dialog_id)
{
	DEBUG_TRACE (DEBUG::GuiStartup, string_compose ("Response %1 from %2 (nsr: %3 / nu: %4)\n", enum_2_string (Gtk::ResponseType (response)), enum_2_string (dialog_id), new_session_required, new_user));

	/* Notes:
	 *
	 * 1) yes, a brand new user might have specified a command line
	 * argument naming a new session. We ignore it. You're new to Ardour?
	 * We want to guide you through the startup.
	 */

	switch (_state) {
	case NotWaiting:
		abort();
		break;

	case WaitingForPreRelease:
		switch (dialog_id) {
		case ApplicationPseudoDialog:
			/* this shouldn't happen; ignore it */
			break;
		case PreReleaseDialog:
		default:
			/* any response value from the pre-release dialog means
			   "move along now"
			*/
			end_dialog (&pre_release_dialog);

			if (NewUserWizard::required()) {
				show_new_user_dialog ();
			} else {
				handle_waiting_for_session_path ();
			}
			break;
		}
		break;

	case WaitingForNewUser:
		switch (dialog_id) {
		case ApplicationPseudoDialog:
			/* this shouldn't happen; ignore it */
			break;
		case NewUserDialog:
			switch (response) {
			case RESPONSE_OK:
				end_dialog (&new_user_dialog);
				show_session_dialog (new_session_required);
				break;
			default:
				_signal_response (QuitProgram);
			}
		default:
			/* ERROR */
			break;
		}
		break;

	case WaitingForSessionPath:
		switch (dialog_id) {
		case NewSessionDialog:
			switch (response) {
			case RESPONSE_OK:
			case RESPONSE_ACCEPT:
				switch (check_session_parameters (new_session_required)) {
				case -1:
					/* Unrecoverable error */
					_signal_response (ExitProgram);
					break;
				case 0:
					end_dialog (&session_dialog);
					start_audio_midi_setup ();
					break;
				case 1:
					/* do nothing - keep dialog up for a
					 * retry. Errors were addressed by
					 * ::check_session_parameters()
					 */
					session_dialog->present ();
					break;
				}
				break;

			default:
				_signal_response (QuitProgram);
				break;
			}
			break;

		case ApplicationPseudoDialog:
			/* macOS, NSM etc. ... existence was already checked */
			if (get_session_parameters_from_path (ARDOUR_COMMAND_LINE::session_name, string(), false)) {
				start_audio_midi_setup ();
			}
			break;

		default:
			/* ERROR */
			break;
		}
		break;

	case WaitingForEngineParams:
		switch (dialog_id) {
		case AudioMIDISetup:
			switch (response) {
			case RESPONSE_OK:
			case RESPONSE_ACCEPT:
				if (AudioEngine::instance()->running()) {
					/* prevent double clicks from changing engine state */
					audiomidi_dialog.set_ui_sensitive (false);
					end_dialog (audiomidi_dialog);
					engine_running ();
				} else {
					/* just keep going */
				}
				break;
			default:
				_signal_response (QuitProgram);
			}
			break;
		default:
			/* ERROR */
			break;
		}
		break;

	case WaitingForPlugins:
		switch (dialog_id) {
		case PluginDialog:
			end_dialog (&plugin_scan_dialog);
			switch (response) {
			case RESPONSE_OK:
				if (AudioEngine::instance()->running()) {
					_signal_response (session_loaded ? LoadedSession : LoadSession);
				} else {
					/* Engine died unexpectedly (it was
					 * running after
					 * WaitingForEngineParams).  Nothing to
					 * do but go back to the audio/MIDI
					 * setup. It would be nice, perhaps, to
					 * show an extra message indicating
					 * that something is not right.
					 */
					ArdourMessageDialog msg (_("The audio/MIDI engine has stopped running unexpectedly.\nSomething is probably wrong with your audio/MIDI device settings."));
					msg.set_position (WIN_POS_CENTER);
					msg.run();
					/* This has been shown before, so we do
					 *  not need start_audio_midi_setup ();
					 */
					show_audiomidi_dialog ();
				}
				break;
			default:
				_signal_response (QuitProgram);
				break;
			}
		default:
			/* ERROR */
			break;
		}
	}
}

void
StartupFSM::handle_waiting_for_session_path ()
{
	if (ARDOUR_COMMAND_LINE::session_name.empty()) {

		/* nothing given on the command line ... show new session dialog */

		show_session_dialog (new_session_required);

	} else {

		if (get_session_parameters_from_command_line (new_session_required)) {

			/* command line arguments all OK. Get engine parameters */

			if (!new_session_required && session_existing_sample_rate > 0) {
				audiomidi_dialog.set_desired_sample_rate (session_existing_sample_rate);
			}

			start_audio_midi_setup ();

		} else {

			/* command line arguments not good. Use
			 * dialog, but prime the dialog with
			 * the information we set up in
			 * get_session_parameters_from_command_line()
			 */

			show_session_dialog (new_session_required);
		}
	}
}

void
StartupFSM::show_plugin_scan_dialog ()
{
	set_state (WaitingForPlugins);

	/* if the user does not ask to discover AU/VSTs at startup, or if this is Mixbus, then the plugin scan
	 * that we run here, during startup, should only use the existing plugin cache (if any).
	 */
	const bool cache_only = (!Config->get_discover_plugins_on_start () || Profile->get_mixbus ());
	const bool verbose = new_user;

	plugin_scan_dialog = new PluginScanDialog (cache_only, verbose);
	current_dialog_connection = plugin_scan_dialog->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &StartupFSM::dialog_response_handler), PluginDialog));
	plugin_scan_dialog->set_position (WIN_POS_CENTER);

	/* We don't show the plugin scan dialog by default. It will appear using it's own code if/when plugins are discovered, if required.
	 *
	 * See also comments in PluginScanDialog::start() to understand the absurd complexities behind this call.
	 */

	DEBUG_TRACE (DEBUG::GuiStartup, string_compose ("starting plugin dialog, cache only ? %1\n", cache_only));
	plugin_scan_dialog->start();
	DEBUG_TRACE (DEBUG::GuiStartup, "plugin dialog done\n");
}

void
StartupFSM::show_new_user_dialog ()
{
	set_state (WaitingForNewUser);
	new_user_dialog = new NewUserWizard;
	current_dialog_connection = new_user_dialog->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &StartupFSM::dialog_response_handler), NewUserDialog));
	new_user_dialog->set_position (WIN_POS_CENTER);
	new_user_dialog->present ();
}

void
StartupFSM::show_session_dialog (bool new_session_required)
{
	set_state (WaitingForSessionPath);
	session_dialog = new SessionDialog (new_session_required ? SessionDialog::New : SessionDialog::Recent, session_name, session_path, session_template, false);
	current_dialog_connection = session_dialog->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &StartupFSM::dialog_response_handler), NewSessionDialog));
	session_dialog->set_position (WIN_POS_CENTER);
	session_dialog->present ();
}

void
StartupFSM::show_audiomidi_dialog ()
{
	set_state (WaitingForEngineParams);
	current_dialog_connection = audiomidi_dialog.signal_response().connect (sigc::bind (sigc::mem_fun (*this, &StartupFSM::dialog_response_handler), AudioMIDISetup));
	audiomidi_dialog.set_position (WIN_POS_CENTER);
	audiomidi_dialog.present ();
}

void
StartupFSM::start_audio_midi_setup ()
{
	BootMessage (_("Starting Audio/MIDI Engine"));
	bool setup_required = false;

	std::shared_ptr<AudioBackend> backend = AudioEngine::instance()->current_backend();
	if (!backend) {
		/* backend is unknown ... */
		setup_required = true;

	} else if (session_is_new && AudioEngine::instance()->running() && AudioEngine::instance()->sample_rate () == session_existing_sample_rate) {
		/* keep engine */

		warning << "A running engine should not be possible at this point" << endmsg;

	} else if (AudioEngine::instance()->setup_required()) {
		/* backend is known, but setup is needed */
		setup_required = true;

	} else if (!AudioEngine::instance()->running()) {
		/* should always be true during startup */
		if (AudioEngine::instance()->start()) {
			setup_required = true;
		}
	}

	bool try_autostart = !new_user && (Config->get_try_autostart_engine () || g_getenv ("ARDOUR_TRY_AUTOSTART_ENGINE"));
	if (session_is_new) {
		try_autostart = false;
	} else if (!backend) {
		try_autostart = false;
	} else if (try_autostart) {
		/* if user has selected auto-start, check if autostart is possible */
		bool ok = true;
		std::string backend_name;
		/* Allow auto-start if there is no backend information for the
		 * given session. This can happen when loading an old (v6) session,
		 * of if the user has been using externally started JACK.
		 */
		if (session_engine_hints.get_property ("backend", backend_name)) {
			std::string input_device;
			std::string output_device;
			ok &= session_engine_hints.get_property ("input-device", input_device);
			ok &= session_engine_hints.get_property ("output-device", output_device);
			ok &= backend->name () == backend_name;
			if (backend->use_separate_input_and_output_devices()) {
				ok &= input_device == backend->input_device_name ();
				ok &= output_device == backend->output_device_name ();
			} else {
				ok &= input_device == backend->device_name ();
				ok &= output_device == backend->device_name ();
			}
		}
		if (!ok) {
			try_autostart = false;
			ArdourMessageDialog msg (
					_("Engine I/O device has changed since you last opened this session.\n"
					  "Please verify that the new device has enough ports, or you may lose some i/o connections."),
					false, Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK, true);
			msg.run ();
		}
	}

	if (setup_required) {
		if (try_autostart) {
			AudioEngine::instance()->set_sample_rate(session_existing_sample_rate);
			if (!AudioEngine::instance()->start ()) {
				if (ARDOUR::AudioEngine::instance()->running()) {
					DEBUG_TRACE (DEBUG::GuiStartup, "autostart successful, audio/MIDI setup dialog not required\n");
					engine_running ();
					return;
				}
			}
		}

		if (!session_is_new && session_existing_sample_rate > 0) {
			audiomidi_dialog.set_desired_sample_rate (session_existing_sample_rate);
		}

		show_audiomidi_dialog ();
		DEBUG_TRACE (DEBUG::GuiStartup, "audiomidi shown and waiting\n");

	} else {

		DEBUG_TRACE (DEBUG::GuiStartup, "engine already running, audio/MIDI setup dialog not required\n");
		engine_running ();
	}
}

void
StartupFSM::engine_running ()
{
	DEBUG_TRACE (DEBUG::GuiStartup, "engine running, start plugin scan then attach UI to engine\n");

	/* This may be very slow. See comments in PluginScanDialog::start() */
	show_plugin_scan_dialog ();

	DEBUG_TRACE (DEBUG::GuiStartup, "attach UI to engine\n");

	/* This may be very slow: it will run the GUI's post-engine
	   initialization which is essentially unbounded in time/scope of what
	   it can do.
	*/

	ARDOUR_UI::instance()->attach_to_engine ();

	/* now that we've done the plugin scan AND attached the UI to the engine, we can
	   proceed with the next (final) steps of startup. This uses the same response
	   signal mechanism we use for the other dialogs.
	*/

	plugin_scan_dialog->response (RESPONSE_OK);
}

bool
StartupFSM::get_session_parameters_from_command_line (bool new_session_required)
{
	return get_session_parameters_from_path (ARDOUR_COMMAND_LINE::session_name, ARDOUR_COMMAND_LINE::load_template, new_session_required);
}

bool
StartupFSM::get_session_parameters_from_path (string const & path_, string const & template_name, bool new_session_required)
{
	if (path_.empty()) {
		/* use GUI to ask the user */
		return false;
	}

	string path (path_);

	if (Glib::file_test (path.c_str(), Glib::FILE_TEST_EXISTS)) {

		session_is_new = false;

		if (new_session_required) {
			/* wait! it already exists */

			if (!ask_about_loading_existing_session (path)) {
				return false;
			} else {
				/* load it anyway */
			}
		}

		// TODO THIS SHOULD CALL ::check_session_parameters (false)
		// to handle session archives etc
		// (needs refactoring of ::check_session_parameters)

		session_name = basename_nosuffix (path);

		if (Glib::file_test (path.c_str(), Glib::FILE_TEST_IS_REGULAR)) {
			/* session/snapshot file, change path to be dir */
			session_path = Glib::path_get_dirname (path);
		} else {
			session_path = path;
		}

		float sr;
		SampleFormat fmt;
		string program_version;

		const string statefile_path = Glib::build_filename (session_path, session_name + ARDOUR::statefile_suffix);
		switch (Session::get_info_from_path (statefile_path, sr, fmt, program_version, &session_engine_hints)) {
			case 0:
				/* OK */
				break;
			case -1:
				error << string_compose (_("Session file %1 does not exist"), statefile_path) << endmsg;
				return false;
				break;
			case -3:
				error << string_compose (_("Session %1 is from a newer version of %2"), statefile_path, PROGRAM_NAME) << endmsg;
				return false;
				break;
			default:
				error << string_compose (_("Cannot get existing session information from %1"), statefile_path) << endmsg;
				return false;
		}

		session_existing_sample_rate = sr;
		return true;

	}

	if (!ARDOUR_COMMAND_LINE::new_session) {
		return false;
	}

	/*  ... did the  user give us a path or just a name? */
	if (!Glib::path_is_absolute (path)) {
		/* check for cwd relative path */
		if (path.find (G_DIR_SEPARATOR) == string::npos) {
			path = Glib::build_filename (Config->get_default_session_parent_dir (), path);
		}
	}

	/* Everything after this involves a new session */

	session_name = basename_nosuffix (path);
	session_path = path;

	session_template = string ();

	if (!template_name.empty()) {

		/* Allow the user to specify a template via path or name on the
		 * command line
		 */

		bool have_resolved_template_name = false;

		/* compare by name (path may or may not be UTF-8) */

		vector<TemplateInfo> templates;
		find_session_templates (templates, false);
		for (vector<TemplateInfo>::iterator x = templates.begin(); x != templates.end(); ++x) {
			if ((*x).name == template_name) {
				session_template = (*x).path;
				have_resolved_template_name = true;
				break;
			}
		}

		/* look up script by name */
		LuaScriptList scripts (LuaScripting::instance ().scripts (LuaScriptInfo::SessionInit));
		LuaScriptList& as (LuaScripting::instance ().scripts (LuaScriptInfo::EditorAction));
		for (LuaScriptList::const_iterator s = as.begin(); s != as.end(); ++s) {
			if ((*s)->subtype & LuaScriptInfo::SessionSetup) {
				scripts.push_back (*s);
			}
		}
		std::sort (scripts.begin(), scripts.end(), LuaScripting::Sorter());
		for (LuaScriptList::const_iterator s = scripts.begin(); s != scripts.end(); ++s) {
			if ((*s)->name == template_name) {
				session_template = "urn:ardour:" + (*s)->path;
				have_resolved_template_name = true;
				break;
			}
		}

		if (!have_resolved_template_name) {
			/* this will produce a more or less meaningful error later:
			 * "ERROR: Could not open session template [abs-path to user-config dir]"
			 */
			session_template = Glib::build_filename (ARDOUR::user_template_directory (), template_name);
		}
	}

	/* We don't know what this is, because the session is new and the
	 * command line doesn't let us specify it. The user will get to decide
	 * in the audio/MIDI dialog.
	 */

	session_existing_sample_rate = 0;
	session_is_new = true;

	/* this is an arbitrary default value but since the user insists on
	 * starting a new session from the command line, it will do as well as
	 * any other possible value. I mean, seriously, what else could it be
	 * by default?
	 */

	bus_profile.master_out_channels = 2;

	return true;
}

/** return values:
 * -1: failure
 *  1: failure but user can retry
 *  0: success, session parameters ready for use
 */
int
StartupFSM::check_session_parameters (bool must_be_new)
{
	bool requested_new = false;
	session_loaded     = false;

	session_name        = session_dialog->session_name (requested_new);
	session_path        = session_dialog->session_folder ();
	session_domain      = session_dialog->session_domain ();
	session_name_edited = session_dialog->was_new_name_edited ();

	if (must_be_new) {
		assert (requested_new);
	}

	if (!must_be_new) {

		/* See if the specified session is a session archive */

		int rv = ARDOUR::inflate_session (session_name, Config->get_default_session_parent_dir(), session_path, session_name);
		if (rv < 0) {
			ArdourMessageDialog msg (*session_dialog, string_compose (_("Extracting session-archive failed: %1"), inflate_error (rv)));
			msg.run ();
			return 1;
		} else if (rv == 0) {
			/* names are good (and session is unarchived/inflated */
			float sr;
			SampleFormat fmt;
			string program_version;
			const string statefile_path = Glib::build_filename (session_path, session_name + ARDOUR::statefile_suffix);
			switch (Session::get_info_from_path (statefile_path, sr, fmt, program_version, &session_engine_hints)) {
				case 0:
					/* OK */
					break;
				case -1:
					error << string_compose (_("Session file %1 does not exist"), statefile_path) << endmsg;
					return -1;
					break;
				case -3:
					error << string_compose (_("Session %1 is from a newer version of %2"), statefile_path, PROGRAM_NAME) << endmsg;
					return -1;
					break;
				default:
					error << string_compose (_("Cannot get existing session information from %1"), statefile_path) << endmsg;
					return -1;
			}

			session_existing_sample_rate = sr;
			return 0;
		}
	}

	if (!must_be_new) {
		int rv = ARDOUR_UI::instance()->new_session_from_aaf (session_name, Config->get_default_session_parent_dir(), session_path, session_name);
		if (rv < 0) {
			ArdourMessageDialog msg (*session_dialog, _("Extracting aaf failed"));
			msg.run ();
			return 1;
		} else if (rv == 0) {
			if (ARDOUR_UI::instance()->session ()) {
				session_existing_sample_rate = ARDOUR_UI::instance()->session ()->nominal_sample_rate ();
			}
			session_loaded = true;
			return 0;
		}
	}

	/* check for ".ardour" in statefile name, because we don't want
	 * it
	 *
	 * XXX Note this weird conflation of a
	 * file-name-without-a-suffix and the session name. It's not
	 * really a session name at all, but rather the suffix-free
	 * name of a statefile (snapshot).
	 */

	const string::size_type suffix_at = session_name.find (statefile_suffix);

	if (suffix_at != string::npos) {
		session_name = session_name.substr (0, suffix_at);
	}

	/* this shouldn't happen, but we catch it just in case it does */

	if (session_name.empty()) {
		return 1; /* keep running dialog */
	}

	if (session_dialog->use_session_template()) {
		session_template = session_dialog->session_template_name();
	}

	if (Glib::path_is_absolute (session_name) || session_name.find (G_DIR_SEPARATOR) != string::npos) {
		/* user typed absolute path or cwd-relative path
		 * specified into session name field. So ... infer
		 * session path and name from what was given.
		 */
		session_path = Glib::path_get_dirname (session_name);
		session_name = Glib::path_get_basename (session_name);

	} else {

		/* session name is just a name */
	}

	/* check if the currently-exists status matches whether or not
	 * it should be new
	 */

	if (Glib::file_test (session_path, Glib::FileTest (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))) {

		if (requested_new /*&& !nsm*/) {

			std::string existing = Glib::build_filename (session_path, session_name);

			if (!ask_about_loading_existing_session (existing)) {
				session_dialog->show ();
				session_dialog->clear_name ();
				return 1; /* try again */
			}
		}

		session_is_new = false;

	} else {

		/* does not exist at present */

		if (!requested_new) {
			ArdourMessageDialog msg (string_compose (_("There is no existing session at \"%1\""), session_path));
			msg.run ();
			session_dialog->clear_name();
			return 1;
		}

		session_is_new = true;
	}


	/* check if name is legal (error for new sessions only) */
	std::string const& illegal = Session::session_name_is_legal (session_name);

	if (!illegal.empty() && session_is_new) {
		ArdourMessageDialog msg (*session_dialog,
		                         string_compose (_("To ensure compatibility with various systems\n"
		                                           "session names may not contain a '%1' character"),
		                                         illegal));
		msg.run ();
		ARDOUR_COMMAND_LINE::session_name = ""; // cancel that
		return 1; /* keep running dialog */
	}


	float sr;
	SampleFormat fmt;
	string program_version;
	const string statefile_path = Glib::build_filename (session_path, session_name + ARDOUR::statefile_suffix);

	if (!session_is_new) {
		switch (Session::get_info_from_path (statefile_path, sr, fmt, program_version, &session_engine_hints)) {
			case 0:
				/* OK */
				break;
			case -1:
				error << string_compose (_("Session file %1 does not exist"), statefile_path) << endmsg;
				return -1;
				break;
			case -3:
				error << string_compose (_("Session %1 is from a newer version of %2"), statefile_path, PROGRAM_NAME) << endmsg;
				return -1;
				break;
			default:
				error << string_compose (_("Cannot get existing session information from %1"), statefile_path) << endmsg;
				return -1;
		}

		session_existing_sample_rate = sr;

	} else {

		bus_profile.master_out_channels = session_dialog->master_channel_count ();
	}

	return 0;
}

void
StartupFSM::copy_demo_sessions ()
{
	// TODO: maybe IFF brand_new_user
	if (ARDOUR::Profile->get_mixbus () && Config->get_copy_demo_sessions ()) {
		std::string dspd (Config->get_default_session_parent_dir());
		Searchpath ds (ARDOUR::ardour_data_search_path());
		ds.add_subdirectory_to_paths ("sessions");
		vector<string> demos;
		find_files_matching_pattern (demos, ds, ARDOUR::session_archive_suffix);

		ARDOUR::RecentSessions rs;
		ARDOUR::read_recent_sessions (rs);

		for (vector<string>::iterator i = demos.begin(); i != demos.end (); ++i) {
			/* "demo-session" must be inside "demo-session.<session_archive_suffix>" */
			std::string name = basename_nosuffix (basename_nosuffix (*i));
			std::string path = Glib::build_filename (dspd, name);
			/* skip if session-dir already exists */
			if (Glib::file_test(path.c_str(), Glib::FILE_TEST_IS_DIR)) {
				continue;
			}
			/* skip sessions that are already in 'recent'.
			 * eg. a new user changed <session-default-dir> shortly after installation
			 */
			for (ARDOUR::RecentSessions::iterator r = rs.begin(); r != rs.end(); ++r) {
				if ((*r).first == name) {
					continue;
				}
			}
			try {
				PBD::FileArchive ar (*i);
				if (0 == ar.inflate (dspd)) {
					store_recent_sessions (name, path);
					info << string_compose (_("Copied Demo Session %1."), name) << endmsg;
				}
			} catch (...) {
				/* relax ? */
			}
		}
	}
}

bool
StartupFSM::ask_about_loading_existing_session (const std::string& session_path)
{
	std::string str = string_compose (_("This session\n%1\nalready exists. Do you want to open it?"), session_path);

	ArdourMessageDialog msg (str,
	                         false,
	                         Gtk::MESSAGE_WARNING,
	                         Gtk::BUTTONS_YES_NO,
	                         true);


	msg.set_name (X_("OpenExistingDialog"));
	msg.set_title (_("Open Existing Session"));
	msg.set_wmclass (X_("existing_session"), PROGRAM_NAME);
	msg.set_position (Gtk::WIN_POS_CENTER);

	switch (msg.run()) {
	case RESPONSE_YES:
		return true;
		break;
	}
	return false;
}

void
StartupFSM::show_pre_release_dialog ()
{
	pre_release_dialog = new ArdourDialog  (_("Pre-Release Warning"), true, false);
	pre_release_dialog->add_button (Gtk::Stock::OK, Gtk::RESPONSE_OK);

	Label* label = manage (new Label);
/*
	label->set_markup (string_compose (_("<span size=\"x-large\" weight=\"bold\">Welcome to this pre-release build of %1 %2</span>\n\n\
<span size=\"large\">There are still several issues and bugs to be worked on,\n\
as well as general workflow improvements, before this can be considered\n\
release software. So, a few guidelines:\n\
*/
	label->set_markup (string_compose (_("<span size=\"x-large\" weight=\"bold\">Welcome to this very-much-not-ready build of %1 %2</span>\n\n\
<span size=\"large\">This is still very much a work-in-progress and many pre-existing\n\
editing features may be broken. In addition the functionality of the MIDI clip editing\n\
on the cue page is rapidly evolving, but if you are seeing this message it is not\n\
considered finished at this time.\n\
1) Please do <b>NOT</b> use this software with the expectation that it is stable or reliable\n\
   though it may be so, depending on your workflow.\n\
2) Please wait for a helpful writeup of new features.\n\
3) <b>Please do NOT use the forums at ardour.org to report issues</b>.\n\
4) <b>Please do NOT file bugs for this alpha-development versions at this point in time</b>.\n\
   There is no bug triaging before the initial development concludes and\n\
   reporting issue for incomplete, ongoing work-in-progress is mostly useless.\n\
5) Please <b>DO</b> join us on IRC for real time discussions about %1 %2. You\n\
   can get there directly from within the program via the Help -> Chat menu option.\n\
6) Please <b>DO</b> submit patches for issues after discussing them on IRC.\n\
\n\
Full information on all the above can be found on the support page at\n\
\n\
                http://ardour.org/support</span>\n\
"), PROGRAM_NAME, VERSIONSTRING));


	current_dialog_connection = pre_release_dialog->signal_response().connect (sigc::bind (sigc::mem_fun (this, &StartupFSM::dialog_response_handler), PreReleaseDialog));

	pre_release_dialog->get_vbox()->set_border_width (12);
	pre_release_dialog->get_vbox()->pack_start (*label, false, false, 12);
	pre_release_dialog->get_vbox()->show_all ();
	pre_release_dialog->set_position (WIN_POS_CENTER);
	pre_release_dialog->present ();
}

void
StartupFSM::handle_path (string const & path)
{
	if (!ARDOUR_COMMAND_LINE::session_name.empty()) {
		return;
	}

	ARDOUR_COMMAND_LINE::session_name = path;

	dialog_response_handler (RESPONSE_OK, ApplicationPseudoDialog);
}
