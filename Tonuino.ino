/*
 _____         _____ _____ _____ _____
 |_   _|___ ___|  |  |     |   | |     |
 | | | . |   |  |  |-   -| | | |  |  |
 |_| |___|_|_|_____|_____|_|___|_____|
 TonUINO Version 2.3

 created by Thorsten Voß and licensed under GNU/GPL.
 Information and contribution at https://tonuino.de.
 */

#include <EEPROM.h>
#include <MFRC522.h>
#include <JC_Button.h>
#include <SPI.h>
#include <avr/sleep.h>
#include <SoftwareSerial.h>

/**
 * Compile-time configuration
 */

#include "Tonuino.h"
#include "Logging.h"
#include "PlayerMode.h"

/**
 * MFRC522
 */
byte blockAddr = 4;
byte trailerBlock = 7;
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522
MFRC522::MIFARE_Key key;

/**
 * Buttons
 */
Button pauseButton(buttonPause);
Button upButton(buttonUp);
Button downButton(buttonDown);

/* Cookie to identify our cards */
const byte cardCookie[4] = { 0x13, 0x37, 0xb3, 0x47 };

/* DFPlayer Mini */
SoftwareSerial mySoftwareSerial(2, 3); // RX, TX

/**
 * Global variables.
 * TODO: Move to global state object
 */
uint8_t myVolume;
unsigned long sleepAtMillis = 0;
bool knownCard = false;
folderSettings *myFolder;
nfcTagObject myCard;
adminSettings mySettings;
static DFMiniMp3<SoftwareSerial, PlayerMode> mp3(mySoftwareSerial);

/**
 * Forward declarations.
 */

uint8_t voiceMenu(
		int numberOfOptions,
		int startMessage,
		int messageOffset,
		bool preview = false,
		int defaultValue = 0);
bool isPlaying();
void writeCard(nfcTagObject nfcTag);
void adminMenu(bool adminCard);

/**
 * Function definitions
 */

void storeSettings() {
	DEBUG_PRINTLN(F("=== storeSettings"));
	int address = sizeof(myFolder->folder) * 100;
	EEPROM.put(address, mySettings);
}

void resetSettings() {
	DEBUG_PRINTLN(F("=== resetSettings"));
	memcpy(&mySettings.cookie[0], &cardCookie[0], sizeof(cardCookie));
	mySettings.version = MY_SETTINGS_VERSION;
	mySettings.maxVolume = 25;
	mySettings.minVolume = 5;
	mySettings.initVolume = 15;
	mySettings.eq = DfMp3_Eq_Normal;
	mySettings.standbyTimer = 0;
	mySettings.shortCuts[0].folder = 0;
	mySettings.shortCuts[1].folder = 0;
	mySettings.shortCuts[2].folder = 0;
	mySettings.shortCuts[3].folder = 0;
	mySettings.adminMenuLocked = 0;
	mySettings.adminMenuPin[0] = 1;
	mySettings.adminMenuPin[1] = 1;
	mySettings.adminMenuPin[2] = 1;
	mySettings.adminMenuPin[3] = 1;

	storeSettings();
}

void loadSettings() {
	DEBUG_PRINTLN(F("=== loadSettings"));
	int address = sizeof(myFolder->folder) * 100;
	EEPROM.get(address, mySettings);
	if (memcmp(&mySettings.cookie[0], &cardCookie[0], sizeof(cardCookie))
			!= 0) {
		resetSettings();
	}

	DEBUG_PRINT(F("Version: "));
	DEBUG_PRINTLN(mySettings.version);

	DEBUG_PRINT(F("Maximal Volume: "));
	DEBUG_PRINTLN(mySettings.maxVolume);

	DEBUG_PRINT(F("Minimal Volume: "));
	DEBUG_PRINTLN(mySettings.minVolume);

	DEBUG_PRINT(F("Initial Volume: "));
	DEBUG_PRINTLN(mySettings.initVolume);

	DEBUG_PRINT(F("EQ: "));
	DEBUG_PRINTLN(mySettings.eq);

	DEBUG_PRINT(F("Sleep Timer: "));
	DEBUG_PRINTLN(mySettings.standbyTimer);

	DEBUG_PRINT(F("Admin Menu locked: "));
	DEBUG_PRINTLN(mySettings.adminMenuLocked);

	DEBUG_PRINT(F("Admin Menu Pin: "));
	DEBUG_PRINT(mySettings.adminMenuPin[0]);
	DEBUG_PRINT(mySettings.adminMenuPin[1]);
	DEBUG_PRINT(mySettings.adminMenuPin[2]);
	DEBUG_PRINTLN(mySettings.adminMenuPin[3]);
}

