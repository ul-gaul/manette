/*
 * Program used to control from a serial bus the state of relays in the
 * avionics systems of the rocket. Green and Red LEDs are used to indicate the
 * state of the relays, green indicating the relay is set (power on) and red
 * indicating the relay is reset (power off)
 *
 * The protocol used for communication is UART 8N1.
 * First, a command byte is sent, then a data byte and finally a checksum byte
 * The commands are GET_RELAY_STATE, SET_RELAY, RESET_RELAY
 * The data byte is always the number of the relay to apply the command to.
 * The slave replies with an ACK or NACK message si the command was executed,
 * or with the state of the concerned relay in the case of a GET_RELAY_STATE.
 *
 */

#include <Arduino.h>

// button related defines
#define PIN_BUTTON_2 4
#define PIN_BUTTON_3 5
#define PIN_BUTTON_4 7
#define PIN_BUTTON_5 9
#define PIN_BUTTON_6 10  // currently not used

// LED related defines
#define PIN_LED_1_7 3
#define PIN_LED_2_8 6
#define PIN_LED_3_9 8
#define PIN_LED_4_10 11
#define PIN_LED_5_11 12

// serial related defines
#define BAUD_RATE 9600

// UART protocol related defines
#define COMMAND_LENGTH 3

#define GET_RELAY_STATE 0xA0
#define SET_RELAY 0xB0
#define RESET_RELAY 0xC0

#define RELAY_ACQUISITION 0x00
#define RELAY_DEPLOYMENT1 0x01
#define RELAY_DEPLOYMENT2 0x02
#define RELAY_PAYLOAD 0x03

#define REPLY_TIMEOUT 100    // timeout in milliseconds

// global variables
int relay_states[4];


// function declarations
int8_t get_relay_state(uint8_t relay_number);
int8_t set_relay(uint8_t relay_number);
int8_t reset_relay(uint8_t relay_number);
int8_t send_command(uint8_t* cmd);
int8_t get_led_from_relay(uint8_t relay_number);
int8_t get_relay_from_button(uint8_t button_number);
int8_t get_button_from_relay(uint8_t relay_number);

void setup() {
    // pin configuration
    pinMode(PIN_BUTTON_2, INPUT_PULLUP);
    pinMode(PIN_BUTTON_3, INPUT_PULLUP);
    pinMode(PIN_BUTTON_4, INPUT_PULLUP);
    pinMode(PIN_BUTTON_5, INPUT_PULLUP);
    pinMode(PIN_BUTTON_6, INPUT_PULLUP);
    pinMode(PIN_LED_1_7, OUTPUT);
    pinMode(PIN_LED_2_8, OUTPUT);
    pinMode(PIN_LED_3_9, OUTPUT);
    pinMode(PIN_LED_4_10, OUTPUT);
    pinMode(PIN_LED_5_11, OUTPUT);
    digitalWrite(PIN_LED_1_7, LOW);
    digitalWrite(PIN_LED_2_8, HIGH);
    digitalWrite(PIN_LED_3_9, LOW);
    digitalWrite(PIN_LED_4_10, HIGH);
    digitalWrite(PIN_LED_5_11, LOW);
    // serial configuration
    Serial.begin(BAUD_RATE);
    // get current relay states
     relay_states[0] = get_relay_state(RELAY_ACQUISITION);
     relay_states[1] = get_relay_state(RELAY_DEPLOYMENT1);
     relay_states[2] = get_relay_state(RELAY_DEPLOYMENT2);
     for(int i = 0; i < 3; i++) {
         if(relay_states[i] != -1) {
             digitalWrite(get_led_from_relay(i), relay_states[i]);
         } else {
             digitalWrite(get_led_from_relay(i), LOW);
             relay_states[i] = 0;
         }
	 delay(20);
     }
}

