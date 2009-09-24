#ifndef H_WINDOW_DOWNLOAD
#define H_WINDOW_DOWNLOAD

//custom
#include "settings.hpp"

//include
#include <convert.hpp>
#include <gtkmm.h>
#include <logger.hpp>
#include <p2p.hpp>

//standard
#include <set>
#include <string>

class window_download : public Gtk::ScrolledWindow, private boost::noncopyable
{
public:
	window_download(
		p2p & P2P_in,
		Gtk::Notebook * notebook_in
	);

private:
	p2p & P2P;

	//convenience pointer
	Gtk::ScrolledWindow * window;

	//the same notebook that exists in GUI
	Gtk::Notebook * notebook;

	//objects for display of downloads
	Gtk::TreeView * download_view;
	Gtk::ScrolledWindow * download_scrolled_window;
	Glib::RefPtr<Gtk::ListStore> download_list;

	//treeview columns
	Gtk::TreeModel::ColumnRecord column;
	Gtk::TreeModelColumn<Glib::ustring> column_hash;
	Gtk::TreeModelColumn<Glib::ustring> column_name;
	Gtk::TreeModelColumn<Glib::ustring> column_size;
	Gtk::TreeModelColumn<Glib::ustring> column_peers;
	Gtk::TreeModelColumn<Glib::ustring> column_speed;
	Gtk::TreeModelColumn<int> column_percent_complete;
	Gtk::CellRendererProgress * cell; //percent renderer

	//popup menus for when user right clicks on treeviews
	Gtk::Menu downloads_popup_menu;

	//hashes of all opened tabs
	std::set<std::string> open_info_tabs;

	/*
	compare_size:
		Signaled when user clicks size or speed column to sort.
	delete_download:
		Removes selected download.
	download_click:
		Called when download_view clicked.
	download_info_tab:
		Sets up a download_window_download_status tab for the selected download.
	download_info_refresh:
		Refreshes the tree view.
	pause_download:
		Pauses the selected download.
	*/
	int compare_size(const Gtk::TreeModel::iterator & lval,
		const Gtk::TreeModel::iterator & rval);
	void delete_download();
	bool download_click(GdkEventButton * event);
	void download_info_tab();
	bool download_info_refresh();
	void pause_download();
};
#endif
