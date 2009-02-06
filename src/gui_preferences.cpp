#include "gui_preferences.hpp"

gui_preferences::gui_preferences(client * Client_in, server * Server_in)
{
	Client = Client_in;
	Server = Server_in;

	window = this;
	download_directory_entry = Gtk::manage(new Gtk::Entry());
	share_directory_entry = Gtk::manage(new Gtk::Entry());
	max_download_rate_entry = Gtk::manage(new Gtk::Entry());
	max_upload_rate_entry = Gtk::manage(new Gtk::Entry());
	downloads_label = Gtk::manage(new Gtk::Label("Download Directory"));
	share_label = Gtk::manage(new Gtk::Label("Share Directory"));
	rate_label = Gtk::manage(new Gtk::Label("Speed (kB/s)"));
	connection_limit_label = Gtk::manage(new Gtk::Label("Connection Limit"));
	upload_rate_label = Gtk::manage(new Gtk::Label("UL"));
	download_rate_label = Gtk::manage(new Gtk::Label("DL"));
	apply_button = Gtk::manage(new Gtk::Button(Gtk::StockID("gtk-apply")));
	cancel_button = Gtk::manage(new Gtk::Button(Gtk::StockID("gtk-cancel")));
	ok_button = Gtk::manage(new Gtk::Button(Gtk::StockID("gtk-ok")));
	button_box = Gtk::manage(new Gtk::HButtonBox);
	client_connections_hscale = Gtk::manage(new Gtk::HScale(0,global::CLIENT_CONNECTIONS,1));
	server_connections_hscale = Gtk::manage(new Gtk::HScale(0,global::CLIENT_CONNECTIONS,1));
	fixed = Gtk::manage(new Gtk::Fixed());

	window->set_resizable(false);
	window->set_title("Preferences");
	window->set_modal(true);
	window->set_keep_above(true);
	window->set_position(Gtk::WIN_POS_CENTER);
	window->add(*fixed);

	download_directory_entry->set_size_request(320,23);
	download_directory_entry->set_text(Client->get_download_directory());

	share_directory_entry->set_size_request(320,23);
	share_directory_entry->set_text(Server->get_share_directory());

	std::stringstream ss;
	unsigned download_rate, upload_rate;
	ss << Client->get_download_rate();
	ss >> download_rate;
	download_rate /= 1024;
	ss.str(""); ss.clear();
	ss << download_rate;
	max_download_rate_entry->set_size_request(75,23);
	max_download_rate_entry->set_text(ss.str());

	ss.str(""); ss.clear();
	ss << Server->get_upload_rate();
	ss >> upload_rate;
	upload_rate /= 1024;
	ss.str(""); ss.clear();
	ss << upload_rate;
	max_upload_rate_entry->set_size_request(75,23);
	max_upload_rate_entry->set_text(ss.str());

	downloads_label->set_size_request(141,17);
	downloads_label->set_alignment(0.5,0.5);
	downloads_label->set_justify(Gtk::JUSTIFY_LEFT);
	downloads_label->set_line_wrap(false);
	downloads_label->set_use_markup(false);
	downloads_label->set_selectable(false);

	share_label->set_size_request(116,17);
	share_label->set_alignment(0.5,0.5);
	share_label->set_justify(Gtk::JUSTIFY_LEFT);
	share_label->set_line_wrap(false);
	share_label->set_use_markup(false);
	share_label->set_selectable(false);
	rate_label->set_size_request(142,17);
	rate_label->set_alignment(0.5,0.5);
	rate_label->set_justify(Gtk::JUSTIFY_LEFT);
	rate_label->set_line_wrap(false);
	rate_label->set_use_markup(false);
	rate_label->set_selectable(false);

	connection_limit_label->set_size_request(143,17);
	connection_limit_label->set_alignment(0.5,0.5);
	connection_limit_label->set_justify(Gtk::JUSTIFY_LEFT);
	connection_limit_label->set_line_wrap(false);
	connection_limit_label->set_use_markup(false);
	connection_limit_label->set_selectable(false);

	upload_rate_label->set_size_request(38,17);
	upload_rate_label->set_alignment(0.5,0.5);
	upload_rate_label->set_justify(Gtk::JUSTIFY_LEFT);
	upload_rate_label->set_line_wrap(false);
	upload_rate_label->set_use_markup(false);
	upload_rate_label->set_selectable(false);

	download_rate_label->set_size_request(38,17);
	download_rate_label->set_alignment(0.5,0.5);
	download_rate_label->set_justify(Gtk::JUSTIFY_LEFT);
	download_rate_label->set_line_wrap(false);
	download_rate_label->set_use_markup(false);
	download_rate_label->set_selectable(false);

	client_connections_hscale->set_size_request(200,37);
	client_connections_hscale->set_draw_value(true);
	client_connections_hscale->set_value_pos(Gtk::POS_TOP);
	client_connections_hscale->set_value(Client->get_max_connections());

	server_connections_hscale->set_size_request(200,37);
	server_connections_hscale->set_draw_value(true);
	server_connections_hscale->set_value_pos(Gtk::POS_TOP);
	server_connections_hscale->set_value(Server->get_max_connections());

	button_box->set_spacing(8);
	button_box->set_border_width(8);
	button_box->pack_start(*apply_button);
	button_box->pack_start(*cancel_button);
	button_box->pack_start(*ok_button);

	fixed->put(*download_directory_entry, 16, 40);
	fixed->put(*share_directory_entry, 16, 104);
	fixed->put(*max_download_rate_entry, 40, 184);
	fixed->put(*max_upload_rate_entry, 40, 224);
	fixed->put(*downloads_label, 8, 16);
	fixed->put(*share_label, 8, 80);
	fixed->put(*rate_label, 4, 152);
	fixed->put(*connection_limit_label, 140, 152);
	fixed->put(*download_rate_label, 8, 187);
	fixed->put(*upload_rate_label, 8, 227);
	fixed->put(*client_connections_hscale, 140, 171);
	fixed->put(*server_connections_hscale, 140, 211);
	fixed->put(*button_box, 80, 256);

	show_all_children();

	//signaled functions
	apply_button->signal_clicked().connect(sigc::mem_fun(*this, &gui_preferences::apply_click), false);
	cancel_button->signal_clicked().connect(sigc::mem_fun(*this, &gui_preferences::cancel_click), false);
	ok_button->signal_clicked().connect(sigc::mem_fun(*this, &gui_preferences::ok_click), false);
	client_connections_hscale->signal_value_changed().connect(sigc::mem_fun(*this, &gui_preferences::client_connections_changed), false);
	server_connections_hscale->signal_value_changed().connect(sigc::mem_fun(*this, &gui_preferences::server_connections_changed), false);
}

