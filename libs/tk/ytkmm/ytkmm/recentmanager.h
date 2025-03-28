// -*- c++ -*-
// Generated by gmmproc 2.45.3 -- DO NOT MODIFY!
#ifndef _GTKMM_RECENTMANAGER_H
#define _GTKMM_RECENTMANAGER_H

#include <ytkmm/ytkmmconfig.h>


#include <glibmm/ustring.h>
#include <sigc++/sigc++.h>

/* Copyright (C) 2006 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

 
#include <ydkmm/screen.h>
#include <ydkmm/pixbuf.h>

#include <ytkmm/recentinfo.h>

#include <glibmm/object.h>
#include <glibmm/containers.h>


#ifndef DOXYGEN_SHOULD_SKIP_THIS
typedef struct _GtkRecentManager GtkRecentManager;
typedef struct _GtkRecentManagerClass GtkRecentManagerClass;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */


#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace Gtk
{ class RecentManager_Class; } // namespace Gtk
#endif //DOXYGEN_SHOULD_SKIP_THIS

namespace Gtk
{

/** Exception class for Gtk::RecentManager errors.
 */
class RecentManagerError : public Glib::Error
{
public:
  /** 
   */
  enum Code
  {
    NOT_FOUND,
    INVALID_URI,
    INVALID_ENCODING,
    NOT_REGISTERED,
    READ,
    WRITE,
    UNKNOWN
  };

  RecentManagerError(Code error_code, const Glib::ustring& error_message);
  explicit RecentManagerError(GError* gobject);
  Code code() const;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
private:

  static void throw_func(GError* gobject);

  friend void wrap_init(); // uses throw_func()

  #endif //DOXYGEN_SHOULD_SKIP_THIS
};

} // namespace Gtk

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace Glib
{

template <>
class Value<Gtk::RecentManagerError::Code> : public Glib::Value_Enum<Gtk::RecentManagerError::Code>
{
public:
  static GType value_type() G_GNUC_CONST;
};

} // namespace Glib
#endif /* DOXYGEN_SHOULD_SKIP_THIS */


namespace Gtk
{


/** @defgroup RecentFiles RecentFiles
 */

/** RecentManager provides a facility for adding, removing and
 * looking up recently used files.  Each recently used file is
 * identified by its URI, and has meta-data associated to it, like
 * the names and command lines of the applications that have
 * registered it, the number of time each application has registered
 * the same file, the mime type of the file and whether the file
 * should be displayed only by the applications that have
 * registered it.
 *
 * The RecentManager acts like a database of all the recently
 * used files.  You can create new RecentManager objects, but
 * it is more efficient to use the standard recent manager for
 * the Gdk::Screen so that informations about the recently used
 * files is shared with other people using them. Normally, you
 * retrieve the recent manager for a particular screen using
 * get_for_screen() and it will contain information about current
 * recent manager for that screen.
 *
 * @ingroup RecentFiles
 */

class RecentManager : public Glib::Object
{
  
#ifndef DOXYGEN_SHOULD_SKIP_THIS

public:
  typedef RecentManager CppObjectType;
  typedef RecentManager_Class CppClassType;
  typedef GtkRecentManager BaseObjectType;
  typedef GtkRecentManagerClass BaseClassType;

private:  friend class RecentManager_Class;
  static CppClassType recentmanager_class_;

private:
  // noncopyable
  RecentManager(const RecentManager&);
  RecentManager& operator=(const RecentManager&);

protected:
  explicit RecentManager(const Glib::ConstructParams& construct_params);
  explicit RecentManager(GtkRecentManager* castitem);

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

public:
  virtual ~RecentManager();

  /** Get the GType for this class, for use with the underlying GObject type system.
   */
  static GType get_type()      G_GNUC_CONST;

#ifndef DOXYGEN_SHOULD_SKIP_THIS


  static GType get_base_type() G_GNUC_CONST;
#endif

  ///Provides access to the underlying C GObject.
  GtkRecentManager*       gobj()       { return reinterpret_cast<GtkRecentManager*>(gobject_); }

  ///Provides access to the underlying C GObject.
  const GtkRecentManager* gobj() const { return reinterpret_cast<GtkRecentManager*>(gobject_); }

  ///Provides access to the underlying C instance. The caller is responsible for unrefing it. Use when directly setting fields in structs.
  GtkRecentManager* gobj_copy();

private:


protected:
  RecentManager();

public:
  
