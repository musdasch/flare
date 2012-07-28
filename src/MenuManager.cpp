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
 * class MenuManager
 */

#include "FileParser.h"
#include "UtilsParsing.h"
#include "UtilsFileSystem.h"
#include "Menu.h"
#include "MenuManager.h"
#include "MenuActionBar.h"
#include "MenuCharacter.h"
#include "MenuStatBar.h"
#include "MenuHUDLog.h"
#include "MenuInventory.h"
#include "MenuMiniMap.h"
#include "MenuPowers.h"
#include "MenuEnemy.h"
#include "MenuVendor.h"
#include "MenuTalker.h"
#include "MenuExit.h"
#include "MenuActiveEffects.h"
#include "MenuStash.h"
#include "MenuLog.h"
#include "ModManager.h"
#include "NPC.h"
#include "PowerManager.h"
#include "SharedResources.h"

MenuManager::MenuManager(PowerManager *_powers, StatBlock *_stats, CampaignManager *_camp, ItemManager *_items) {
	powers = _powers;
	stats = _stats;
	powers = _powers;
	camp = _camp;
	items = _items;

	loadIcons();

	hp = new MenuStatBar("hp");
	menus.push_back(hp); // menus[0]
	mp = new MenuStatBar("mp");
	menus.push_back(mp); // menus[1]
	xp = new MenuStatBar("xp");
	menus.push_back(xp); // menus[2]
	effects = new MenuActiveEffects(icons);
	menus.push_back(effects); // menus[3]
	hudlog = new MenuHUDLog();
	menus.push_back(hudlog); // menus[4]
	act = new MenuActionBar(powers, stats, icons);
	menus.push_back(act); // menus[5]
	enemy = new MenuEnemy();
	menus.push_back(enemy); // menus[6]
	vendor = new MenuVendor(items, stats);
	menus.push_back(vendor); // menus[7]
	talker = new MenuTalker(camp);
	menus.push_back(talker); // menus[8]
	exit = new MenuExit();
	menus.push_back(exit); // menus[9]
	mini = new MenuMiniMap();
	menus.push_back(mini); // menus[10]
	chr = new MenuCharacter(stats);
	menus.push_back(chr); // menus[11]
	inv = new MenuInventory(items, stats, powers);
	menus.push_back(inv); // menus[12]
	pow = new MenuPowers(stats, powers, icons);
	menus.push_back(pow); // menus[13]
	log = new MenuLog();
	menus.push_back(log); // menus[14]
	stash = new MenuStash(items, stats);
	menus.push_back(stash); // menus[15]
	tip = new WidgetTooltip();

	// Load the menu positions and alignments from menus/menus.txt
	int x,y,w,h;
	std::string align;
	int menu_index;
	FileParser infile;
	if (infile.open(mods->locate("menus/menus.txt"))) {
		while (infile.next()) {
			infile.val = infile.val + ',';
			x = eatFirstInt(infile.val, ',');
			y = eatFirstInt(infile.val, ',');
			w = eatFirstInt(infile.val, ',');
			h = eatFirstInt(infile.val, ',');
			align = eatFirstString(infile.val, ',');

			menu_index = -1;

			if (infile.key == "hp") menu_index = 0;
			else if (infile.key == "mp") menu_index = 1;
			else if (infile.key == "xp") menu_index = 2;
			else if (infile.key == "effects") menu_index = 3;
			else if (infile.key == "hudlog") menu_index = 4;
			else if (infile.key == "actionbar") menu_index = 5;
			else if (infile.key == "enemy") menu_index = 6;
			else if (infile.key == "vendor") menu_index = 7;
			else if (infile.key == "talker") menu_index = 8;
			else if (infile.key == "exit") menu_index = 9;
			else if (infile.key == "minimap") menu_index = 10;
			else if (infile.key == "character") menu_index = 11;
			else if (infile.key == "inventory") menu_index = 12;
			else if (infile.key == "powers") menu_index = 13;
			else if (infile.key == "log") menu_index = 14;
			else if (infile.key == "stash") menu_index = 15;

			if (menu_index != -1) {
				menus[menu_index]->window_area.x = x;
				menus[menu_index]->window_area.y = y;
				menus[menu_index]->window_area.w = w;
				menus[menu_index]->window_area.h = h;
				menus[menu_index]->alignment = align;
				menus[menu_index]->align();
			}

		}
	} else {
		fprintf(stderr, "Unable to open menus.txt!\n");
	}
	infile.close();

	// Some menus need to be updated to apply their new dimensions
	act->update();
	vendor->update();
	vendor->buyback_stock.init(NPC_VENDOR_MAX_STOCK, items);
	talker->update();
	exit->update();
	chr->update();
	inv->update();
	pow->update();
	log->update();
	stash->update();

	pause = false;
	dragging = false;
	drag_stack.item = 0;
	drag_stack.quantity = 0;
	drag_power = -1;
	drag_src = 0;
	drop_stack.item = 0;
	drop_stack.quantity = 0;


	loadSounds();

	done = false;
}

