/*
 _____         _____ _____ _____ _____
 |_   _|___ ___|  |  |     |   | |     |
 | | | . |   |  |  |-   -| | | |  |  |
 |_| |___|_|_|_____|_____|_|___|_____|
 TonUINO Version 2.3

 created by Thorsten Voß and licensed under GNU/GPL.
 Information and contribution at https://tonuino.de.
 */

#ifndef TONUINO_H
#define TONUINO_H

#include <DFMiniMp3.h>

#define TIMEOUT 1000UL

/**
 * MFRC522
 */
#define RST_PIN 9                 // Configurable, see typical pin layout above
#define SS_PIN 10                 // Configurable, see typical pin layout above

/**
 * Buttons
 */
#define buttonPause A0
#define buttonUp A1
#define buttonDown A2

#define LONG_PRESS 1000

/**
 * PINs
 */
#define busyPin 4
#define shutdownPin 7
#define openAnalogPin A7

/**
 * Magic pattern of our NFC cards.
 */
extern const byte cardCookie[4];

/**
 * Folder settings
 */
struct folderSettings {
	uint8_t folder;
	uint8_t mode;
};

/**
 * NFC tag data
 */
struct nfcTagObject {
	byte cookie[sizeof(cardCookie)];
	uint8_t version;
	folderSettings nfcFolderSettings;
};
#define MY_NFC_TAG_VERSION 3

/**
 * Admin settings stored in EEPROM
 */
struct adminSettings {
	byte cookie[sizeof(cardCookie)];
	byte version;
	byte maxVolume;
	byte minVolume;
	byte initVolume;
	DfMp3_Eq eq;
	byte standbyTimer;
	folderSettings shortCuts[4];
	byte adminMenuLocked;
	byte adminMenuPin[4];
};
#define MY_SETTINGS_VERSION 3

#endif // TONUINO_H
