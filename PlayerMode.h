#ifndef PLAYER_MODE_H
#define PLAYER_MODE_H

#include "Tonuino.h"
#include <DFMiniMp3.h>

/*
 * Player modes.
 * The are presented in audio menu with names starting from mp3/0311_*.mp3.
 */

// No mode configured (invalid)
#define MODE_INVALID 0
// Random mode: Play random track from folder
#define MODE_RANDOM 1
// Album mode: Play complete folder in numbered order
#define MODE_ALBUM 2
// Hörbuch mode: Play complete folder and save position
#define MODE_AUDIO_BOOK 3
// Admin mode: Admin functions
#define MODE_ADMIN_MENU 4
#define MODE_ADMIN 255

/**
 * Active player mode.
 */
class PlayerMode;
PlayerMode* activeMode = NULL;

/**
 * Player Mode.
 */
class PlayerMode {
public:

	PlayerMode(uint16_t num_tracks) : n_tracks(num_tracks), curr_track(1) {};
	virtual ~PlayerMode() {};

	virtual bool handleNextTrack() = 0;
	virtual bool handlePreviousTrack() = 0;
	virtual bool handlePlay() = 0;
	virtual bool handleStop() = 0;

	/* Object members */
	uint16_t n_tracks;
	uint8_t curr_track;

	/* Class member functions for MP3 event handling */

	static void OnError(uint16_t errorCode) {
		// see DfMp3_Error for code meaning
		DEBUG_PRINT("COM error: ");
		DEBUG_PRINTLN(errorCode);
		return;
	}
	static void OnPlayFinished(DfMp3_PlaySources source, uint16_t track) {
		(void)track;
		PrintlnSourceAction(source, "finished playing");
		if (activeMode) activeMode->handleNextTrack();
	}
	static void OnPlaySourceOnline(DfMp3_PlaySources source) {
		PrintlnSourceAction(source, "online");
	}
	static void OnPlaySourceInserted(DfMp3_PlaySources source) {
		PrintlnSourceAction(source, "ready");
	}
	static void OnPlaySourceRemoved(DfMp3_PlaySources source) {
		PrintlnSourceAction(source, "removed");
	}

private:

	/* Class helper function for debugging */

	static void PrintlnSourceAction(DfMp3_PlaySources source, const char *action) {
			if (source & DfMp3_PlaySources_Sd) {
				DEBUG_PRINT("SD card ");
			} else {
				DEBUG_PRINT("Unsupported source ");
			}
			DEBUG_PRINTLN(action);
		}
};

/**
 * AlbumMode.
 */
class AlbumMode: public PlayerMode {
public:
	AlbumMode(uint16_t num_tracks) : PlayerMode(num_tracks) {}
	~AlbumMode() {};
	bool handleNextTrack();
	bool handlePreviousTrack();
	bool handlePlay();
	bool handleStop();
};

/**
 * AudiobookMode.
 */
class AudiobookMode: public PlayerMode {
public:
	AudiobookMode(uint16_t num_tracks) : PlayerMode(num_tracks) {}
	~AudiobookMode() {};
	bool handleNextTrack();
	bool handlePreviousTrack();
	bool handlePlay();
	bool handleStop();
};

/**
 * RandomMode.
 */
class RandomMode: public PlayerMode {
public:
	RandomMode(uint16_t num_tracks) : PlayerMode(num_tracks) {
		this->queue = new byte[num_tracks];
	}
	~RandomMode() {
		delete this->queue;
	};
	bool handleNextTrack();
	bool handlePreviousTrack();
	bool handlePlay();
	bool handleStop();
private:
	byte* queue;
	void shuffleQueue();
};

#endif // PLAYER_MODE_H