/**
 * Icon set shared by all menus
 */
void MenuManager::loadIcons() {

	icons = IMG_Load(mods->locate("images/icons/icons_small.png").c_str());
	if(!icons) {
		fprintf(stderr, "Couldn't load icons: %s\n", IMG_GetError());
		SDL_Quit();
	}

	// optimize
	SDL_Surface *cleanup = icons;
	icons = SDL_DisplayFormatAlpha(icons);
	SDL_FreeSurface(cleanup);
}

void MenuManager::loadSounds() {
	if (audio == true) {
		sfx_open = Mix_LoadWAV(mods->locate("soundfx/inventory/inventory_page.ogg").c_str());
		sfx_close = Mix_LoadWAV(mods->locate("soundfx/inventory/inventory_book.ogg").c_str());

		if (!sfx_open || !sfx_close)
			fprintf(stderr, "Mix_LoadWAV: %s\n", Mix_GetError());
	} else {
		sfx_open = NULL;
		sfx_close = NULL;
	}
}


void MenuManager::renderIcon(int icon_id, int x, int y) {
	SDL_Rect src;
	SDL_Rect dest;
	dest.x = x;
	dest.y = y;
	src.w = src.h = dest.w = dest.h = ICON_SIZE_SMALL;
	src.x = (icon_id % 16) * ICON_SIZE_SMALL;
	src.y = (icon_id / 16) * ICON_SIZE_SMALL;
	SDL_BlitSurface(icons, &src, screen, &dest);
}

