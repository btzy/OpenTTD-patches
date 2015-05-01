/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game_sl.cpp Handles the saveload part of the GameScripts */

#include "../stdafx.h"
#include "../debug.h"
#include "saveload_buffer.h"
#include "saveload_error.h"
#include "../string.h"

#include "../game/game.hpp"
#include "../game/game_config.hpp"
#include "../network/network.h"
#include "../game/game_instance.hpp"
#include "../game/game_text.hpp"

struct GameSaveload {
	char name[64];
	char settings[1024];
	int version;
	bool is_random;
};

static const SaveLoad _game_script[] = {
	     SLE_STR(GameSaveload, name,        SLS_NONE),
	     SLE_STR(GameSaveload, settings,    SLS_NONE),
	     SLE_VAR(GameSaveload, version,   SLE_UINT32),
	     SLE_VAR(GameSaveload, is_random,   SLE_BOOL),
	     SLE_END()
};

static void Load_GSDT(LoadBuffer *reader)
{
	/* Free all current data */
	GameConfig::GetConfig(GameConfig::SSS_FORCE_GAME)->Change(NULL);

	if (reader->IterateChunk() == -1) return;

	GameSaveload gsl;
	gsl.version = -1;
	reader->ReadObject(&gsl, _game_script);

	if (_networking && !_network_server) {
		GameInstance::LoadEmpty(reader);
		if (reader->IterateChunk() != -1) throw SlCorrupt("Too many GameScript configs");
		return;
	}

	GameConfig *config = GameConfig::GetConfig(GameConfig::SSS_FORCE_GAME);
	if (StrEmpty(gsl.name)) {
	} else {
		config->Change(gsl.name, gsl.version, false, gsl.is_random);
		if (!config->HasScript()) {
			/* No version of the GameScript available that can load the data. Try to load the
			 * latest version of the GameScript instead. */
			config->Change(gsl.name, -1, false, gsl.is_random);
			if (!config->HasScript()) {
				if (strcmp(gsl.name, "%_dummy") != 0) {
					DEBUG(script, 0, "The savegame has an GameScript by the name '%s', version %d which is no longer available.", gsl.name, gsl.version);
					DEBUG(script, 0, "This game will continue to run without GameScript.");
				} else {
					DEBUG(script, 0, "The savegame had no GameScript available at the time of saving.");
					DEBUG(script, 0, "This game will continue to run without GameScript.");
				}
			} else {
				DEBUG(script, 0, "The savegame has an GameScript by the name '%s', version %d which is no longer available.", gsl.name, gsl.version);
				DEBUG(script, 0, "The latest version of that GameScript has been loaded instead, but it'll not get the savegame data as it's incompatible.");
			}
			/* Make sure the GameScript doesn't get the saveload data, as he was not the
			 *  writer of the saveload data in the first place */
			gsl.version = -1;
		}
	}

	config->StringToSettings(gsl.settings);

	/* Start the GameScript directly if it was active in the savegame */
	Game::StartNew();
	Game::Load(reader, gsl.version);

	if (reader->IterateChunk() != -1) throw SlCorrupt("Too many GameScript configs");
}

static void Save_GSDT(SaveDumper *dumper)
{
	GameConfig *config = GameConfig::GetConfig();
	GameSaveload gsl;

	if (config->HasScript()) {
		bstrcpy (gsl.name, config->GetName());
		gsl.version = config->GetVersion();
	} else {
		/* No GameScript is configured for this so store an empty string as name. */
		gsl.name[0] = '\0';
		gsl.version = -1;
	}

	gsl.is_random = config->IsRandom();
	gsl.settings[0] = '\0';
	config->SettingsToString(gsl.settings, lengthof(gsl.settings));

	SaveDumper temp(1024);

	temp.WriteObject(&gsl, _game_script);
	Game::Save(&temp);

	dumper->WriteElementHeader(0, temp.GetSize());
	temp.Dump(dumper);
}

static void Load_GSTR(LoadBuffer *reader)
{
	delete GameStrings::current;
	GameStrings::current = new GameStrings();

	while (reader->IterateChunk() != -1) {
		char *s = reader->ReadString (SLS_NONE);
		LanguageStrings *ls = new LanguageStrings (s != NULL ? s : "");
		free (s);

		for (uint n = reader->ReadUint32(); n > 0; n--) {
			char *s = reader->ReadString (SLS_ALLOW_CONTROL);
			*ls->raw.Append() = (s != NULL) ? s : xstrdup("");
		}

		GameStrings::current->strings.append (ls);
	}

	/* If there were no strings in the savegame, set GameStrings to NULL */
	if (GameStrings::current->strings.size() == 0) {
		delete GameStrings::current;
		GameStrings::current = NULL;
		return;
	}

	GameStrings::current->Compile();
	ReconsiderGameScriptLanguage();
}

static void Save_GSTR(SaveDumper *dumper)
{
	if (GameStrings::current == NULL) return;

	for (uint i = 0; i < GameStrings::current->strings.size(); i++) {
		const LanguageStrings *ls = GameStrings::current->strings[i].get();

		SaveDumper temp(1024);

		temp.WriteString (ls->language);

		uint n = ls->raw.Length();
		temp.WriteUint32 (n);

		for (uint j = 0; j < n; j++) {
			temp.WriteString (ls->raw[j]);
		}

		dumper->WriteElementHeader(i, temp.GetSize());
		temp.Dump(dumper);
	}
}

extern const ChunkHandler _game_chunk_handlers[] = {
	{ 'GSTR', Save_GSTR, Load_GSTR, NULL, NULL, CH_ARRAY },
	{ 'GSDT', Save_GSDT, Load_GSDT, NULL, NULL, CH_ARRAY | CH_LAST},
};