void loop() {
    int8_t state;
    // probe all the buttons
    for(uint8_t c = 0; c < 0x04; c++) {
        // check for button press (LOW value on button pin)
        if(!digitalRead(get_button_from_relay(c))) {
            // check current state and toggle it
            if(relay_states[c]) {
                // relay is on, turn it off
                state = reset_relay(c);
            } else {
                // relay is off, turn it on
                state = set_relay(c);
            }
            // check reply and apply LED pattern if error
            if(state == -1) {
                // timeout occured
                for(int i = 0; i < 10; i++) {
                    if(i % 2 == 0) {
                        digitalWrite(get_led_from_relay(c), HIGH);
                    } else {
                        digitalWrite(get_led_from_relay(c), LOW);
                    }
                    delay(100);
                }
                continue;
            } else if (state == 0) {
                // received NACK
                for(int i = 0; i < 4; i++) {
                    if(i % 2 == 0) {
                        digitalWrite(get_led_from_relay(c), HIGH);
                    } else {
                        digitalWrite(get_led_from_relay(c), LOW);
                    }
                    delay(250);
                }
                continue;
            }
	    delay(10);
            // get the state of the relay
            state = get_relay_state(c);
            // check reply and apply LED pattern if error
            if(state == -1) {
                // timeout occured
                for(int i = 0; i < 10; i++) {
                    if(i % 2 == 0) {
                        digitalWrite(get_led_from_relay(c), HIGH);
                    } else {
                        digitalWrite(get_led_from_relay(c), LOW);
                    }
                    delay(100);
                }
            } else {
                if(state == 0x01) {
                    digitalWrite(get_led_from_relay(c), HIGH);
                } else if (state == 0x00) {
                    digitalWrite(get_led_from_relay(c), LOW);
                }
                relay_states[c] = state;
            }
        }
    }
    delay(100);	// debounce
}

int8_t get_relay_state(uint8_t relay_number) {
    uint8_t reply;
    uint8_t cmd[COMMAND_LENGTH];
    if(relay_number == RELAY_ACQUISITION ||
        relay_number == RELAY_DEPLOYMENT1 ||
        relay_number == RELAY_DEPLOYMENT2 ||
        relay_number == RELAY_PAYLOAD) {
        // relay number is valid, creating command
        cmd[0] = GET_RELAY_STATE;
        cmd[1] = relay_number;
        cmd[2] = cmd[0] + cmd[1];
    } else {
        return -2;
    }
    return send_command(cmd);
}

int8_t set_relay(uint8_t relay_number) {
    uint8_t cmd[COMMAND_LENGTH];
    if(relay_number == RELAY_ACQUISITION ||
        relay_number == RELAY_DEPLOYMENT1 ||
        relay_number == RELAY_DEPLOYMENT2 ||
        relay_number == RELAY_PAYLOAD) {
        // relay number is valid, creating command
        cmd[0] = SET_RELAY;
        cmd[1] = relay_number;
        cmd[2] = cmd[0] + cmd[1];
    } else {
        return -1;
    }
    // reply is either:
    // -1 (timeout)
    // 1 (ACK)
    // 0 (NACK)
    return send_command(cmd);
}

int8_t reset_relay(uint8_t relay_number) {
    uint8_t cmd[COMMAND_LENGTH];
    if(relay_number == RELAY_ACQUISITION ||
        relay_number == RELAY_DEPLOYMENT1 ||
        relay_number == RELAY_DEPLOYMENT2 ||
        relay_number == RELAY_PAYLOAD) {
        // relay number is valid, creating command
        cmd[0] = RESET_RELAY;
        cmd[1] = relay_number;
        cmd[2] = cmd[0] + cmd[1];
    } else {
        return -1;
    }
    // reply is either:
    // -1 (timeout)
    // 1 (ACK)
    // 0 (NACK)
    return send_command(cmd);
}

int8_t send_command(uint8_t* cmd) {
    uint8_t read;
    for(int i = 0; i < COMMAND_LENGTH; i++) {
        Serial.write(cmd[i]);
    }
    // wait for reply from acquisition system
    for(int i = 0; i < REPLY_TIMEOUT; i++) {
        delay(1);   // wait 1 ms
        if(Serial.available()) {
            return Serial.read();
        }
    }
    // timeout occured
    return -1;
}

int8_t get_led_from_relay(uint8_t relay_number) {
    if(relay_number == 0x00) {
        return PIN_LED_1_7;
    } else if(relay_number == 0x01) {
        return PIN_LED_2_8;
    } else if(relay_number == 0x02) {
        return PIN_LED_3_9;
    } else if(relay_number == 0x03) {
        return PIN_LED_4_10;
    }
    return -1;
}

int8_t get_relay_from_button(uint8_t button_number) {
    if(button_number == PIN_BUTTON_2) {
        return 0x00;
    } else if(button_number == PIN_BUTTON_3) {
        return 0x01;
    } else if(button_number == PIN_BUTTON_4) {
        return 0x02;
    } else if(button_number == PIN_BUTTON_5) {
        return 0x03;
    }
    return -1;
}

int8_t get_button_from_relay(uint8_t relay_number) {
    if(relay_number == 0x00) {
        return PIN_BUTTON_2;
    } else if(relay_number == 0x01) {
        return PIN_BUTTON_3;
    } else if(relay_number == 0x02) {
        return PIN_BUTTON_4;
    } else if(relay_number == 0x03) {
        return PIN_BUTTON_5;
    }
    return -1;
}