void MenuManager::logic() {

	bool clicking_character = false;
	bool clicking_inventory = false;
	bool clicking_powers = false;
	bool clicking_log = false;
	ItemStack stack;

	hp->update(stats->hp,stats->maxhp,inpt->mouse,"");
	mp->update(stats->mp,stats->maxmp,inpt->mouse,"");
	xp->update((stats->xp - stats->xp_table[stats->level-1]),(stats->xp_table[stats->level] - stats->xp_table[stats->level-1]),inpt->mouse,msg->get("XP: %d/%d", stats->xp, stats->xp_table[stats->level]));
	effects->update(stats);

	hudlog->logic();
	enemy->logic();
	chr->logic();
	inv->logic();
	vendor->logic();
	pow->logic();
	log->logic();
	talker->logic();
	stash->logic();
	if (chr->checkUpgrade() || stats->level_up) {
		// apply equipment and max hp/mp
		inv->applyEquipment(inv->inventory[EQUIPMENT].storage);
		stats->hp = stats->maxhp;
		stats->mp = stats->maxmp;
		stats->level_up = false;
	}

	// only allow the vendor window to be open if the inventory is open
	if (vendor->visible && !(inv->visible)) {
	  closeLeft(true);
	  if (vendor->talker_visible && !(inv->visible))
		  closeRight(true);
	}

	if (!inpt->pressing[INVENTORY] && !inpt->pressing[POWERS] && !inpt->pressing[CHARACTER] && !inpt->pressing[LOG])
		key_lock = false;

	// check if mouse-clicking a menu button
	act->checkMenu(inpt->mouse, clicking_character, clicking_inventory, clicking_powers, clicking_log);

	if (exit->visible) {
		exit->logic();
		if (exit->isExitRequested()) {
			done = true;
		}
	}

	// exit menu toggle
	if ((inpt->pressing[CANCEL] && !inpt->lock[CANCEL] && !key_lock && !dragging) && !(stats->corpse && stats->permadeath) && stats->transform_duration < 1) {
		inpt->lock[CANCEL] = true;
		key_lock = true;
		if (menus_open) {
			closeAll(true);
		}
		else {
			exit->visible = !exit->visible;
		}
	}

	// inventory menu toggle
	if ((inpt->pressing[INVENTORY] && !key_lock && !dragging) || clicking_inventory) {
		key_lock = true;
		if (inv->visible) {
			closeRight(true);
		}
		else {
			closeRight(false);
			act->requires_attention[MENU_INVENTORY] = false;
			inv->visible = true;
			if (sfx_open)
				Mix_PlayChannel(-1, sfx_open, 0);
		}

	}

	// powers menu toggle
	if (((inpt->pressing[POWERS] && !key_lock && !dragging) || clicking_powers) && stats->humanoid) {
		key_lock = true;
		if (pow->visible) {
			closeRight(true);
		}
		else {
			closeRight(false);
			act->requires_attention[MENU_POWERS] = false;
			pow->visible = true;
			if (sfx_open)
				Mix_PlayChannel(-1, sfx_open, 0);
		}
	}

	// character menu toggleggle
	if (((inpt->pressing[CHARACTER] && !key_lock && !dragging) || clicking_character) && stats->humanoid) {
		key_lock = true;
		if (chr->visible) {
			closeLeft(true);
		}
		else {
			closeLeft(false);
			act->requires_attention[MENU_CHARACTER] = false;
			chr->visible = true;
			if (sfx_open)
				Mix_PlayChannel(-1, sfx_open, 0);
		}
	}

	// log menu toggle
	if ((inpt->pressing[LOG] && !key_lock && !dragging) || clicking_log) {
		key_lock = true;
		if (log->visible) {
			closeLeft(true);
		}
		else {
			closeLeft(false);
			act->requires_attention[MENU_LOG] = false;
			log->visible = true;
			if (sfx_open)
				Mix_PlayChannel(-1, sfx_open, 0);
		}
	}

	if (MENUS_PAUSE) {
		pause = (inv->visible || pow->visible || chr->visible || log->visible || vendor->visible || talker->visible);
	}
	menus_open = (inv->visible || pow->visible || chr->visible || log->visible || vendor->visible || talker->visible);

	if (stats->alive) {

		// handle right-click
		if (!dragging && inpt->pressing[MAIN2] && !inpt->lock[MAIN2]) {
			// exit menu
			if (exit->visible && isWithin(exit->window_area, inpt->mouse)) {
				inpt->lock[MAIN2] = true;
			}

			// activate inventory item
			else if (inv->visible && isWithin(inv->window_area, inpt->mouse)) {
				inpt->lock[MAIN2] = true;
				if (isWithin(inv->carried_area, inpt->mouse)) {
					inv->activate(inpt);
				}
			}
		}

		// handle left-click
		if (!dragging && inpt->pressing[MAIN1] && !inpt->lock[MAIN1]) {
			// exit menu
			if (exit->visible && isWithin(exit->window_area, inpt->mouse)) {
				inpt->lock[MAIN1] = true;
			}

			if (vendor->visible && isWithin(vendor->window_area,inpt->mouse)) {
				inpt->lock[MAIN1] = true;
				vendor->tabsLogic();
				if (inpt->pressing[CTRL]) {
					// buy item from a vendor
					stack = vendor->click(inpt);
					if (stack.item > 0) {
						if( inv->full(stack.item)) {
							// Can we say "Not enough place" ?
							vendor->itemReturn( stack);
						} else if( ! inv->buy( stack)) {
							// Can we say "Not enough money" ? (here or in MenuInventory::buy())
							vendor->itemReturn( stack);
						}
					}
				} else {
					// start dragging a vendor item
					drag_stack = vendor->click(inpt);
					if (drag_stack.item > 0) {
						dragging = true;
						drag_src = DRAG_SRC_VENDOR;
					}
				}
			}

			if (stash->visible && isWithin(stash->window_area,inpt->mouse)) {
				inpt->lock[MAIN1] = true;
				if (inpt->pressing[CTRL]) {
					// take an item from the stash
					stack = stash->click(inpt);
					if (stack.item > 0) {
						if( inv->full(stack.item)) {
							// Can we say "Not enough place" ?
							stash->itemReturn( stack);
						} else if( ! inv->stashRemove( stack)) {
							stash->itemReturn( stack);
						}
						stash->updated = true;
					}
				} else {
					// start dragging a stash item
					drag_stack = stash->click(inpt);
					if (drag_stack.item > 0) {
						dragging = true;
						drag_src = DRAG_SRC_STASH;
					}
				}
			}

			if(log->visible && isWithin(log->window_area,inpt->mouse)) {
				inpt->lock[MAIN1] = true;
				log->tabsLogic();
			}

			// pick up an inventory item
			if (inv->visible && isWithin(inv->window_area,inpt->mouse)) {
				if (inpt->pressing[CTRL]) {
					vendor->setTab(VENDOR_SELL);
					inpt->lock[MAIN1] = true;
					stack = inv->click(inpt);
					if( stack.item > 0) {
						if (vendor->visible) {
							// The vendor could have a limited amount of money in the future. It will be tested here.
							if (inv->sell(stack)) {
								vendor->add(stack);
							}
							else {
								inv->itemReturn(stack);
							}
						}
						else if (stash->visible) {
							if (inv->stashAdd(stack) && !stash->full(stack.item)) {
								stash->add(stack);
								stash->updated = true;
							}
							else {
								inv->itemReturn(stack);
							}
						}
						else {
							if (!inv->sell(stack)) {
								inv->itemReturn(stack);
							}
						}
					}
				}
				else {
					if (vendor->visible) vendor->setTab(VENDOR_SELL);
					inpt->lock[MAIN1] = true;
					drag_stack = inv->click(inpt);
					if (drag_stack.item > 0) {
						dragging = true;
						drag_src = DRAG_SRC_INVENTORY;
					}
				}
			}
			// pick up a power
			if (pow->visible && isWithin(pow->window_area,inpt->mouse)) {
				inpt->lock[MAIN1] = true;
				pow->unlock_click(inpt->mouse);
				drag_power = pow->click(inpt->mouse);
				if (drag_power > -1) {
					dragging = true;
					drag_src = DRAG_SRC_POWERS;
				}
			}
			// action bar
			if (isWithin(act->numberArea,inpt->mouse) || isWithin(act->mouseArea,inpt->mouse) || isWithin(act->menuArea, inpt->mouse)) {
				inpt->lock[MAIN1] = true;

				// ctrl-click action bar to clear that slot
				if (inpt->pressing[CTRL] && !stats->transformed) {
					act->remove(inpt->mouse);
				}
				// allow drag-to-rearrange action bar
				else if (!isWithin(act->menuArea, inpt->mouse)) {
					drag_power = act->checkDrag(inpt->mouse);
					if (drag_power > -1) {
						dragging = true;
						drag_src = DRAG_SRC_ACTIONBAR;
					}
				}

				// else, clicking action bar to use a power?
				// this check is done by GameEngine when calling Avatar::logic()


			}
		}

		// handle dropping
		if (dragging && !inpt->pressing[MAIN1]) {

			// putting a power on the Action Bar
			if (drag_src == DRAG_SRC_POWERS) {
				if (isWithin(act->numberArea,inpt->mouse) || isWithin(act->mouseArea,inpt->mouse)) {
					act->drop(inpt->mouse, drag_power, 0);
				}
			}

			// rearranging the action bar
			else if (drag_src == DRAG_SRC_ACTIONBAR) {
				if (isWithin(act->numberArea,inpt->mouse) || isWithin(act->mouseArea,inpt->mouse)) {
					act->drop(inpt->mouse, drag_power, 1);
				}
			}

			// rearranging inventory or dropping items
			else if (drag_src == DRAG_SRC_INVENTORY) {

				if (inv->visible && isWithin(inv->window_area, inpt->mouse)) {
					inv->drop(inpt->mouse, drag_stack);
					drag_stack.item = 0;
				}
				else if (isWithin(act->numberArea,inpt->mouse) || isWithin(act->mouseArea,inpt->mouse)) {
					// The action bar is not storage!
					inv->itemReturn(drag_stack);

					// put an item with a power on the action bar
					if (items->items[drag_stack.item].power != -1) {
						act->drop(inpt->mouse, items->items[drag_stack.item].power, false);
					}
				}
				else if (vendor->visible && isWithin(vendor->slots_area, inpt->mouse)) {
					if (inv->sell( drag_stack)) {
						vendor->add( drag_stack);
					}
					else {
						inv->itemReturn(drag_stack);
					}
					drag_stack.item = 0;
				}
				else if (stash->visible && isWithin(stash->slots_area, inpt->mouse)) {
					if (inv->stashAdd( drag_stack) && !stash->full(drag_stack.item)) {
						stash->add( drag_stack);
						stash->updated = true;
					}
					else {
						inv->itemReturn(drag_stack);
					}
					drag_stack.item = 0;
				}
				else {
					// if dragging and the source was inventory, drop item to the floor

					// quest items cannot be dropped
					if (items->items[drag_stack.item].type != ITEM_TYPE_QUEST) {
						drop_stack = drag_stack;
						drag_stack.item = 0;
						drag_stack.quantity = 0;
					}
					else {
						inv->itemReturn(drag_stack);
					}
				}
			}

			else if (drag_src == DRAG_SRC_VENDOR) {

				// dropping an item from vendor (we only allow to drop into the carried area)
				if (inv->visible && isWithin( inv->carried_area, inpt->mouse)) {
					if( inv->full(drag_stack.item)) {
						// Can we say "Not enough place" ?
						vendor->itemReturn( drag_stack);
					}
					else if( ! inv->buy( drag_stack, inpt->mouse)) {
						// Can we say "Not enough money" ? (here or in MenuInventory::buy())
						vendor->itemReturn( drag_stack);
					}
					drag_stack.item = 0;
				}
				else {
					vendor->itemReturn(drag_stack);
				}
			}

			else if (drag_src == DRAG_SRC_STASH) {

				// dropping an item from vendor (we only allow to drop into the carried area)
				if (inv->visible && isWithin( inv->carried_area, inpt->mouse)) {
					if( inv->full(drag_stack.item)) {
						// Can we say "Not enough place" ?
						stash->itemReturn( drag_stack);
					}
					else if( ! inv->stashRemove( drag_stack, inpt->mouse)) {
						stash->itemReturn( drag_stack);
					}
					stash->updated = true;
					drag_stack.item = 0;
				}
				else {
					stash->itemReturn(drag_stack);
				}
			}

			dragging = false;
		}

	}

	// handle equipment changes affecting hero stats
	if (inv->changed_equipment || inv->changed_artifact) {
		inv->applyEquipment(inv->inventory[EQUIPMENT].storage);
		inv->changed_artifact = false;
		// the equipment flag is reset after the new sprites are loaded
	}

	// for action-bar powers that represent items, lookup the current item count
	for (int i=0; i<12; i++) {
		act->slot_enabled[i] = true;
		act->slot_item_count[i] = -1;

		if (act->hotkeys[i] != -1) {
			int item_id = powers->powers[act->hotkeys[i]].requires_item;
			if (item_id != -1 && items->items[item_id].type == ITEM_TYPE_CONSUMABLE) {
				act->slot_item_count[i] = inv->getItemCountCarried(item_id);
				if (act->slot_item_count[i] == 0) {
					act->slot_enabled[i] = false;
				}
			}
			else if (item_id != -1) {

				// if a non-consumable item power is unequipped, disable that slot
				if (!inv->isItemEquipped(item_id)) {
					act->slot_enabled[i] = false;
				}
			}
		}
	}

}

