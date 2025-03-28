// Generated by gmmproc 2.45.3 -- DO NOT MODIFY!


#include <glibmm.h>

#include <ytkmm/imagemenuitem.h>
#include <ytkmm/private/imagemenuitem_p.h>


// -*- c++ -*-
/* $Id: imagemenuitem.ccg,v 1.2 2003/10/28 17:12:27 murrayc Exp $ */

/* 
 *
 * Copyright 1998-2002 The gtkmm Development Team
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

#include <ytk/ytk.h>

#include <ytkmm/image.h>
#include <ytkmm/stock.h>
#include <ytkmm/accellabel.h>

namespace Gtk
{

ImageMenuItem::ImageMenuItem(Widget& image, 
                             const Glib::ustring& label, bool mnemonic)
:
  // Mark this class as non-derived to allow C++ vfuncs to be skipped.
  Glib::ObjectBase(0),
  Gtk::MenuItem(Glib::ConstructParams(imagemenuitem_class_.init()))
{
  set_image(image);
  add_accel_label(label, mnemonic);
}

ImageMenuItem::ImageMenuItem(const Glib::ustring& label, bool mnemonic)
:
  // Mark this class as non-derived to allow C++ vfuncs to be skipped.
  Glib::ObjectBase(0),
  Gtk::MenuItem(Glib::ConstructParams(imagemenuitem_class_.init()))
{
  add_accel_label(label, mnemonic);
}

ImageMenuItem::ImageMenuItem(const Gtk::StockID& stock_id)
:
  // Mark this class as non-derived to allow C++ vfuncs to be skipped.
  Glib::ObjectBase(0),
  Gtk::MenuItem(Glib::ConstructParams(imagemenuitem_class_.init()))
{
  Gtk::Image* image = new Gtk::Image(stock_id, ICON_SIZE_MENU);
  image->show();
  set_image( *(Gtk::manage(image)) );

  Gtk::StockItem item;
  if(Gtk::Stock::lookup(stock_id, item))
  {
    add_accel_label(item.get_label(), true); //true = use mnemonic.
    set_accel_key( AccelKey(item.get_keyval(), item.get_modifier()) );
  }
  else
  {
    add_accel_label(stock_id.get_string(), false);
  }
}

} // namespace Gtk


namespace
{
} // anonymous namespace


namespace Glib
{

Gtk::ImageMenuItem* wrap(GtkImageMenuItem* object, bool take_copy)
{
  return dynamic_cast<Gtk::ImageMenuItem *> (Glib::wrap_auto ((GObject*)(object), take_copy));
}

} /* namespace Glib */

namespace Gtk
{


/* The *_Class implementation: */

const Glib::Class& ImageMenuItem_Class::init()
{
  if(!gtype_) // create the GType if necessary
  {
    // Glib::Class has to know the class init function to clone custom types.
    class_init_func_ = &ImageMenuItem_Class::class_init_function;

    // This is actually just optimized away, apparently with no harm.
    // Make sure that the parent type has been created.
    //CppClassParent::CppObjectType::get_type();

    // Create the wrapper type, with the same class/instance size as the base type.
    register_derived_type(gtk_image_menu_item_get_type());

    // Add derived versions of interfaces, if the C type implements any interfaces:

  }

  return *this;
}


void ImageMenuItem_Class::class_init_function(void* g_class, void* class_data)
{
  BaseClassType *const klass = static_cast<BaseClassType*>(g_class);
  CppClassParent::class_init_function(klass, class_data);


}


Glib::ObjectBase* ImageMenuItem_Class::wrap_new(GObject* o)
{
  return manage(new ImageMenuItem((GtkImageMenuItem*)(o)));

}


/* The implementation: */

ImageMenuItem::ImageMenuItem(const Glib::ConstructParams& construct_params)
:
  Gtk::MenuItem(construct_params)
{
  }

ImageMenuItem::ImageMenuItem(GtkImageMenuItem* castitem)
:
  Gtk::MenuItem((GtkMenuItem*)(castitem))
{
  }

ImageMenuItem::~ImageMenuItem()
{
  destroy_();
}

ImageMenuItem::CppClassType ImageMenuItem::imagemenuitem_class_; // initialize static member

GType ImageMenuItem::get_type()
{
  return imagemenuitem_class_.init().get_type();
}


GType ImageMenuItem::get_base_type()
{
  return gtk_image_menu_item_get_type();
}


ImageMenuItem::ImageMenuItem()
:
  // Mark this class as non-derived to allow C++ vfuncs to be skipped.
  Glib::ObjectBase(0),
  Gtk::MenuItem(Glib::ConstructParams(imagemenuitem_class_.init()))
{
  

}

void ImageMenuItem::set_always_show_image(bool always_show)
{
  gtk_image_menu_item_set_always_show_image(gobj(), static_cast<int>(always_show));
}

bool ImageMenuItem::get_always_show_image() const
{
  return gtk_image_menu_item_get_always_show_image(const_cast<GtkImageMenuItem*>(gobj()));
}

void ImageMenuItem::set_image(Widget& image)
{
  gtk_image_menu_item_set_image(gobj(), (image).gobj());
}

Widget* ImageMenuItem::get_image()
{
  return Glib::wrap(gtk_image_menu_item_get_image(gobj()));
}

const Widget* ImageMenuItem::get_image() const
{
  return const_cast<ImageMenuItem*>(this)->get_image();
}

void ImageMenuItem::set_use_stock(bool use_stock)
{
  gtk_image_menu_item_set_use_stock(gobj(), static_cast<int>(use_stock));
}

bool ImageMenuItem::get_use_stock() const
{
  return gtk_image_menu_item_get_use_stock(const_cast<GtkImageMenuItem*>(gobj()));
}

void ImageMenuItem::set_accel_group(const Glib::RefPtr<AccelGroup>& accel_group)
{
  gtk_image_menu_item_set_accel_group(gobj(), Glib::unwrap(accel_group));
}


Glib::PropertyProxy< Widget* > ImageMenuItem::property_image() 
{
  return Glib::PropertyProxy< Widget* >(this, "image");
}

Glib::PropertyProxy_ReadOnly< Widget* > ImageMenuItem::property_image() const
{
  return Glib::PropertyProxy_ReadOnly< Widget* >(this, "image");
}

Glib::PropertyProxy< bool > ImageMenuItem::property_use_stock() 
{
  return Glib::PropertyProxy< bool >(this, "use-stock");
}

Glib::PropertyProxy_ReadOnly< bool > ImageMenuItem::property_use_stock() const
{
  return Glib::PropertyProxy_ReadOnly< bool >(this, "use-stock");
}

Glib::PropertyProxy_WriteOnly< Glib::RefPtr<AccelGroup> > ImageMenuItem::property_accel_group() 
{
  return Glib::PropertyProxy_WriteOnly< Glib::RefPtr<AccelGroup> >(this, "accel-group");
}

Glib::PropertyProxy< bool > ImageMenuItem::property_always_show_image() 
{
  return Glib::PropertyProxy< bool >(this, "always-show-image");
}

Glib::PropertyProxy_ReadOnly< bool > ImageMenuItem::property_always_show_image() const
{
  return Glib::PropertyProxy_ReadOnly< bool >(this, "always-show-image");
}


} // namespace Gtk


