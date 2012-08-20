/*
Copyright © 2011-2012 Clint Bellanger

This file is part of FLARE.

FLARE is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

FLARE is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
FLARE.  If not, see http://www.gnu.org/licenses/
*/

/**
 * Save and Load functions for the GameStatePlay.
 *
 * I put these in a separate cpp file just to keep GameStatePlay.cpp devoted to its core.
 *
 * class GameStatePlay
 */

#include "Avatar.h"
#include "CampaignManager.h"
#include "FileParser.h"
#include "GameStatePlay.h"
#include "MapRenderer.h"
#include "Menu.h"
#include "MenuActionBar.h"
#include "MenuCharacter.h"
#include "MenuInventory.h"
#include "MenuManager.h"
#include "MenuPowers.h"
#include "MenuStash.h"
#include "MenuTalker.h"
#include "Settings.h"
#include "UtilsFileSystem.h"
#include "UtilsParsing.h"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

/**
 * Before exiting the game, save to file
 */
void GameStatePlay::saveGame() {

	// game slots are currently 1-4
	if (game_slot == 0) return;

	ofstream outfile;

	stringstream ss;
	ss.str("");
	ss << PATH_USER << "save" << game_slot << ".txt";

	outfile.open(ss.str().c_str(), ios::out);

	if (outfile.is_open()) {

		// hero name
		outfile << "name=" << pc->stats.name << "\n";

		// permadeath
		outfile << "permadeath=" << pc->stats.permadeath << "\n";

		// hero visual option
		outfile << "option=" << pc->stats.base << "," << pc->stats.head << "," << pc->stats.portrait << "\n";

		// current experience
		outfile << "xp=" << pc->stats.xp << "\n";

		// hp and mp
		if (SAVE_HPMP) outfile << "hpmp=" << pc->stats.hp << "," << pc->stats.mp << "\n";

		// stat spec
		outfile << "build=" << pc->stats.physical_character << "," << pc->stats.mental_character << "," << pc->stats.offense_character << "," << pc->stats.defense_character << "\n";

		// current gold
		outfile << "gold=" << menu->inv->gold << "\n";

		// equipped gear
		outfile << "equipped=" << menu->inv->inventory[EQUIPMENT].getItems() << "\n";
		outfile << "equipped_quantity=" << menu->inv->inventory[EQUIPMENT].getQuantities() << "\n";

		// carried items
		outfile << "carried=" << menu->inv->inventory[CARRIED].getItems() << "\n";
		outfile << "carried_quantity=" << menu->inv->inventory[CARRIED].getQuantities() << "\n";

		// spawn point
		outfile << "spawn=" << map->respawn_map << "," << map->respawn_point.x/UNITS_PER_TILE << "," << map->respawn_point.y/UNITS_PER_TILE << "\n";

		// action bar
		outfile << "actionbar=";
		for (int i=0; i<12; i++) {
			if (pc->stats.transformed) outfile << menu->act->actionbar[i];
			else outfile << menu->act->hotkeys[i];
			if (i<11) outfile << ",";
		}
		outfile << "\n";

		//shapeshifter value
		if (pc->stats.transform_type == "untransform") outfile << "transformed=" << "\n";
		else outfile << "transformed=" << pc->stats.transform_type << "\n";

		// enabled powers
		outfile << "powers=";
		for (unsigned int i=0; i<menu->pow->powers_list.size(); i++) {
			if (i == 0) outfile << menu->pow->powers_list[i];
			else outfile << "," << menu->pow->powers_list[i];
		}
		outfile << "\n";

		// campaign data
		outfile << "campaign=";
		outfile << camp->getAll();

		outfile << endl;

		outfile.close();
	}

	// Save stash
	ss.str("");
	ss << PATH_USER << "stash.txt";

	outfile.open(ss.str().c_str(), ios::out);

	if (outfile.is_open()) {
		outfile << "item=" << menu->stash->stock.getItems() << "\n";
		outfile << "quantity=" << menu->stash->stock.getQuantities() << "\n";

		outfile << endl;

		outfile.close();
	}
}

/**
 * When loading the game, load from file if possible
 */