void MenuManager::render() {
	for (unsigned int i=0; i<menus.size(); i++) {
		menus[i]->render();
	}

	TooltipData tip_new;

	// Find tooltips depending on mouse position
	if (chr->visible && isWithin(chr->window_area,inpt->mouse)) {
		tip_new = chr->checkTooltip();
	}
	if (vendor->visible && isWithin(vendor->window_area,inpt->mouse)) {
		tip_new = vendor->checkTooltip(inpt->mouse);
	}
	if (stash->visible && isWithin(stash->window_area,inpt->mouse)) {
		tip_new = stash->checkTooltip(inpt->mouse);
	}
	if (pow->visible && isWithin(pow->window_area,inpt->mouse)) {
		tip_new = pow->checkTooltip(inpt->mouse);
	}
	if (inv->visible && !dragging && isWithin(inv->window_area,inpt->mouse)) {
		tip_new = inv->checkTooltip(inpt->mouse);
	}
	if (isWithin(act->window_area,inpt->mouse)) {
		tip_new = act->checkTooltip(inpt->mouse);
	}

	if (tip_new.num_lines > 0) {

		// when we render a tooltip it buffers the rasterized text for performance.
		// If this new tooltip is the same as the existing one, reuse.

		// TODO: comparing the first line of a tooltip works in all existing cases,
		// but may not hold true in the future.
		if (tip_new.lines[0] != tip_buf.lines[0]) {
			tip_buf.clear();
			tip_buf = tip_new;
		}
		tip->render(tip_buf, inpt->mouse, STYLE_FLOAT);
	}

	// draw icon under cursor if dragging
	if (dragging) {
		if (drag_src == DRAG_SRC_INVENTORY || drag_src == DRAG_SRC_VENDOR || drag_src == DRAG_SRC_STASH)
			items->renderIcon(drag_stack, inpt->mouse.x - ICON_SIZE_SMALL/2, inpt->mouse.y - ICON_SIZE_SMALL/2, ICON_SIZE_SMALL);
		else if (drag_src == DRAG_SRC_POWERS || drag_src == DRAG_SRC_ACTIONBAR)
			renderIcon(powers->powers[drag_power].icon, inpt->mouse.x-ICON_SIZE_SMALL/2, inpt->mouse.y-ICON_SIZE_SMALL/2);
	}

}

