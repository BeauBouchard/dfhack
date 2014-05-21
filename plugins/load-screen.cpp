#include "Console.h"
#include "Core.h"
#include "DataDefs.h"
#include "Export.h"
#include "PluginManager.h"

#include <regex>
#include <set>

#include <modules/Gui.h>
#include <modules/Screen.h>
#include <VTableInterpose.h>

#include "ColorText.h"
#include "df/enabler.h"
#include "df/graphic.h"
#include "df/viewscreen_titlest.h"
#include "df/viewscreen_loadgamest.h"

#define LOAD_LIST_MAX_X 77

using namespace DFHack;

using df::global::enabler;
using df::global::gps;

typedef df::viewscreen_loadgamest::T_saves T_saves;

class viewscreen_load_screen;
class viewscreen_load_options;

void OutputString (int8_t fg, int x, int y, const std::string text);
void OutputStringX (int8_t fg, int &x, int y, const std::string text);
void OutputStringCenter (int8_t fg, int y, const std::string text);
std::string mode_string (df::game_type gametype);
command_result cmd_load_screen (color_ostream &out, std::vector <std::string> & parameters);

class SaveGame
{
public:
    SaveGame (T_saves save);
    T_saves save;
    df::game_type game_type;
    std::string fort_name;
    std::string world_name;
    int32_t year;
    std::string folder_name;
    std::string base_folder_name;
    bool is_backup;
};

class viewscreen_load_screen : public dfhack_viewscreen
{
public:
    void feed (std::set<df::interface_key> *input);
    void render ();
    void help () { };
    std::string getFocusString() { return "loadscreen"; };
    viewscreen_load_screen ();
    ~viewscreen_load_screen () { };
    df::viewscreen_loadgamest* parent_screen ();
    bool load_game (std::string folder);
    void dismiss ();
protected:
    int sel_idx;
    int sel_offset;
    std::vector<SaveGame*> save_folders;
    void init_save_folders ();
    void show_options ();
    bool mouse_select_save (int x, int y);
};

class viewscreen_load_options : public dfhack_viewscreen
{
public:
    void feed (std::set<df::interface_key> *input);
    void render ();
    void help () { };
    std::string getFocusString() { return "loadscreen/options"; };
    viewscreen_load_options (viewscreen_load_screen * parent, SaveGame * save);
    ~viewscreen_load_options () { };
protected:
    viewscreen_load_screen * parent;
    SaveGame * save;
    int width;
    int height;
    void do_load ();
    bool load_flag;
};

void OutputString (int8_t fg, int x, int y, const std::string text)
{
    Screen::paintString(Screen::Pen(' ', fg, COLOR_BLACK), x, y, text);
}
void OutputStringX (int8_t fg, int &x, int y, const std::string text)
{
    OutputString(fg, x, y, text);
    x += text.length();
}
void OutputStringCenter (int8_t fg, int y, const std::string text)
{
    int x = Screen::getWindowSize().x / 2 - text.length() / 2;
    OutputString(fg, x, y, text);
}

std::string mode_string (df::game_type gametype)
{
    switch (gametype)
    {
        case df::game_type::DWARF_MAIN:
            return "Fortress mode";
        case df::game_type::DWARF_RECLAIM:
            return "Reclaim fortress mode";
        case df::game_type::ADVENTURE_MAIN:
            return "Adventure mode";
        default:
            return "";
    }
}

SaveGame::SaveGame (T_saves save):
    save(save),
    is_backup(false)
{
    this->game_type = save.game_type;
    this->fort_name = save.fort_name;
    this->world_name = save.world_name;
    this->year = save.year;
    this->folder_name = this->base_folder_name = save.folder_name;
    int pos = 0;
    if ((pos = folder_name.find("-")) != std::string::npos)
    {
        this->base_folder_name = this->folder_name.substr(0, pos);
        this->is_backup = true;
    }
}