void gui_preferences::apply_click()
{
	apply_settings();
}

void gui_preferences::apply_settings()
{
	int download_rate, upload_rate;
	std::stringstream ss;
	ss << max_download_rate_entry->get_text();
	ss >> download_rate;
	ss.str(""); ss.clear();
	ss << max_upload_rate_entry->get_text();
	ss >> upload_rate;
	ss.str(""); ss.clear();

	/*
	Maintain minimum of 1 to 256 ratio of upload to download. Without this
	downloads can starve because they don't have enough upload to make requests.
	*/
	if(upload_rate != 0 && upload_rate < (unsigned)((float)download_rate / 256 + 0.5)){
		upload_rate = (unsigned)((float)download_rate / 256 + 0.5);
		ss << upload_rate;
		max_upload_rate_entry->set_text(ss.str());
	}

	if(download_rate == 0){
		upload_rate = 0;
		max_upload_rate_entry->set_text("0");
	}

	Client->set_download_directory(download_directory_entry->get_text());
	Client->set_max_download_rate(download_rate * 1024);
	Client->set_max_connections((int)client_connections_hscale->get_value());
	Server->set_share_directory(share_directory_entry->get_text());
	Server->set_max_upload_rate(upload_rate * 1024);
	Server->set_max_connections((int)server_connections_hscale->get_value());
}

void gui_preferences::cancel_click()
{
	hide();
}

void gui_preferences::client_connections_changed()
{
	int client = (int)client_connections_hscale->get_value();
	int server = (int)server_connections_hscale->get_value();
	if(server < client){
		server_connections_hscale->set_value(client);
	}
}

void gui_preferences::server_connections_changed()
{
	int client = (int)client_connections_hscale->get_value();
	int server = (int)server_connections_hscale->get_value();
	if(server < client){
		client_connections_hscale->set_value(server);
	}
}

void gui_preferences::ok_click()
{
	apply_settings();
	hide();
}