/// Funktionen für den Standby Timer (z.B. über Pololu-Switch oder Mosfet)

void setstandbyTimer() {
	DEBUG_PRINTLN(F("=== setstandbyTimer()"));
	if (mySettings.standbyTimer != 0)
		sleepAtMillis = millis() + (mySettings.standbyTimer * 60 * 1000);
	else
		sleepAtMillis = 0;
	DEBUG_PRINTLN(sleepAtMillis);
}

void disablestandbyTimer() {
	DEBUG_PRINTLN(F("=== disablestandby()"));
	sleepAtMillis = 0;
}

void checkStandbyAtMillis() {
	if (sleepAtMillis != 0 && millis() > sleepAtMillis) {
		DEBUG_PRINTLN(F("=== power off!"));
		// enter sleep state
		digitalWrite(shutdownPin, HIGH);
		delay(500);

		// http://discourse.voss.earth/t/intenso-s10000-powerbank-automatische-abschaltung-software-only/805
		// powerdown to 27mA (powerbank switches off after 30-60s)
		mfrc522.PCD_AntennaOff();
		mfrc522.PCD_SoftPowerDown();
		mp3.sleep();

		set_sleep_mode(SLEEP_MODE_PWR_DOWN);
		cli();  // Disable interrupts
		sleep_mode();
	}
}

/**
 * Check line busy.
 */
bool isPlaying() {
	return !digitalRead(busyPin);
}

void waitForTrackToFinish() {
	// TODO: Add time-out as parameter
	do {
		delay(500);
	} while (isPlaying());
}

void setup() {

	Serial.begin(115200); // set-up serial for debug messages

	// init randomSeed() from ADC LSBs
	uint32_t ADC_LSB = 0;
	uint32_t ADCSeed = 0;
	for (uint8_t i = 0; i < 128; i++) {
		ADC_LSB = analogRead(openAnalogPin) & 0x1;
		ADCSeed ^= ADC_LSB << (i % 32);
	}
	randomSeed(ADCSeed); // init PRNG

	// Dieser Hinweis darf nicht entfernt werden
	DEBUG_PRINTLN(F("\n _____         _____ _____ _____ _____"));
	DEBUG_PRINTLN(F("|_   _|___ ___|  |  |     |   | |     |"));
	DEBUG_PRINTLN(F("  | | | . |   |  |  |-   -| | | |  |  |"));
	DEBUG_PRINTLN(F("  |_| |___|_|_|_____|_____|_|___|_____|\n"));
	DEBUG_PRINTLN(F("TonUINO Version 2.3"));
	DEBUG_PRINTLN(F("created by Thorsten Voß and licensed under GNU/GPL."));
	DEBUG_PRINTLN(F("Information and contribution at https://tonuino.de.\n"));

	// Busy Pin
	pinMode(busyPin, INPUT);

	// load Settings from EEPROM
	loadSettings();

	// activate standby timer
	setstandbyTimer();

	// DFPlayer Mini initialisieren
	mp3.begin();
	// Zwei Sekunden warten bis der DFPlayer Mini initialisiert ist
	delay(2000);

	// NFC Leser initialisieren
	SPI.begin();        // Init SPI bus
	mfrc522.PCD_Init(); // Init MFRC522
	mfrc522.PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader
	for (byte i = 0; i < 6; i++) {
		key.keyByte[i] = 0xFF;
	}

	pinMode(buttonPause, INPUT_PULLUP);
	pinMode(buttonUp, INPUT_PULLUP);
	pinMode(buttonDown, INPUT_PULLUP);
	pinMode(shutdownPin, OUTPUT);
	digitalWrite(shutdownPin, LOW);

	// RESET --- ALLE DREI KNÖPFE BEIM STARTEN GEDRÜCKT HALTEN -> alle EINSTELLUNGEN werden gelöscht
	if (digitalRead(buttonPause) == LOW && digitalRead(buttonUp) == LOW
			&& digitalRead(buttonDown) == LOW) {
		DEBUG_PRINTLN(F("Reset -> EEPROM wird gelöscht"));
		for (unsigned int i = 0; i < EEPROM.length(); i++) {
			EEPROM.update(i, 0);
		}
		loadSettings();
	}

	// Set volume
	myVolume = mySettings.initVolume;
	mp3.setVolume(myVolume);

	// Set eq mode
	mp3.setEq(mySettings.eq);

	// Play welcome jingle
	mp3.playMp3FolderTrack(400);
	waitForTrackToFinish();
}

