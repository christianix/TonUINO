#include "PlayerMode.h"

/*
 *  AlbumMode
 */

bool AlbumMode::handleNextTrack() {
	if (this->curr_track < this->n_tracks) {
		this->curr_track = this->curr_track + 1;
	} else {
		this->curr_track = 1;
	}

	DEBUG_PRINT(F("AlbumMode: Playing next track "));
	DEBUG_PRINT(this->curr_track);
	DEBUG_PRINT(F(" of "));
	DEBUG_PRINTLN(this->n_tracks);

	mp3.playFolderTrack(myFolder->folder, this->curr_track);
	return true;
}

bool AlbumMode::handlePreviousTrack() {
	if (this->curr_track > 1) {
		this->curr_track = this->curr_track - 1;
	} else {
		this->curr_track = this->n_tracks;
	}

	DEBUG_PRINT(F("AlbumMode: Playing previous track "));
	DEBUG_PRINT(this->curr_track);
	DEBUG_PRINT(F(" of "));
	DEBUG_PRINTLN(this->n_tracks);

	mp3.playFolderTrack(myFolder->folder, this->curr_track);
	return true;
}

bool AlbumMode::handlePlay() {
	DEBUG_PRINT(F("AlbumMode: Playing track "));
	DEBUG_PRINT(this->curr_track);
	DEBUG_PRINT(F(" of "));
	DEBUG_PRINTLN(this->n_tracks);

	mp3.playFolderTrack(myFolder->folder, this->curr_track);
	return true;
}

bool AlbumMode::handleStop() {
	mp3.stop();
	return true;
}

/*
 *  AudiobookMode
 */

bool AudiobookMode::handleNextTrack() {
	if (this->curr_track < this->n_tracks) {
		this->curr_track = this->curr_track + 1;

		DEBUG_PRINT(F("AudiobookMode: Playing previous track "));
		DEBUG_PRINT(this->curr_track);
		DEBUG_PRINT(F(" of "));
		DEBUG_PRINTLN(this->n_tracks);

		mp3.playFolderTrack(myFolder->folder, this->curr_track);
		// Fortschritt im EEPROM abspeichern
		EEPROM.update(myFolder->folder, this->curr_track);
	}
	/* TODO: Check if user request or finished
	if user finished playing:
		EEPROM.update(myFolder->folder, 1);
		setstandbyTimer();
	 */
	return true;
}

bool AudiobookMode::handlePreviousTrack() {
	if (this->curr_track > 1) {
		this->curr_track = this->curr_track - 1;

		DEBUG_PRINT(F("AudiobookMode: Playing previous track "));
		DEBUG_PRINT(this->curr_track);
		DEBUG_PRINT(F(" of "));
		DEBUG_PRINTLN(this->n_tracks);

		mp3.playFolderTrack(myFolder->folder, this->curr_track);
		// save progress in EEPROM
		EEPROM.update(myFolder->folder, this->curr_track);
	}
	return true;
}

bool AudiobookMode::handlePlay() {
	// load progress from EEPROM
	this->curr_track = EEPROM.read(myFolder->folder);
	if ( (this->curr_track < 1) || (this->curr_track > this->n_tracks) ) {
		this->curr_track = 1;
		// save progress in EEPROM
		EEPROM.update(myFolder->folder, this->curr_track);
	}

	DEBUG_PRINT(F("AudiobookMode: Playing track "));
	DEBUG_PRINT(this->curr_track);
	DEBUG_PRINT(F(" of "));
	DEBUG_PRINTLN(this->n_tracks);

	mp3.playFolderTrack(myFolder->folder, this->curr_track);
	return true;
}

bool AudiobookMode::handleStop() {
	mp3.stop();
	return true;
}

/*
 *  RandomMode
 */

bool RandomMode::handleNextTrack() {
	if (this->curr_track != this->n_tracks) {
		DEBUG_PRINT(F("Party -> weiter in der Queue "));
		this->curr_track++;
	} else {
		DEBUG_PRINTLN(F("Ende der Queue -> beginne von vorne"));
		this->curr_track = 1;
		shuffleQueue();
	}
	DEBUG_PRINTLN (queue[this->curr_track - 1]);
	mp3.playFolderTrack(myFolder->folder, queue[this->curr_track - 1]);
	return true;
}

bool RandomMode::handlePreviousTrack() {
	if (this->curr_track != 1) {
		DEBUG_PRINT(F("Party Modus ist aktiv -> zurück in der Qeueue "));
		this->curr_track--;
	} else {
		DEBUG_PRINT(F("Anfang der Queue -> springe ans Ende "));
		this->curr_track = this->n_tracks;
	}
	DEBUG_PRINTLN (queue[this->curr_track - 1]);
	mp3.playFolderTrack(myFolder->folder, queue[this->curr_track - 1]);
	return true;
}

bool RandomMode::handlePlay() {
	DEBUG_PRINTLN(F("Party Modus -> Ordner in zufälliger Reihenfolge wiedergeben"));
	shuffleQueue();
	this->curr_track = 1;
	mp3.playFolderTrack(myFolder->folder, queue[this->curr_track - 1]);
	return true;
}

bool RandomMode::handleStop() {
	mp3.stop();
	return true;
}

void RandomMode::shuffleQueue() {
	// Queue für die Zufallswiedergabe erstellen
	for (uint8_t x = 0; x < this->n_tracks; x++)
		queue[x] = x + 1;
	// Rest mit 0 auffüllen
	for (uint8_t x = this->n_tracks; x < 255; x++)
		queue[x] = 0;
	// Queue mischen
	for (uint8_t i = 0; i < this->n_tracks; i++) {
		uint8_t j = random(0, this->n_tracks);
		uint8_t t = queue[i];
		queue[i] = queue[j];
		queue[j] = t;
	}
	DEBUG_VERBOSE_PRINTLN(F("Queue :"));
	for (uint8_t x = 0; x < this->n_tracks; x++) {
		DEBUG_VERBOSE_PRINTLN (queue[x]);
	}
}
