/*
 * Backup slave used to commute the power relays in case
 * the acquisition systems cannot do it. Follow the same 
 * protocol so check the documentation on the acquisition
 * project for details.
 */

#include "Arduino.h"

#define COMMAND_LENGTH 3

#define GET_RELAY_STATE 0xA0
#define SET_RELAY 0xB0
#define RESET_RELAY 0xC0

#define RELAY_DEPLOY1 0x01
#define RELAY_DEPLOY2 0x02
#define RELAY_PAYLOAD 0x03

#define SET_DEPLOY1 12
#define RESET_DEPLOY1 11
#define FEEDBACK_DEPLOY1 23

#define SET_DEPLOY2 10
#define RESET_DEPLOY2 9
#define FEEDBACK_DEPLOY2 22

#define SET_PAYLOAD 7
#define RESET_PAYLOAD 8
#define FEEDBACK_PAYLOAD 21


// enum for the relay command decoding
enum RelayDecodeStates {
	WAIT_COMMAND,
	WAIT_RELAY_NO,
	WAIT_CHECKSUM
};
enum RelayDecodeStates r_decode_state = WAIT_COMMAND;
uint8_t r_command[COMMAND_LENGTH];

// global functions
void decode_relay_command(uint8_t* cmd, uint8_t b);
void execute_relay_command(uint8_t* cmd);

void setup() {
	Serial.begin(9600);
	pinMode(SET_DEPLOY1, OUTPUT);
	pinMode(RESET_DEPLOY1, OUTPUT);
	pinMode(FEEDBACK_DEPLOY1, INPUT_PULLUP);
	pinMode(SET_DEPLOY2, OUTPUT);
	pinMode(RESET_DEPLOY2, OUTPUT);
	pinMode(FEEDBACK_DEPLOY2, INPUT_PULLUP);
	pinMode(SET_PAYLOAD, OUTPUT);
	pinMode(RESET_PAYLOAD, OUTPUT);
	pinMode(FEEDBACK_PAYLOAD, INPUT_PULLUP);
}

void loop() {
	uint8_t b;
	if(Serial.available()) {
		b = Serial.read();
		decode_relay_command(r_command, b);
	}
}

void decode_relay_command(uint8_t* cmd, uint8_t b) {
	switch(r_decode_state) {
	case WAIT_COMMAND:
		if(b == GET_RELAY_STATE || b == SET_RELAY || b == RESET_RELAY) {
			cmd[0] = b;
			r_decode_state = WAIT_RELAY_NO;
		} else {
			// send NACK (0x00)
			Serial.write(0x00);
		}
		break;
	case WAIT_RELAY_NO:
		if(b == RELAY_DEPLOY1 || b == RELAY_DEPLOY2 || b == RELAY_PAYLOAD) {
			cmd[1] = b;
			r_decode_state = WAIT_CHECKSUM;
		} else {
			cmd[0] = 0x00;	// invalidate current command
			r_decode_state = WAIT_COMMAND;
			// send NACK (0x00)
			Serial.write(0x00);
		}
		break;
	case WAIT_CHECKSUM:
		r_decode_state = WAIT_COMMAND;
		if(b == cmd[0] + cmd[1]) {
			// command is valid
			execute_relay_command(cmd);
		} else {
			// send NACK (0x00)
			Serial.write(0x00);
		}
		break;
	}
}

void execute_relay_command(uint8_t* cmd) {
	switch(cmd[0]) {
	case GET_RELAY_STATE:
		switch(cmd[1]) {
		case RELAY_DEPLOY1:
			Serial.write(digitalRead(FEEDBACK_DEPLOY1));
			break;
		case RELAY_DEPLOY2:
			Serial.write(digitalRead(FEEDBACK_DEPLOY2));
			break;
		case RELAY_PAYLOAD:
			Serial.write(digitalRead(FEEDBACK_PAYLOAD));
			break;
		}
		return;
	case SET_RELAY:
		switch(cmd[1]) {
		case RELAY_DEPLOY1:
			digitalWrite(RESET_DEPLOY1, LOW);
			digitalWrite(SET_DEPLOY1, HIGH);
			break;
		case RELAY_DEPLOY2:
			digitalWrite(RESET_DEPLOY2, LOW);
			digitalWrite(SET_DEPLOY2, HIGH);
			break;
		case RELAY_PAYLOAD:
			digitalWrite(RESET_PAYLOAD, LOW);
			digitalWrite(SET_PAYLOAD, HIGH);
			break;
		}
		// send ACK (0x01)
		Serial.write(0x01);
		break;
	case RESET_RELAY:
		switch(cmd[1]) {
		case RELAY_DEPLOY1:
			digitalWrite(SET_DEPLOY1, LOW);
			digitalWrite(RESET_DEPLOY1, HIGH);
			break;
		case RELAY_DEPLOY2:
			digitalWrite(SET_DEPLOY2, LOW);
			digitalWrite(RESET_DEPLOY2, HIGH);
			break;
		case RELAY_PAYLOAD:
			digitalWrite(SET_PAYLOAD, LOW);
			digitalWrite(RESET_PAYLOAD, HIGH);
			break;
		}
		// send ACK (0x01)
		Serial.write(0x01);
		break;
	}
}