void readButtons() {
	pauseButton.read();
	upButton.read();
	downButton.read();
}

void loop() {
	do {
		checkStandbyAtMillis();
		mp3.loop();

		// check buttons
		readButtons();

		// check for admin menu request
		if (upButton.pressedFor(LONG_PRESS) && downButton.pressedFor(LONG_PRESS)
				&& pauseButton.isReleased())
		{
			PlayerMode* savedMode = activeMode;
			if (activeMode) {
				activeMode->handleStop();
				activeMode = NULL;
			}
			adminMenu(false);
			activeMode = savedMode;
			if (activeMode) activeMode->handlePlay();
		}

		// handle pause/play button
		// TODO: Action for LONG_PRESS?
		if (pauseButton.wasReleased()) {
			if (isPlaying()) {
				mp3.pause();
				setstandbyTimer();
			} else if (knownCard) {
				disablestandbyTimer();
				mp3.start();
			}
		}

		// handle up button
		if (upButton.pressedFor(LONG_PRESS) && downButton.isReleased()) {
			if (activeMode) {
				DEBUG_PRINTLN(F("=== nextTrack"));
				activeMode->handleNextTrack();
				delay(500);
			}
			// wait for key to be released
			do {
				readButtons();
			} while (upButton.isPressed());
			readButtons();
		} else if (upButton.wasReleased()) {
			DEBUG_PRINTLN(F("=== volumeUp"));
			if (myVolume < mySettings.maxVolume) {
				mp3.increaseVolume();
				myVolume++;
			}
			DEBUG_PRINTLN(myVolume);
		}

		// handle down button
		if (downButton.pressedFor(LONG_PRESS) && upButton.isReleased()) {
			if (activeMode) {
				DEBUG_PRINTLN(F("=== previousTrack"));
				activeMode->handlePreviousTrack();
				delay(1000);
			}
			// wait for key to be released
			do {
				readButtons();
			} while (downButton.isPressed());
			readButtons();
		} else if (downButton.wasReleased()) {
			DEBUG_PRINTLN(F("=== volumeDown"));
			if (myVolume > mySettings.minVolume) {
				mp3.decreaseVolume();
				myVolume--;
			}
			DEBUG_PRINTLN(myVolume);
		}
		// Ende der Buttons
	} while (!mfrc522.PICC_IsNewCardPresent());

	// RFID Karte wurde aufgelegt

	if (!mfrc522.PICC_ReadCardSerial())
		return;

	/* Invalidate card and active mode */
	knownCard = false;
	if (activeMode != NULL) {
		delete activeMode;
		activeMode = NULL;
	}

	if (readCard(&myCard) == true) {
		// valid card found, stop playing current title
		if (activeMode) activeMode->handleStop();

		// read tracks in folder of card
		uint16_t numTracksInFolder = 0;
		disablestandbyTimer();
		if (myFolder->folder != 0) {
			numTracksInFolder = mp3.getFolderTrackCount(myFolder->folder);
			DEBUG_PRINT(numTracksInFolder);
			DEBUG_PRINT(F(" Dateien in Ordner "));
			DEBUG_PRINTLN(myFolder->folder);
		}

		// set mode of card
		if (myCard.nfcFolderSettings.mode != MODE_INVALID) {
			DEBUG_PRINTLN(F("== set mode of folder"));
			myFolder->mode = myCard.nfcFolderSettings.mode;
		}

		switch (myFolder->mode) {
		case MODE_ALBUM:
			knownCard = true;
			activeMode = new AlbumMode(numTracksInFolder);
			break;
		case MODE_AUDIO_BOOK:
			knownCard = true;
			activeMode = new AudiobookMode(numTracksInFolder);
			break;
		case MODE_RANDOM:
			knownCard = true;
			activeMode = new RandomMode(numTracksInFolder);
			break;
		case MODE_ADMIN:
			adminMenu(true);
			return;
		default:
			DEBUG_PRINTLN(F("Error: Unknown mode on card"));
			myFolder->mode = MODE_INVALID;
			myFolder->folder = 0;
		}
	}

	if (knownCard) {
		activeMode->handlePlay();
	} else {
		mp3.playMp3FolderTrack(300);
		waitForTrackToFinish();
		setupCard();
	}

	// Leave card alone
	mfrc522.PICC_HaltA();
	mfrc522.PCD_StopCrypto1();
}