void MenuManager::closeAll(bool play_sound) {
	if (!dragging) {
		closeLeft(play_sound);
		closeRight(false);
		vendor->talker_visible = false;
	}
}

void MenuManager::closeLeft(bool play_sound) {
	if (!dragging) {
		chr->visible = false;
		log->visible = false;
		vendor->visible = false;
		talker->visible = false;
		exit->visible = false;
		stash->visible = false;

		if (sfx_close)
			if (play_sound) Mix_PlayChannel(-1, sfx_close, 0);

	}
}

void MenuManager::closeRight(bool play_sound) {
	if (!dragging) {
		inv->visible = false;
		pow->visible = false;
		talker->visible = false;
		exit->visible = false;

		if (sfx_close)
			if (play_sound) Mix_PlayChannel(-1, sfx_close, 0);
	}
}

MenuManager::~MenuManager() {

	tip_buf.clear();

	delete hp;
	delete mp;
	delete xp;
	delete mini;
	delete inv;
	delete pow;
	delete chr;
	delete hudlog;
	delete log;
	delete act;
	delete tip;
	delete vendor;
	delete talker;
	delete exit;
	delete enemy;
	delete effects;
	delete stash;

	Mix_FreeChunk(sfx_open);
	Mix_FreeChunk(sfx_close);
	SDL_FreeSurface(icons);
}
