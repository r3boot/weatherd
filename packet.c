
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "packet.h"
#include "logging.h"
#include "serial.h"
#include "queue.h"

uint8_t MSG_START = 0xfc;
uint8_t MSG_END = 0xcf;
uint8_t MSG_NULL = 0xfd;

uint8_t TEMPERATURE = 0x01;
uint8_t PRESSURE = 0x02;
uint8_t HUMIDITY = 0x03;
uint8_t RAINFALL = 0x04;
uint8_t WINDSPEED = 0x05;
uint8_t WINDDIRECTION = 0x06;
uint8_t LIGHT = 0x07;
uint8_t HOSTID = 0x42;

uint8_t serial_prev = 0;
uint8_t prev = 0;
uint8_t cur_byte = 0;

int in_msg = 0;
uint8_t multiplier = 0;
int i = 0;
int num_starts = 0;
int num_ends = 0;

int host_id = 0;
float temperature = 0;
float humidity = 0;
float rainfall = 0;
float wind_speed = 0;
long pressure = 0;
long wind_direction = 0;
float light = 0;
long checksum = 0;

int packet_checksum = 0;

struct s_packet packet;
struct s_packet_stats packet_stats;

struct s_packet get_packet() {
	return packet;
}

void packet_reset_vars() {
	in_msg = -1;
	multiplier = 0;
	i = 0;
	host_id = 0;
	temperature = 0;
	humidity = 0;
	rainfall = 0;
	wind_speed = 0;
	wind_direction = 0;
	light = 0;
	checksum = 0;
	packet_checksum = 0;
	num_starts = 0;
	num_ends = 0;

	packet.host_id = 0;
	packet.temperature = 0;
	packet.humidity = 0;
	packet.rainfall = 0;
	packet.wind_speed = 0;
	packet.wind_direction = 0;
	packet.pressure = 0;
	packet.light = 0;

}

int packet_process_byte(uint8_t byte) {

	prev = ~serial_prev;
	if (byte == prev) {
		cur_byte = serial_prev;
		packet_stats.total_bytes += 1;

		if (cur_byte == MSG_START) {
			packet_stats.msg_start += 1;
			packet_stats.valid_bytes += 1;
			num_starts += 1;
			if (num_starts < 2) {
				return -1;
			}
		} else if (cur_byte == MSG_END) {
			packet_stats.msg_end += 1;
			packet_stats.valid_bytes += 1;
			num_ends += 1;
			if (num_ends < 2) {
				return -1;
			}
		}

		if (num_starts >= 2) {
			// MSG_START
			packet_reset_vars();
			in_msg = 1;
		} else if (num_ends >= 2) {
			// MSG_END
			packet_stats.total_packets += 1;
			if (checksum != packet_checksum) {
				printf("received packet with invalid checksum (expected: %d, got %d)\n", (unsigned int)checksum, packet_checksum);
				packet_stats.invalid_packet_checksum += 1;
			} else {
				// valid packet found
				packet_stats.valid_packets += 1;

				packet.host_id = host_id;
				packet.temperature = temperature;
				packet.pressure = pressure;
				packet.humidity = humidity;
				packet.light = light;
				packet.wind_speed = wind_speed;
				packet.wind_direction = wind_direction;
				packet.rainfall = rainfall;

				return 0;

			}
			packet_reset_vars();
		} else if  (i > 16) {
			packet_stats.packet_too_large += 1;

			packet_reset_vars();
		} else if ((in_msg == 1) && (i <= 16)) {
			switch (i) {
				case 0:
					host_id = cur_byte;
					checksum += cur_byte;
					packet_stats.data_bytes += 1;
					packet_stats.valid_bytes += 1;
					break;
				case 1: case 3: case 5: case 7: case 9: case 11: case 13:
					if (cur_byte == MSG_NULL) {
						multiplier = 0;
					} else {
						multiplier = cur_byte;
					}
					checksum += cur_byte;
					packet_stats.data_bytes += 1;
					packet_stats.valid_bytes += 1;
					break;
				case 15:
					multiplier = cur_byte;
					packet_stats.data_bytes += 1;
					packet_stats.valid_bytes += 1;
					break;
				case 2:
					temperature = ((multiplier * 255) + cur_byte) / 100.0;
					checksum += cur_byte;
					packet_stats.data_bytes += 1;
					packet_stats.valid_bytes += 1;
					break;
				case 4:
					pressure = (multiplier * 255) + cur_byte;
					checksum += cur_byte;
					packet_stats.data_bytes += 1;
					packet_stats.valid_bytes += 1;
					break;
				case 6:
					humidity = ((multiplier * 255) + cur_byte) / 100.0;
					checksum += cur_byte;
					packet_stats.data_bytes += 1;
					packet_stats.valid_bytes += 1;
					break;
				case 8:
					light = (multiplier * 255) + cur_byte / 100.0;
					checksum += cur_byte;
					packet_stats.data_bytes += 1;
					packet_stats.valid_bytes += 1;
					break;
				case 10:
					if (multiplier > 0) {
						wind_speed = ((multiplier * 255) + cur_byte) / 100.0;
					} else {
						wind_speed = cur_byte / 100.0;
					}
					checksum += cur_byte;
					packet_stats.data_bytes += 1;
					packet_stats.valid_bytes += 1;
					break;
				case 12:
					wind_direction = (multiplier * 255) + cur_byte;
					checksum += cur_byte;
					packet_stats.data_bytes += 1;
					packet_stats.valid_bytes += 1;
					break;
				case 14:
					rainfall = ((multiplier * 255) + cur_byte) / 100.0;
					checksum += cur_byte;
					packet_stats.data_bytes += 1;
					packet_stats.valid_bytes += 1;
				case 16:
					packet_checksum = (multiplier * 255) + cur_byte;
					break;
				default:
					log_debug("out-of-bounds packet received");
					packet_reset_vars();
					packet_stats.oob_bytes += 1;
					break;
			}

			i += 1;
		} else {
			printf("unknown byte received: 0x%02x\n", cur_byte);
			packet_stats.unknown_bytes += 1;
			packet_reset_vars();
		}
	} else {
		serial_prev = byte;
		cur_byte = 0;
	}

	return -1;
}