void adminMenu(bool adminCard) {
	int res;
	disablestandbyTimer();
	mp3.playMp3FolderTrack(401); // play ok
	waitForTrackToFinish();

	DEBUG_PRINTLN(F("=== adminMenu"));
	if (adminCard == false) {
		// wait for keys to be released
		do {
			readButtons();
		} while (upButton.isPressed() || downButton.isPressed());
		readButtons();

		// Admin menu has been locked - it still can be trigged via admin card
		if (mySettings.adminMenuLocked == 1) return;

		// Pin check
		if (mySettings.adminMenuLocked == 2) {
			uint8_t pin[4];
			mp3.playMp3FolderTrack(991);
			if (askCode(pin) == true) {
				if (memcmp(pin, mySettings.adminMenuPin, sizeof(pin)) != 0) {
					return;
				}
			} else {
				return;
			}
		}
		// Match check
		else if (mySettings.adminMenuLocked == 3) {
			uint8_t a = random(10, 20);
			uint8_t b = random(1, 10);
			uint8_t c;
			mp3.playMp3FolderTrack(992);
			waitForTrackToFinish();
			mp3.playMp3FolderTrack(a);

			if (random(1, 3) == 2) {
				// a + b
				c = a + b;
				waitForTrackToFinish();
				mp3.playMp3FolderTrack(993);
			} else {
				// a - b
				c = a - b;
				waitForTrackToFinish();
				mp3.playMp3FolderTrack(994);
			}
			waitForTrackToFinish();
			mp3.playMp3FolderTrack(b);
			DEBUG_PRINTLN(c);
			uint8_t temp = voiceMenu(255, 0, 0, false);
			if (temp != c) return;
		}
	}
	int subMenu = voiceMenu(9, 900, 900, false, 0);
	switch (subMenu) {
	case 1: // reset card
		resetCard();
		mfrc522.PICC_HaltA();
		mfrc522.PCD_StopCrypto1();
		break;
	case 2: // set max. volume
		mySettings.maxVolume = mySettings.minVolume + voiceMenu(
				30 - mySettings.minVolume,
				930,
				mySettings.minVolume,
				false,
				mySettings.maxVolume - mySettings.minVolume);
		break;
	case 3: // set min. volume
		mySettings.minVolume = voiceMenu(
				mySettings.maxVolume - 1,
				931,
				0,
				false,
				mySettings.minVolume);
		break;
	case 4: // set initial volume
		mySettings.initVolume = mySettings.minVolume - 1 + voiceMenu(
				mySettings.maxVolume - mySettings.minVolume + 1,
				932,
				mySettings.minVolume - 1,
				false,
				mySettings.initVolume - mySettings.minVolume + 1);
		break;
	case 5: // set EQ
		res = voiceMenu(6, 920, 920, false, mySettings.eq);
		if (res <= DfMp3_Eq_Bass) {
			mySettings.eq = static_cast<DfMp3_Eq>(res);
		}
		mp3.setEq(mySettings.eq);
		break;
	case 6: // set short-cuts
		res = voiceMenu(4, 940, 940, true, 0);
		if (res > 0) {
			if (setupFolder(&mySettings.shortCuts[res - 1])) {
				mp3.playMp3FolderTrack(401); // play ok
			} else {
				mp3.playMp3FolderTrack(402); // play error
			}
		} else {
			mp3.playMp3FolderTrack(402); // play error
		}
		waitForTrackToFinish();
		break;
	case 7: // set stand-by time
		switch (voiceMenu(5, 960, 960)) {
		case 1:
			mySettings.standbyTimer = 5;
			break;
		case 2:
			mySettings.standbyTimer = 15;
			break;
		case 3:
			mySettings.standbyTimer = 30;
			break;
		case 4:
			mySettings.standbyTimer = 60;
			break;
		case 5:
			mySettings.standbyTimer = 0;
			break;
		}
		setstandbyTimer();
		break;
	case 8: // reset EEPROM
		DEBUG_PRINTLN(F("Reset -> EEPROM wird gelöscht"));
		for (uint16_t i = 0; i < EEPROM.length(); i++) {
			EEPROM.update(i, 0);
		}
		resetSettings();
		mp3.playMp3FolderTrack(999);
		break;
	case 9: // lock admin menu
		res = voiceMenu(4, 980, 980, false);
		if (res == 1) {
			mySettings.adminMenuLocked = 0;
		} else if (res == 2) {
			mySettings.adminMenuLocked = 1;
		} else if (res == 3) {
			uint8_t pin[4];
			mp3.playMp3FolderTrack(991);
			if (askCode(&pin[0])) {
				memcpy(mySettings.adminMenuPin, pin, 4);
				mySettings.adminMenuLocked = 2;
			}
		} else if (res == 4) {
			mySettings.adminMenuLocked = 3;
		}
		break;
	default:
		return;
	}
	storeSettings();
}