  static Glib::RefPtr<RecentManager> create();


  /** Gets a unique instance of Gtk::RecentManager, that you can share
   * in your application without caring about memory management. The
   * returned instance will be freed when you application terminates.
   * 
   * @return A unique Gtk::RecentManager. Do not ref or unref it.
   */
  static Glib::RefPtr<RecentManager> get_default();
  
#ifndef GTKMM_DISABLE_DEPRECATED

  /** Gets the recent manager object associated with @a screen; if this
   * function has not previously been called for the given screen,
   * a new recent manager object will be created and associated with
   * the screen. Recent manager objects are fairly expensive to create,
   * so using this function is usually a better choice than calling 
   * new() and setting the screen yourself; by using
   * this function a single recent manager object will be shared between
   * users.
   * 
   * Deprecated: 2.12: This function has been deprecated and should
   * not be used in newly written code. Calling this function is
   * equivalent to calling get_default().
   * 
   * @param screen A Gdk::Screen.
   * @return A unique Gtk::RecentManager associated with the given
   * screen. This recent manager is associated to the with the screen
   * and can be used as long as the screen is open. Do not ref or
   * unref it.
   */
  static Glib::RefPtr<RecentManager> get_for_screen(const Glib::RefPtr<Gdk::Screen>& screen);
#endif // GTKMM_DISABLE_DEPRECATED


  /** Meta-data passed to add_item().  You should
   * use RecentManager::Data if you want to control the meta-data associated
   * to an entry of the recently used files list when you are adding
   * a new file to it.
   *
   * - display_name: the string to be used when displaying the file inside a RecentChooser
   * - description: a user readable description of the file
   * - mime_type: the mime type of the file
   * - app_name: the name of the application that is registering the file
   * - app_exec: the command line that should be used when launching the file
   * - groups: the list of group names to which the file belongs to
   * - is_private: whether the file should be displayed only by the applications that have registered it
  */
  class Data
  {
  public:
    Glib::ustring display_name;
    Glib::ustring description;

    Glib::ustring mime_type;

    Glib::ustring app_name;
    Glib::ustring app_exec;

    std::vector<Glib::ustring> groups;

    bool is_private;
  };

  
#ifndef GTKMM_DISABLE_DEPRECATED

  /** Sets the screen for a recent manager; the screen is used to
   * track the user's currently configured recently used documents
   * storage.
   * 
   * Deprecated: 2.12: This function has been deprecated and should
   * not be used in newly written code. Calling this function has
   * no effect.
   * 
   * @param screen A Gdk::Screen.
   */
  void set_screen(const Glib::RefPtr<Gdk::Screen>& screen);
#endif // GTKMM_DISABLE_DEPRECATED


  /** Adds a new resource into the recently used resources list. This function
   * will try and guess some of the meta-data associated to a URI. If you
   * know some of meta-data about the document yourself, set the desired
   * fields of a RecentManager::Data structure and pass it to add_item().
   */
  bool add_item(const Glib::ustring& uri);

  /** Adds a new resource into the recently used resources list, taking
   * meta data from the given Data instead of guessing it from the URI.
   */
  bool add_item(const Glib::ustring& uri, const Data& recent_data);

  
  /** Removes a resource pointed by @a uri from the recently used resources
   * list handled by a recent manager.
   * 
   * @param uri The URI of the item you wish to remove.
   * @return <tt>true</tt> if the item pointed by @a uri has been successfully
   * removed by the recently used resources list, and <tt>false</tt> otherwise.
   */
  bool remove_item(const Glib::ustring& uri);
  
  /** Searches for a URI inside the recently used resources list, and
   * returns a structure containing informations about the resource
   * like its MIME type, or its display name.
   * 
   * @param uri A URI.
   * @return A Gtk::RecentInfo structure containing information
   * about the resource pointed by @a uri, or <tt>0</tt> if the URI was
   * not registered in the recently used resources list.  Free with
   * Gtk::RecentInfo::unref().
   */
  Glib::RefPtr<RecentInfo> lookup_item(const Glib::ustring& uri);
  
  /** Searches for a URI inside the recently used resources list, and
   * returns a structure containing informations about the resource
   * like its MIME type, or its display name.
   * 
   * @param uri A URI.
   * @return A Gtk::RecentInfo structure containing information
   * about the resource pointed by @a uri, or <tt>0</tt> if the URI was
   * not registered in the recently used resources list.  Free with
   * Gtk::RecentInfo::unref().
   */
  Glib::RefPtr<const RecentInfo> lookup_item(const Glib::ustring& uri) const;
  
