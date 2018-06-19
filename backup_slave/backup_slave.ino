/*
 * Backup slave used to commute the power relays in case
 * the acquisition systems cannot do it. Follow the same
 * protocol so check the documentation on the acquisition
 * project for details.
 */

#include "Arduino.h"

// protocol used can be simplified to use only one byte, uncomment for the
// simplified version
#define SIMPLE_PROTOCOL

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

#define ACK 0x11
#define NACK 0x10

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
void decode_simple_relay_command(uint8_t b);

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
	digitalWrite(SET_DEPLOY1, LOW);
	digitalWrite(RESET_DEPLOY1, LOW);
	digitalWrite(SET_DEPLOY2, LOW);
	digitalWrite(RESET_DEPLOY2, LOW);
	digitalWrite(SET_PAYLOAD, LOW);
	digitalWrite(RESET_PAYLOAD, LOW);
}

void loop() {
	uint8_t b;
	if(Serial.available()) {
		b = Serial.read();
#ifdef SIMPLE_PROTOCOL
		decode_simple_relay_command(b);
#else
		decode_relay_command(r_command, b);
#endif
	}
}

void decode_relay_command(uint8_t* cmd, uint8_t b) {
	switch(r_decode_state) {
	case WAIT_COMMAND:
		if(b == GET_RELAY_STATE || b == SET_RELAY || b == RESET_RELAY) {
			cmd[0] = b;
			r_decode_state = WAIT_RELAY_NO;
			Serial.write(ACK);
		} else {
			Serial.write(NACK);
			// invalidate first byte
			cmd[0] = 0xFF;
		}
		break;
	case WAIT_RELAY_NO:
		if(b == RELAY_DEPLOY1 || b == RELAY_DEPLOY2 || b == RELAY_PAYLOAD) {
			cmd[1] = b;
			r_decode_state = WAIT_CHECKSUM;
			Serial.write(ACK);
		} else {
			cmd[0] = 0x00;	// invalidate current command
			r_decode_state = WAIT_COMMAND;
			Serial.write(NACK);
		}
		break;
	case WAIT_CHECKSUM:
		r_decode_state = WAIT_COMMAND;
		if(b == cmd[0] + cmd[1]) {
			// command is valid
			execute_relay_command(cmd);
		} else {
			Serial.write(NACK);
		}
		break;
	}
}

void decode_simple_relay_command(uint8_t b) {
	// byte comes as xxxxyyyy where:
	//	xxxx is the command
	//	yyyy is the relay number
	uint8_t cmd[2];
	// get command byte
	cmd[0] = 0xF0 & b;
	// get relay number byte
	cmd[1] = 0x0F & b;
	// validate command byte
	if(!(cmd[0] == GET_RELAY_STATE || cmd[0] == SET_RELAY || cmd[0] == RESET_RELAY)) {
		// command byte is invalid
		Serial.write(NACK);
		Serial.write(cmd[0]);
		return;
	}
	// validate relay number
	if(!(cmd[1] == RELAY_DEPLOY1 || cmd[1] == RELAY_DEPLOY2 || cmd[1] == RELAY_PAYLOAD)) {
		Serial.write(NACK);
		Serial.write(cmd[1]);
		return;
	}
	// execute command
	execute_relay_command(cmd);
}

void execute_relay_command(uint8_t* cmd) {
	int analog_value;
	switch(cmd[0]) {
	case GET_RELAY_STATE:
		for(int i = 0; i < 10; i++) {
			switch(cmd[1]) {
			case RELAY_DEPLOY1:
				analog_value = analogRead(FEEDBACK_DEPLOY1);
				break;
			case RELAY_DEPLOY2:
				analog_value = analogRead(FEEDBACK_DEPLOY2);
				break;
			case RELAY_PAYLOAD:
				analog_value = analogRead(FEEDBACK_PAYLOAD);
				break;
			}
			if(analog_value >= 750) {
				Serial.write(0x01);
				return;
			} else if(analog_value <= 250) {
				Serial.write(0x00);
				return;
			} else {
				continue;
			}
			return;
		}
		// no valid analog value could be read
		Serial.write(NACK);
		Serial.write(0x69);
		break;
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
		Serial.write(ACK);
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
		Serial.write(ACK);
		break;
	}
}
