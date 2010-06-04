#ifndef H_WINDOW_MAIN
#define H_WINDOW_MAIN

//custom
#include "settings.hpp"
#include "fixed_statusbar.hpp"
#include "window_prefs.hpp"
#include "window_transfer.hpp"

//include
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <convert.hpp>
#include <gtkmm.h>
#include <p2p.hpp>

class window_main : public Gtk::Window, private boost::noncopyable
{
public:
	window_main();

private:
	p2p P2P;

	/*
	file_drag_data_received:
		File dragged on to window.
	on_delete_event:
		Overrides event which happens on window close.
	process_URI:
		Processes URI of file dropped on window.
		Note: URI might be newline delimited list of URIs.
	*/
	void file_drag_data_received(const Glib::RefPtr<Gdk::DragContext> & context,
		int x, int y, const Gtk::SelectionData & selection_data, guint info, guint time);
	bool on_delete_event(GdkEventAny * event);
	void process_URI(const std::string & URI);
};
#endif
