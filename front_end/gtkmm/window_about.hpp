#ifndef H_WINDOW_ABOUT
#define H_WINDOW_ABOUT

//custom
#include "settings.hpp"

//include
#include <boost/utility.hpp>
#include <logger.hpp>
#include <p2p.hpp>
#include <gtkmm.h>

class window_about : public Gtk::Window, private boost::noncopyable
{
public:
	window_about();

private:
	Gtk::Window * window;
	Gtk::VBox * vbox;
	Gtk::ScrolledWindow * scrolled_window;
	Gtk::TextView * text_view;
	Glib::RefPtr<Gtk::TextBuffer> text_buff;
	Gtk::HButtonBox * button_box;
	Gtk::Button * close_button;

	/*
	close_click:
		Called when close button clicked.
	*/
	void close_click();
};
#endif