viewscreen_load_screen::viewscreen_load_screen ()
{
    sel_idx = 0;
    sel_offset = 0;
}

df::viewscreen_loadgamest* viewscreen_load_screen::parent_screen ()
{
    df::viewscreen * top = this->parent;
    VIRTUAL_CAST_VAR(parent_screen, df::viewscreen_loadgamest, top);
    return parent_screen;
}

void viewscreen_load_screen::init_save_folders ()
{
    std::vector<T_saves*> saves = parent_screen()->saves;
    for (auto iter = saves.begin(); iter != saves.end(); iter++)
    {
        T_saves * t_save = *iter;
        SaveGame * save = new SaveGame(*t_save);
        save_folders.push_back(save);
    }
}

bool viewscreen_load_screen::mouse_select_save (int x, int y)
{
    if (x < 2 || x > LOAD_LIST_MAX_X)
        return false;
    int index = (y / 2) - 1 + sel_offset;
    if (index < 0 || index >= save_folders.size())
        return false;
    sel_idx = index;
    return true;
}

void viewscreen_load_screen::show_options ()
{
    Screen::show(new viewscreen_load_options(this, save_folders[sel_idx]));
}

void viewscreen_load_screen::feed (std::set<df::interface_key> *input)
{
    if (input->count(df::interface_key::LEAVESCREEN))
    {
        this->dismiss();
        return;
    }
    else if (input->count(df::interface_key::CURSOR_DOWN))
    {
        sel_idx++;
        if (sel_idx >= save_folders.size())
            sel_idx = 0;
    }
    else if (input->count(df::interface_key::CURSOR_UP))
    {
        sel_idx--;
        if (sel_idx < 0)
            sel_idx = save_folders.size() - 1;
    }
    else if (input->count(df::interface_key::SELECT))
    {
        show_options();
    }

    if (enabler->tracking_on && enabler->mouse_lbut)
    {
        if (mouse_select_save(gps->mouse_x, gps->mouse_y))
            show_options();
    }
}

void viewscreen_load_screen::render ()
{
    if (!save_folders.size())
        init_save_folders();
    auto dim = Screen::getWindowSize();
    df::viewscreen_loadgamest* parent = parent_screen();
    if (!parent)
        return;
    Screen::clear();
    std::string title = "Load game (DFHack)";
    Screen::paintString(Screen::Pen(' ', COLOR_WHITE, COLOR_BLACK),
                        (dim.x / 2) - (title.length() / 2),
                        0,
                        title);
    auto games = save_folders;
    int max_rows = (dim.y / 2) - 1;
    sel_offset = sel_idx - (max_rows - 3);
    sel_offset = std::max(0, sel_offset);
    int row = 2, i = sel_offset;
    for (auto iter = games.begin() + sel_offset; iter != games.end() && row + 2 < dim.y; iter++, row += 2, i++)
    {
        SaveGame * save = *iter;
        color_value fg = COLOR_GREY;
        if (save->is_backup) fg = COLOR_RED;
        if (i == sel_idx) fg = (color_value)(fg + 8);
        color_value bg = (i == sel_idx) ? COLOR_GREEN : COLOR_BLACK;
        auto pen = Screen::Pen(' ', fg, bg);
        Screen::fillRect(pen, 2, row, LOAD_LIST_MAX_X, row + 1);
        Screen::paintString(pen, 2, row, save->fort_name + " - " + mode_string(save->game_type));
        Screen::paintString(pen, 3, row + 1, "Folder: " + save->folder_name);
        std::string world_name = save->world_name;
        Screen::paintString(pen, LOAD_LIST_MAX_X - world_name.length() + 1, row, world_name);
        std::string year = "Year " + std::to_string(save->year);
        Screen::paintString(pen, LOAD_LIST_MAX_X - year.length(), row + 1, year);
    }
    OutputStringCenter(COLOR_LIGHTCYAN, dim.y - 1, "\031 More \031");
    if (enabler->tracking_on && gps->mouse_x != -1 && gps->mouse_y != -1 &&
        Gui::getCurFocus() == "dfhack/loadscreen")
    {
        mouse_select_save(gps->mouse_x, gps->mouse_y);
    }
}

