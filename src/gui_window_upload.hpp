#ifndef H_GUI_WINDOW_UPLOAD
#define H_GUI_WINDOW_UPLOAD

//custom
#include "gui_window_download_status.hpp"
#include "p2p.hpp"
#include "settings.hpp"

//gui
#include <gtkmm.h>

class gui_window_upload : public Gtk::ScrolledWindow, private boost::noncopyable
{
public:
	gui_window_upload(p2p & P2P_in);

private:
	//convenience pointer
	Gtk::ScrolledWindow * window;

	//objects for display of uploads
	Gtk::TreeView * upload_view;
	Glib::RefPtr<Gtk::ListStore> upload_list;

	//columns
	Gtk::TreeModel::ColumnRecord column;
	Gtk::TreeModelColumn<Glib::ustring> column_hash;
	Gtk::TreeModelColumn<Glib::ustring> column_IP;
	Gtk::TreeModelColumn<Glib::ustring> column_name;
	Gtk::TreeModelColumn<Glib::ustring> column_size;
	Gtk::TreeModelColumn<Glib::ustring> column_speed;
	Gtk::TreeModelColumn<int> column_percent_complete;
	Gtk::CellRendererProgress * cell;

	//pointer to server that exists in gui class
	p2p * P2P;

	/*
	Signaled Functions
	upload_info_refresh - refreshes upload_view
	*/
	bool upload_info_refresh();
};
#endif