void GameStatePlay::loadGame() {
	int saved_hp = 0;
	int saved_mp = 0;

	// game slots are currently 1-4
	if (game_slot == 0) return;

	FileParser infile;
	int hotkeys[12];

	for (int i=0; i<12; i++) {
		hotkeys[i] = -1;
	}

	stringstream ss;
	ss.str("");
	ss << PATH_USER << "save" << game_slot << ".txt";

	if (infile.open(ss.str())) {
		while (infile.next()) {
			if (infile.key == "name") pc->stats.name = infile.val;
			else if (infile.key == "permadeath") {
				pc->stats.permadeath = toInt(infile.val);
			}
			else if (infile.key == "option") {
				pc->stats.base = infile.nextValue();
				pc->stats.head = infile.nextValue();
				pc->stats.portrait = infile.nextValue();
			}
			else if (infile.key == "xp") pc->stats.xp = toInt(infile.val);
			else if (infile.key == "hpmp") {
				saved_hp = toInt(infile.nextValue());
				saved_mp = toInt(infile.nextValue());
			}
			else if (infile.key == "build") {
				pc->stats.physical_character = toInt(infile.nextValue());
				pc->stats.mental_character = toInt(infile.nextValue());
				pc->stats.offense_character = toInt(infile.nextValue());
				pc->stats.defense_character = toInt(infile.nextValue());
			}
			else if (infile.key == "gold") {
				menu->inv->gold = toInt(infile.val);
			}
			else if (infile.key == "equipped") {
				menu->inv->inventory[EQUIPMENT].setItems(infile.val);
			}
			else if (infile.key == "equipped_quantity") {
				menu->inv->inventory[EQUIPMENT].setQuantities(infile.val);
				menu->inv->inventory[EQUIPMENT].fillEquipmentSlots();
			}
			else if (infile.key == "carried") {
				menu->inv->inventory[CARRIED].setItems(infile.val);
			}
			else if (infile.key == "carried_quantity") {
				menu->inv->inventory[CARRIED].setQuantities(infile.val);
			}
			else if (infile.key == "spawn") {
				map->teleport_mapname = infile.nextValue();

				if (fileExists(mods->locate("maps/" + map->teleport_mapname))) {
					map->teleport_destination.x = toInt(infile.nextValue()) * UNITS_PER_TILE + UNITS_PER_TILE/2;
					map->teleport_destination.y = toInt(infile.nextValue()) * UNITS_PER_TILE + UNITS_PER_TILE/2;
					map->teleportation = true;

					// prevent spawn.txt from putting us on the starting map
					map->clearEvents();
				}
				else {
					map->teleport_mapname = "spawn.txt";
					map->teleport_destination.x = 1;
					map->teleport_destination.y = 1;
					map->teleportation = true;

				}
			}
			else if (infile.key == "actionbar") {
				for (int i=0; i<12; i++)
					hotkeys[i] = toInt(infile.nextValue());
				menu->act->set(hotkeys);
			}
			else if (infile.key == "transformed") {
				pc->stats.transform_type = infile.nextValue();
				if (pc->stats.transform_type != "") pc->stats.transform_duration = -1;
			}
			else if (infile.key == "powers") {
				string power;
				while ( (power = infile.nextValue()) != "") {
					menu->pow->powers_list.push_back(toInt(power));
				}
			}
			else if (infile.key == "campaign") camp->setAll(infile.val);
		}

		infile.close();
	}

	// Load stash
	ss.str("");
	ss << PATH_USER << "stash.txt";

	if (infile.open(ss.str())) {
		while (infile.next()) {
			if (infile.key == "item") {
				menu->stash->stock.setItems(infile.val);
			}
			else if (infile.key == "quantity") {
				menu->stash->stock.setQuantities(infile.val);
			}
		}
		infile.close();
	}

	// initialize vars
	pc->stats.recalc();
	menu->inv->applyEquipment(menu->inv->inventory[EQUIPMENT].storage);
	if (SAVE_HPMP) {
		pc->stats.hp = saved_hp;
		pc->stats.mp = saved_mp;
	} else {
		pc->stats.hp = pc->stats.maxhp;
		pc->stats.mp = pc->stats.maxmp;
	}

	// reset character menu
	menu->chr->refreshStats();

	// just for aesthetics, turn the hero to face the camera
	pc->stats.direction = 6;

	// set up MenuTalker for this hero
	menu->talker->setHero(pc->stats.name, pc->stats.portrait);

	// load sounds (gender specific)
	pc->loadSounds();

}

/*
 * This is used to load the stash when starting a new game
 */
void GameStatePlay::loadStash() {
	// Load stash
	FileParser infile;
	stringstream ss;
	ss.str("");
	ss << PATH_USER << "stash.txt";

	if (infile.open(ss.str())) {
		while (infile.next()) {
			if (infile.key == "item") {
				menu->stash->stock.setItems(infile.val);
			}
			else if (infile.key == "quantity") {
				menu->stash->stock.setQuantities(infile.val);
			}
		}
		infile.close();
	}
}