  /** Checks whether there is a recently used resource registered
   * with @a uri inside the recent manager.
   * 
   * @param uri A URI.
   * @return <tt>true</tt> if the resource was found, <tt>false</tt> otherwise.
   */
  bool has_item(const Glib::ustring& uri) const;
  
  /** Changes the location of a recently used resource from @a uri to @a new_uri.
   * 
   * Please note that this function will not affect the resource pointed
   * by the URIs, but only the URI used in the recently used resources list.
   * 
   * @param uri The URI of a recently used resource.
   * @param new_uri The new URI of the recently used resource, or <tt>0</tt> to
   * remove the item pointed by @a uri in the list.
   * @return <tt>true</tt> on success.
   */
  bool move_item(const Glib::ustring& uri, const Glib::ustring& new_uri);
  
  /** Sets the maximum number of item that the get_items()
   * function should return.  If @a limit is set to -1, then return all the
   * items.
   * 
   * Deprecated: 2.22: The length of the list should be managed by the
   * view (implementing Gtk::RecentChooser), and not by the model (the
   * Gtk::RecentManager). See Gtk::RecentChooser::property_limit().
   * 
   * @param limit The maximum number of items to return, or -1.
   */
  void set_limit(int limit);
  
  /** Gets the maximum number of items that the get_items()
   * function should return.
   * 
   * Deprecated: 2.22: The length of the list should be managed by the
   * view (implementing Gtk::RecentChooser), and not by the model (the
   * Gtk::RecentManager). See Gtk::RecentChooser::property_limit().
   * 
   * @return The number of items to return, or -1 for every item.
   */
  int get_limit() const;

  typedef Glib::ListHandle<RecentInfo, RecentInfoTraits> ListHandle_RecentInfos;
  

  /** Gets the list of recently used resources.
   * 
   * @return A list of
   * newly allocated Gtk::RecentInfo objects. Use
   * Gtk::RecentInfo::unref() on each item inside the list, and then
   * free the list itself using Glib::list_free().
   */
  ListHandle_RecentInfos get_items() const;

  
  /** Purges every item from the recently used resources list.
   * 
   * @return The number of items that have been removed from the
   * recently used resources list.
   */
  int purge_items();

  /// For instance, void on_changed();
  typedef sigc::slot<void> SlotChanged;

  
  /** The "changed" signal is emitted when an item in the recently used resources list is changed.
   *
   * @par Slot Prototype:
   * <tt>void on_my_%changed()</tt>
   *
   */

  Glib::SignalProxy0< void > signal_changed();


  /** The full path to the file to be used to store and read the list.
   *
   * @return A PropertyProxy_ReadOnly that allows you to get the value of the property,
   * or receive notification when the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly< Glib::ustring > property_filename() const;


  /** The maximum number of items to be returned by gtk_recent_manager_get_items().
   *
   * @return A PropertyProxy that allows you to get or set the value of the property,
   * or receive notification when the value of the property changes.
   */
  Glib::PropertyProxy< int > property_limit() ;

/** The maximum number of items to be returned by gtk_recent_manager_get_items().
   *
   * @return A PropertyProxy_ReadOnly that allows you to get the value of the property,
   * or receive notification when the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly< int > property_limit() const;

  /** The size of the recently used resources list.
   *
   * @return A PropertyProxy_ReadOnly that allows you to get the value of the property,
   * or receive notification when the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly< int > property_size() const;


public:

public:
  //C++ methods used to invoke GTK+ virtual functions:

protected:
  //GTK+ Virtual Functions (override these to change behaviour):

  //Default Signal Handlers::
  /// This is a default handler for the signal signal_changed().
  virtual void on_changed();


};

} // namespace Gtk


namespace Glib
{
  /** A Glib::wrap() method for this object.
   * 
   * @param object The C instance.
   * @param take_copy False if the result should take ownership of the C instance. True if it should take a new copy or ref.
   * @result A C++ instance that wraps this C instance.
   *
   * @relates Gtk::RecentManager
   */
  Glib::RefPtr<Gtk::RecentManager> wrap(GtkRecentManager* object, bool take_copy = false);
}


#endif /* _GTKMM_RECENTMANAGER_H */