bool viewscreen_load_screen::load_game (std::string folder)
{
    df::viewscreen_loadgamest* parent = parent_screen();
    parent->sel_idx = 0;
    parent->saves[0]->folder_name = folder;
    parent->loading = 1;
    parent->logic();
    return true;
}

void viewscreen_load_screen::dismiss ()
{
    Screen::dismiss(this);
    Screen::dismiss(this->parent);
}

viewscreen_load_options::viewscreen_load_options (viewscreen_load_screen * parent, SaveGame * save):
    parent(parent),
    save(save),
    width(42),
    height(12),
    load_flag(false)
{ }

void viewscreen_load_options::feed (std::set<df::interface_key> *input)
{
    if (input->count(df::interface_key::LEAVESCREEN))
    {
        Screen::dismiss(this);
    }
    else if (input->count(df::interface_key::SELECT))
    {
        load_flag = true;
    }
}

void viewscreen_load_options::render ()
{
    if (load_flag)
    {
        Screen::clear();
        OutputString(COLOR_WHITE, 2, 2, "Loading game...");
        do_load();
        return;
    }
    parent->render();
    auto dim = Screen::getWindowSize();
    int min_x = dim.x / 2 - width / 2,
        max_x = dim.x / 2 + width / 2,
        min_y = dim.y / 2 - height / 2,
        max_y = dim.y / 2 + height / 2;
    Screen::fillRect(Screen::Pen(' ', COLOR_BLACK, COLOR_GREY), min_x, min_y, max_x, max_y);
    Screen::fillRect(Screen::Pen(' ', COLOR_BLACK, COLOR_BLACK),
                     min_x + 1, min_y + 1, max_x - 1, max_y - 1);
    std::string title = "Load game: " + this->save->folder_name;
    Screen::paintString(Screen::Pen(' ', COLOR_BLACK, COLOR_GREY),
                        dim.x / 2 - title.length() / 2, min_y, title);
    int x = min_x + 2,
        y = min_y + 2;
    OutputStringX(COLOR_LIGHTRED, x, y, Screen::getKeyDisplay(df::interface_key::SELECT));
    OutputStringX(COLOR_WHITE, x, y, ": Play now");
    x = min_x + 2; y = max_y - 2;
    OutputStringX(COLOR_LIGHTRED, x, y, Screen::getKeyDisplay(df::interface_key::LEAVESCREEN));
    OutputStringX(COLOR_WHITE, x, y, ": Back");
}

void viewscreen_load_options::do_load ()
{
    this->parent->load_game(save->folder_name);
    Screen::dismiss(this);
    this->parent->dismiss();
}

struct loadgame_hooks : df::viewscreen_loadgamest
{
    typedef df::viewscreen_loadgamest interpose_base;
    DEFINE_VMETHOD_INTERPOSE(void, render, ())
    {
        Screen::show(new viewscreen_load_screen);
    }
};

IMPLEMENT_VMETHOD_INTERPOSE(loadgame_hooks, render);

DFHACK_PLUGIN("load-screen");
DFHACK_PLUGIN_IS_ENABLED(is_enabled);

DFhackCExport command_result plugin_init ( color_ostream &out, std::vector <PluginCommand> &commands)
{
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    return CR_OK;
}

DFhackCExport command_result plugin_enable (color_ostream &out, bool enable)
{
    if (!gps)
        return CR_FAILURE;
    if (enable != is_enabled)
    {
        if (!INTERPOSE_HOOK(loadgame_hooks, render).apply(enable))
            return CR_FAILURE;
        is_enabled = enable;
    }
    return CR_OK;
}

