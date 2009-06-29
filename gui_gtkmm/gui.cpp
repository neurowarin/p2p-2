#include "gui.hpp"

gui::gui():
	Gtk::Window(Gtk::WINDOW_TOPLEVEL)
{
	window = this;

	menubar = Gtk::manage(new Gtk::MenuBar);
	notebook = Gtk::manage(new Gtk::Notebook);

	//menus
	file_menu = Gtk::manage(new Gtk::Menu);
	settings_menu = Gtk::manage(new Gtk::Menu);
	help_menu = Gtk::manage(new Gtk::Menu);

	//notebook labels
	Gtk::Image * search_label = Gtk::manage(new Gtk::Image(Gtk::Stock::FIND,
		Gtk::ICON_SIZE_LARGE_TOOLBAR));
	Gtk::Image * download_label = Gtk::manage(new Gtk::Image(Gtk::Stock::GO_DOWN,
		Gtk::ICON_SIZE_LARGE_TOOLBAR));
	Gtk::Image * upload_label = Gtk::manage(new Gtk::Image(Gtk::Stock::GO_UP,
		Gtk::ICON_SIZE_LARGE_TOOLBAR));

	VBox_Search = Gtk::manage(new vbox_search(P2P));
	Window_Download = Gtk::manage(new window_download(P2P, notebook));
	Window_Upload = Gtk::manage(new window_upload(P2P));
	statusbar = Gtk::manage(new statusbar_main(P2P));

	//boxes (divides the window)
	main_VBox = Gtk::manage(new Gtk::VBox(false, 0));

	//icon for top left of window
	window->set_icon(
		Gtk::Widget::render_icon(Gtk::Stock::NETWORK, Gtk::ICON_SIZE_LARGE_TOOLBAR)
	);

	//add items to File menu
	file_menu->items().push_back(Gtk::Menu_Helpers::StockMenuElem(Gtk::StockID(Gtk::Stock::QUIT)));
	quit = static_cast<Gtk::ImageMenuItem *>(&file_menu->items().back());
	menubar->items().push_back(Gtk::Menu_Helpers::MenuElem(("_File"), *file_menu));
	file_menu_item = static_cast<Gtk::MenuItem *>(&menubar->items().back());
	menubar->items().push_back(Gtk::Menu_Helpers::MenuElem(("_Settings"), *settings_menu));
	settings_menu_item = static_cast<Gtk::MenuItem *>(&menubar->items().back());
	menubar->items().push_back(Gtk::Menu_Helpers::MenuElem(("_Help"), *help_menu));
	help_menu_item = static_cast<Gtk::MenuItem *>(&menubar->items().back());

	//add items to Settings menu
	settings_menu->items().push_back(Gtk::Menu_Helpers::StockMenuElem(
		Gtk::StockID(Gtk::Stock::PREFERENCES)));
	preferences = static_cast<Gtk::MenuItem *>(&settings_menu->items().back());

	//add items to Help menu
	help_menu->items().push_back(Gtk::Menu_Helpers::StockMenuElem(Gtk::StockID(Gtk::Stock::ABOUT)));
	about = static_cast<Gtk::MenuItem *>(&help_menu->items().back());

	//set notebook properties
	notebook->set_flags(Gtk::CAN_FOCUS);
	notebook->set_show_tabs(true);
	notebook->set_show_border(true);
	notebook->set_tab_pos(Gtk::POS_TOP);
	notebook->set_scrollable(true);

	//add elements to the notebook
	notebook->append_page(*VBox_Search, *search_label);
	notebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);
	notebook->append_page(*Window_Download, *download_label);
	notebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);
	notebook->append_page(*Window_Upload, *upload_label);
	notebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);

	//add items to the main VBox
	main_VBox->pack_start(*menubar, Gtk::PACK_SHRINK, 0);
	main_VBox->pack_start(*notebook);
	main_VBox->pack_start(*statusbar, Gtk::PACK_SHRINK, 0);

	//window properties
	window->set_title(settings::NAME);
	window->resize(640, 480);
	window->set_modal(false);
	window->property_window_position().set_value(Gtk::WIN_POS_CENTER_ON_PARENT);
	window->set_resizable(true);
	window->property_destroy_with_parent().set_value(false);
	window->add(*main_VBox);

	//signaled functions
	quit->signal_activate().connect(sigc::mem_fun(*this, &gui::on_quit), false);
	preferences->signal_activate().connect(sigc::mem_fun(*this, &gui::settings_preferences), false);
	about->signal_activate().connect(sigc::mem_fun(*this, &gui::help_about), false);

	//set objects to be visible
	window->show_all_children();
}

gui::~gui()
{

}

void gui::on_quit()
{
	hide();
}

bool gui::on_delete_event(GdkEventAny* event)
{
	on_quit();
	return true;
}

void gui::help_about()
{
	boost::shared_ptr<window_about> Window_About(new window_about());
	Gtk::Main::run(*Window_About);
}

void gui::settings_preferences()
{
	boost::shared_ptr<window_preferences> prefs(new window_preferences(P2P));
	Gtk::Main::run(*prefs);
}
