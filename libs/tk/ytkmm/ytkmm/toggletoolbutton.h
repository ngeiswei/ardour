// -*- c++ -*-
// Generated by gmmproc 2.45.3 -- DO NOT MODIFY!
#ifndef _GTKMM_TOGGLETOOLBUTTON_H
#define _GTKMM_TOGGLETOOLBUTTON_H


#include <glibmm/ustring.h>
#include <sigc++/sigc++.h>

/* $Id: toggletoolbutton.hg,v 1.5 2004/03/07 14:35:47 murrayc Exp $ */

/* box.h
 *
 * Copyright (C) 2003 The gtkmm Development Team
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

#include <ytkmm/toolbutton.h>


#ifndef DOXYGEN_SHOULD_SKIP_THIS
typedef struct _GtkToggleToolButton GtkToggleToolButton;
typedef struct _GtkToggleToolButtonClass GtkToggleToolButtonClass;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */


namespace Gtk
{ class ToggleToolButton_Class; } // namespace Gtk
namespace Gtk
{

/** A Gtk::ToolItem containing a toggle button.
 *
 * A ToggleToolButton is a Gtk::ToolItem that contains a toggle button.
 *
 * @ingroup Widgets
 */

class ToggleToolButton : public ToolButton
{
  public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  typedef ToggleToolButton CppObjectType;
  typedef ToggleToolButton_Class CppClassType;
  typedef GtkToggleToolButton BaseObjectType;
  typedef GtkToggleToolButtonClass BaseClassType;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

  virtual ~ToggleToolButton();

#ifndef DOXYGEN_SHOULD_SKIP_THIS

private:
  friend class ToggleToolButton_Class;
  static CppClassType toggletoolbutton_class_;

  // noncopyable
  ToggleToolButton(const ToggleToolButton&);
  ToggleToolButton& operator=(const ToggleToolButton&);

protected:
  explicit ToggleToolButton(const Glib::ConstructParams& construct_params);
  explicit ToggleToolButton(GtkToggleToolButton* castitem);

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  static GType get_type()      G_GNUC_CONST;


  static GType get_base_type() G_GNUC_CONST;
#endif

  ///Provides access to the underlying C GtkObject.
  GtkToggleToolButton*       gobj()       { return reinterpret_cast<GtkToggleToolButton*>(gobject_); }

  ///Provides access to the underlying C GtkObject.
  const GtkToggleToolButton* gobj() const { return reinterpret_cast<GtkToggleToolButton*>(gobject_); }


public:
  //C++ methods used to invoke GTK+ virtual functions:

protected:
  //GTK+ Virtual Functions (override these to change behaviour):

  //Default Signal Handlers::
  /// This is a default handler for the signal signal_toggled().
  virtual void on_toggled();


private:

public:

  /** Creates a new ToggleToolButton.
   */
  ToggleToolButton();
  
  /** Creates a new ToggleToolButton from a StockID.
   *
   * The ToggleToolButton will be created according to the @a stock_id properties.
   * 
   * @param stock_id The StockID which determines the look of the ToggleToolButton.
   */
    explicit ToggleToolButton(const Gtk::StockID& stock_id);

  
  /** Creates a new ToggleToolButton with a label.
   *
   * The ToggleToolButton will have the label @a label.
   *
   * @param label The string used to display the label for this ToggleToolButton.
   */
  explicit ToggleToolButton(const Glib::ustring& label);
  
  /** Creates a new ToggleToolButton with an image.
   *
   * The ToggleToolButton will have the label @a label and an image widget @a icon_widget.
   *
   * @param icon_widget The widget placed as the ToggleToolButton's icon.
   * @param label The string used to display the label for this ToggleToolButton.
   */
  explicit ToggleToolButton(Widget& icon_widget, const Glib::ustring& label = Glib::ustring());

  
  /** Sets the status of the toggle tool button. Set to <tt>true</tt> if you
   * want the GtkToggleButton to be 'pressed in', and <tt>false</tt> to raise it.
   * This action causes the toggled signal to be emitted.
   * 
   * @param is_active Whether @a button should be active.
   */
  void set_active(bool is_active =  true);
  
  /** Queries a Gtk::ToggleToolButton and returns its current state.
   * Returns <tt>true</tt> if the toggle button is pressed in and <tt>false</tt> if it is raised.
   * 
   * @return <tt>true</tt> if the toggle tool button is pressed in, <tt>false</tt> if not.
   */
  bool get_active() const;

  
  /**
   * @par Slot Prototype:
   * <tt>void on_my_%toggled()</tt>
   *
   */

  Glib::SignalProxy0< void > signal_toggled();

  
  /** If the toggle button should be pressed in or not.
   *
   * @return A PropertyProxy that allows you to get or set the value of the property,
   * or receive notification when the value of the property changes.
   */
  Glib::PropertyProxy< bool > property_active() ;

/** If the toggle button should be pressed in or not.
   *
   * @return A PropertyProxy_ReadOnly that allows you to get the value of the property,
   * or receive notification when the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly< bool > property_active() const;


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
   * @relates Gtk::ToggleToolButton
   */
  Gtk::ToggleToolButton* wrap(GtkToggleToolButton* object, bool take_copy = false);
} //namespace Glib


#endif /* _GTKMM_TOGGLETOOLBUTTON_H */