bool askCode(uint8_t *code) {
	uint8_t x = 0;
	while (x < 4) {
		readButtons();
		if (pauseButton.pressedFor(LONG_PRESS)) {
			do {
				readButtons();
			} while (pauseButton.isPressed());
			readButtons();
			break;
		}
		if (pauseButton.wasReleased())
			code[x++] = 1;
		if (upButton.wasReleased())
			code[x++] = 2;
		if (downButton.wasReleased())
			code[x++] = 3;
	}
	return true;
}

uint8_t voiceMenu(
		int numberOfOptions,
		int startMessage,
		int messageOffset,
		bool preview,
		int defaultValue )
{
	// first key-press increases returnValue to 1 or defaultValue
	int16_t returnValue = max(0, defaultValue-1);

	// play start message
	if (startMessage != 0) {
		DEBUG_PRINT(F("=== Menu: Play message "));
		DEBUG_PRINTLN(startMessage);
		mp3.playMp3FolderTrack(startMessage);
	}

	DEBUG_PRINT(F("=== voiceMenu ("));
	DEBUG_PRINT(returnValue);
	DEBUG_PRINT(F(" / "));
	DEBUG_PRINT(numberOfOptions);
	DEBUG_PRINTLN(F(")"));
	do {
		mp3.loop();
		readButtons();
		if (pauseButton.pressedFor(LONG_PRESS)) {
			mp3.playMp3FolderTrack(802);
			waitForTrackToFinish();
			do {
				readButtons();
			} while (pauseButton.isPressed());
			readButtons();
			return defaultValue;
		}
		if (pauseButton.wasReleased()) {
			DEBUG_PRINT(F("=== Selected option "));
			DEBUG_PRINTLN(returnValue);
			if ( (returnValue > 0) && (returnValue <= numberOfOptions) ) {
				mp3.playMp3FolderTrack(401); // play ok
				waitForTrackToFinish();
				return returnValue;
			}
			mp3.playMp3FolderTrack(402); // play error
			waitForTrackToFinish();
			return defaultValue;
		}

		if (upButton.pressedFor(LONG_PRESS)) {
			returnValue = min(returnValue + 10, numberOfOptions);
			if (preview) {
				mp3.playFolderTrack(returnValue, 1);
			} else {
				mp3.playMp3FolderTrack(messageOffset + returnValue);
			}
			do {
				readButtons();
			} while (upButton.isPressed());
			readButtons();
		} else if (upButton.wasReleased()) {
			returnValue = min(returnValue + 1, numberOfOptions);
			if (preview) {
				mp3.playFolderTrack(returnValue, 1);
			} else {
				mp3.playMp3FolderTrack(messageOffset + returnValue);
			}
		}

		if (downButton.pressedFor(LONG_PRESS)) {
			returnValue = max(returnValue - 10, 1);
			if (preview) {
				mp3.playFolderTrack(returnValue, 1);
			} else {
				mp3.playMp3FolderTrack(messageOffset + returnValue);
			}
			do {
				readButtons();
			} while (downButton.isPressed());
			readButtons();
		} else if (downButton.wasReleased()) {
			returnValue = max(returnValue - 1, 1);
			if (preview) {
				mp3.playFolderTrack(returnValue, 1);
			} else {
				mp3.playMp3FolderTrack(messageOffset + returnValue);
			}
		}
	} while (true);
}