void reset_packet_stats() {
	packet_stats.total_packets = 0;
	packet_stats.total_bytes = 0;
	packet_stats.valid_packets= 0;
	packet_stats.valid_bytes = 0;
	packet_stats.invalid_packet_checksum = 0;
	packet_stats.packet_too_large = 0;
	packet_stats.oob_bytes = 0;
	packet_stats.unknown_bytes = 0;
	packet_stats.msg_start = 0;
	packet_stats.msg_end = 0;
	packet_stats.data_bytes = 0;
}

void print_packet_stats() {
	log_debug("STATISTICS");
	printf("total packets received:     %d\n", packet_stats.total_packets);
	printf("total valid packets:        %d\n", packet_stats.valid_packets);
	printf("packetloss:                 %.02f%%\n", ((100.0 / packet_stats.total_packets) * (packet_stats.total_packets - packet_stats.valid_packets)));
	printf("total bytes received:       %d\n", packet_stats.total_bytes);
	printf("total valid bytes:          %d\n", packet_stats.valid_bytes);
	printf("byteloss:                   %.02f%%\n", ((100.0 / packet_stats.total_bytes) * (packet_stats.total_bytes - packet_stats.valid_bytes)));
	printf("pkts with invalid checksum: %d\n", packet_stats.invalid_packet_checksum);
	printf("too large packets:          %d\n", packet_stats.packet_too_large);
	printf("# of out-of-bound bytes:    %d\n", packet_stats.oob_bytes);
	printf("# of unknown bytes:         %d\n", packet_stats.unknown_bytes);
	printf("# of msg_start bytes:       %d\n", packet_stats.msg_start);
	printf("# of msg_end bytes:         %d\n", packet_stats.msg_end);
	printf("# of data bytes:            %d\n", packet_stats.data_bytes);
	printf("payload percentage:         %.02f%%\n\n", (100.0 / (packet_stats.valid_bytes - packet_stats.oob_bytes - packet_stats.unknown_bytes)) * packet_stats.data_bytes );
}
