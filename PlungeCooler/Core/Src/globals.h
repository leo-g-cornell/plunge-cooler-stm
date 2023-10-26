/*
 * globals.h
 *
 *  Created on: Oct 26, 2023
 *      Author: Leo
 */

#ifndef SRC_GLOBALS_H_
#define SRC_GLOBALS_H_

#define LOG_SIZE 10000
#define MOVING_AVG_LENGTH 50
#define DISPENSE_DELAY 100 	// in ms
#define LOGGING_TIMEBASE 1	// in
extern uint32_t posLog[LOG_SIZE];
extern uint32_t log_position;
extern uint32_t running_sum;
extern uint32_t disp_pos;

#endif /* SRC_GLOBALS_H_ */