void resetCard() {
	mp3.playMp3FolderTrack(800);
	do {
		pauseButton.read();
		upButton.read();
		downButton.read();

		if (upButton.wasReleased() || downButton.wasReleased()) {
			DEBUG_PRINT(F("Abgebrochen!"));
			mp3.playMp3FolderTrack(802);
			return;
		}
	} while (!mfrc522.PICC_IsNewCardPresent());

	if (!mfrc522.PICC_ReadCardSerial())
		return;

	DEBUG_PRINT(F("Karte wird neu konfiguriert!"));
	setupCard();
}

// TODO: Umstellen auf PlayerMode
bool setupFolder(folderSettings *theFolder) {
	// Ordner abfragen
	theFolder->folder = voiceMenu(99, 301, 0, true, 0);
	if (theFolder->folder == 0) return false;

	// Wiedergabemodus abfragen
	theFolder->mode = voiceMenu(4, 310, 310, false, MODE_INVALID);
	if (theFolder->mode == MODE_INVALID) return false;

	// Admin Funktionen
	if (theFolder->mode == MODE_ADMIN_MENU) {
		theFolder->folder = 0;
		theFolder->mode = MODE_ADMIN;
	}
	return true;
}

void setupCard() {
	DEBUG_PRINTLN(F("=== setupCard"));
	nfcTagObject newCard;
	if (setupFolder(&newCard.nfcFolderSettings) == true) {
		// folder and mode selected, write to card
		writeCard(newCard);
	}
}

bool readCard(nfcTagObject *nfcTag) {
	nfcTagObject tempCard;
	MFRC522::StatusCode status = MFRC522::STATUS_ERROR;

	// Show some details of the PICC (that is: the tag/card)
	DEBUG_PRINT(F("Card UID:"));
	DEBUG_PRINTB(mfrc522.uid.uidByte, mfrc522.uid.size);
	DEBUG_PRINTLN();
	DEBUG_PRINT(F("PICC type: "));
	MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
	DEBUG_PRINTLN(mfrc522.PICC_GetTypeName(piccType));

	byte buffer[18];

	// Authenticate using key A
	if ((piccType == MFRC522::PICC_TYPE_MIFARE_MINI)
			|| (piccType == MFRC522::PICC_TYPE_MIFARE_1K)
			|| (piccType == MFRC522::PICC_TYPE_MIFARE_4K)) {
		DEBUG_PRINTLN(F("Authenticating Classic using key A..."));
		status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A,
				trailerBlock, &key, &(mfrc522.uid));
	} else if (piccType == MFRC522::PICC_TYPE_MIFARE_UL) {
		byte pACK[] = { 0, 0 }; //16 bit PassWord ACK returned by the tempCard

		// Authenticate using key A
		DEBUG_PRINTLN(F("Authenticating MIFARE UL..."));
		status = mfrc522.PCD_NTAG216_AUTH(key.keyByte, pACK);
	}

	if (status != MFRC522::STATUS_OK) {
		DEBUG_PRINT(F("PCD_Authenticate() failed: "));
		DEBUG_PRINTLN(mfrc522.GetStatusCodeName(status));
		return false;
	}

	// Read data from the block
	if ((piccType == MFRC522::PICC_TYPE_MIFARE_MINI)
			|| (piccType == MFRC522::PICC_TYPE_MIFARE_1K)
			|| (piccType == MFRC522::PICC_TYPE_MIFARE_4K)) {
		DEBUG_PRINT(F("Reading data from block "));
		DEBUG_PRINT(blockAddr);
		DEBUG_PRINTLN(F(" ..."));
		byte size = sizeof(buffer);
		status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockAddr, buffer,
				&size);
		if (status != MFRC522::STATUS_OK) {
			DEBUG_PRINT(F("MIFARE_Read() failed: "));
			DEBUG_PRINTLN(mfrc522.GetStatusCodeName(status));
			return false;
		}
	} else if (piccType == MFRC522::PICC_TYPE_MIFARE_UL) {
		byte buffer2[18];
		byte size2 = sizeof(buffer2);

		status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(8, buffer2, &size2);
		if (status != MFRC522::STATUS_OK) {
			DEBUG_PRINT(F("MIFARE_Read_1() failed: "));
			DEBUG_PRINTLN(mfrc522.GetStatusCodeName(status));
			return false;
		}
		memcpy(buffer, buffer2, 4);

		status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(9, buffer2, &size2);
		if (status != MFRC522::STATUS_OK) {
			DEBUG_PRINT(F("MIFARE_Read_2() failed: "));
			DEBUG_PRINTLN(mfrc522.GetStatusCodeName(status));
			return false;
		}
		memcpy(buffer + 4, buffer2, 4);

		status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(10, buffer2, &size2);
		if (status != MFRC522::STATUS_OK) {
			DEBUG_PRINT(F("MIFARE_Read_3() failed: "));
			DEBUG_PRINTLN(mfrc522.GetStatusCodeName(status));
			return false;
		}
		memcpy(buffer + 8, buffer2, 4);

		status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(11, buffer2, &size2);
		if (status != MFRC522::STATUS_OK) {
			DEBUG_PRINT(F("MIFARE_Read_4() failed: "));
			DEBUG_PRINTLN(mfrc522.GetStatusCodeName(status));
			return false;
		}
		memcpy(buffer + 12, buffer2, 4);
	}

	DEBUG_PRINTLN(F("Data on Card:"));
	DEBUG_PRINTB(buffer, sizeof(buffer));
	DEBUG_PRINTLN();
	DEBUG_PRINTLN();

	memcpy(&tempCard.cookie[0], &buffer[0], sizeof(cardCookie));
	tempCard.version = buffer[4];
	tempCard.nfcFolderSettings.folder = buffer[5];
	tempCard.nfcFolderSettings.mode = buffer[6];

	if (memcmp(&tempCard.cookie[0], &cardCookie[0], sizeof(cardCookie)) == 0) {
		memcpy(nfcTag, &tempCard, sizeof(nfcTagObject));
		myFolder = &nfcTag->nfcFolderSettings;
		DEBUG_PRINT(F("myFolder: "));
		DEBUG_PRINTLN(myFolder->folder);
		return true;
	}
	return false;
}

void writeCard(nfcTagObject nfcTag) {
	MFRC522::PICC_Type mifareType;
	MFRC522::StatusCode status = MFRC522::STATUS_ERROR;

	byte buffer[16] = { cardCookie[0], cardCookie[1], cardCookie[2],
			cardCookie[3], // magic cookie
			MY_NFC_TAG_VERSION, // actual version
			nfcTag.nfcFolderSettings.folder, // the folder picked by the user
			nfcTag.nfcFolderSettings.mode, // the play-back mode picked by the user
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	mifareType = mfrc522.PICC_GetType(mfrc522.uid.sak);

	// Authenticate using key B
	//authentificate with the card and set card specific parameters
	if ((mifareType == MFRC522::PICC_TYPE_MIFARE_MINI)
			|| (mifareType == MFRC522::PICC_TYPE_MIFARE_1K)
			|| (mifareType == MFRC522::PICC_TYPE_MIFARE_4K)) {
		DEBUG_PRINTLN(F("Authenticating again using key A..."));
		status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A,
				trailerBlock, &key, &(mfrc522.uid));
	} else if (mifareType == MFRC522::PICC_TYPE_MIFARE_UL) {
		byte pACK[] = { 0, 0 }; //16 bit PassWord ACK returned by the NFCtag

		// Authenticate using key A
		DEBUG_PRINTLN(F("Authenticating UL..."));
		status = mfrc522.PCD_NTAG216_AUTH(key.keyByte, pACK);
	}

	if (status != MFRC522::STATUS_OK) {
		DEBUG_PRINT(F("PCD_Authenticate() failed: "));
		DEBUG_PRINTLN(mfrc522.GetStatusCodeName(status));
		mp3.playMp3FolderTrack(402); // play error
		waitForTrackToFinish();
		return;
	}

	// Write data to the block
	DEBUG_PRINT(F("Writing data into block "));
	DEBUG_PRINT(blockAddr);
	DEBUG_PRINTLN(F(" ..."));
	DEBUG_PRINTB(buffer, 16);
	DEBUG_PRINTLN();

	if ((mifareType == MFRC522::PICC_TYPE_MIFARE_MINI)
			|| (mifareType == MFRC522::PICC_TYPE_MIFARE_1K)
			|| (mifareType == MFRC522::PICC_TYPE_MIFARE_4K)) {
		status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(blockAddr, buffer,
				16);
	} else if (mifareType == MFRC522::PICC_TYPE_MIFARE_UL) {
		byte buffer2[16];
		byte size2 = sizeof(buffer2);

		memset(buffer2, 0, size2);
		memcpy(buffer2, buffer, 4);
		status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(8, buffer2, 16);

		memset(buffer2, 0, size2);
		memcpy(buffer2, buffer + 4, 4);
		status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(9, buffer2, 16);

		memset(buffer2, 0, size2);
		memcpy(buffer2, buffer + 8, 4);
		status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(10, buffer2, 16);

		memset(buffer2, 0, size2);
		memcpy(buffer2, buffer + 12, 4);
		status = (MFRC522::StatusCode) mfrc522.MIFARE_Write(11, buffer2, 16);
	}

	if (status != MFRC522::STATUS_OK) {
		DEBUG_PRINT(F("MIFARE_Write() failed: "));
		DEBUG_PRINTLN(mfrc522.GetStatusCodeName(status));
		mp3.playMp3FolderTrack(402); // play error
	} else {
		mp3.playMp3FolderTrack(801); // play card config ok
	}
	waitForTrackToFinish();
}
