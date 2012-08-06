#!/usr/bin/env python

from __future__ import print_function

import Queue
import argparse
import logging
import logging.handlers
import os
import pwd
import select
import serial
import subprocess
import sys
import threading
import time

__appname__ = 'weatherd.py'
__version__ = 0.1
__author__ = 'Lex van Roon <r3boot@r3blog.nl>'

devices = {
	12: "weatherstation"
}

MSG_START = 0x55
MSG_END = 0xCC

TEMPERATURE = 0x01
PRESSURE = 0x02
HUMIDITY = 0x03
RAINFALL = 0x04
WINDSPEED = 0x05
WINDDIRECTION = 0x06

_d_daemonize = False
_d_verbose = False
_d_line = '/dev/tty01'
_d_speed = 57600
_d_gpio = 'gpio1'
_d_gpio_pin = 'ws_reset'
_d_reset = False
_d_user = 'nobody'
_d_chroot = '/var/empty'
_d_logfile = '/var/log/weatherd.log'
_d_loglevel = 'INFO'
_d_stdout = True
_d_packetlog = False
_d_averagelog = False
_d_graphite = False

class Graphing(threading.Thread):
    pass

class WeatherStationPredictor(threading.Thread):
    _illuminance = {
        0.02:   "Moonless clear night sky",
        1:      "Full moon on a clear night",
        3.4:    "Dark limit of civil twilight under a clear sky",
        100:    "Very dark overcast day",
        500:    "Sunrise or sunset on a clear day",
        1000:   "Overcast day",
        25000:  "Full daylight",
        130000: "Direct sunlight",
    }
    _illuminance_keys = _illuminance.keys()
    _illuminance_keys.sort()

    def __init__(self, parent):
        threading.Thread.__init__(self)
        self.parent = parent
        self.setDaemon(True)
        self.keep_running = True

        self.start()

    def run(self):
        while self.keep_running:
            averages = self.parent.predictor_q.get()
            for lux in self._illuminance_keys:
                if averages['lux'] <= lux:
                    logger.debug(self._illuminance[lux])
                    break
            self.parent.predictor_q.task_done()

class WeatherStationDistributor(threading.Thread):
    def __init__(self, parent):
        threading.Thread.__init__(self)
        self.parent = parent
        self.setDaemon(True)
        self.keep_running = True

        self.average = {
            'temperature': 0,
            'pressure': 0,
            'humidity': 0,
            'lux': 0,
            'anemo': 0,
            'rain': 0,
            'direction': 0
        }

        self.samples = {
            'temperature': [0,0],
            'pressure': [0,0],
            'humidity': [0,0],
            'lux': [0,0],
            'anemo': [0,0],
            'rain': [0,0],
            'direction': [0,[]]
        }

        self.direction = {
            0:  'unknown',
            1:  'N',
            2:  'NE',
            3:  'E',
            4:  'SE',
            5:  'S',
            6:  'SW',
            7:  'W',
            8:  'NW'
        }

        self.total_samples = 0
        self.all_values = ['temperature', 'pressure', 'humidity', 'lux', 'anemo', 'direction', 'rain']

        self.predictor_q = Queue.Queue()
        self.wsp = WeatherStationPredictor(self)

        self.start()

    def run(self):
        t_start = time.time()

        while self.keep_running:
            packet = self.parent.q.get()
            for value in self.all_values:
                if value == 'direction':
                    self.samples[value][0] += 1
                    self.samples[value][1].append(packet[value])
                else:
                    self.samples[value][0] += 1
                    self.samples[value][1] += packet[value]

            self.total_samples += 1
            self.parent.q.task_done()

            t_now = time.time()
            t_diff = t_now - t_start
            if t_diff >= 60:
                logger.debug('calculating averages over %d seconds based on %s samples' % (int(t_diff), self.total_samples))

                for value in self.all_values:
                    if value == 'direction':
                        highest = [0,0]
                        for k in self.samples[value][1]:
                            total = self.samples[value][1].count(k)
                            if highest == [0,0]:
                                highest = [k, total]
                            elif total > highest[1]:
                                highest = [k, total]

                        self.average[value] = highest[0]

                        self.samples[value] = [0, []]
                    else:
                        self.average[value] = self.samples[value][1] / self.samples[value][0]
                        self.samples[value] = [0,0]

                self.predictor_q.put(self.average)

                logger.debug('temperature: %.01f C' % self.average['temperature'])
                logger.debug('pressure: %s Pa' % self.average['pressure'])
                logger.debug('humidity: %.02f%%' % self.average['humidity'])
                logger.debug('light: %.002f lux' % self.average['lux'])
                logger.debug('wind: %s KM/h' % (((self.average['anemo'])/1000)*60))
                logger.debug('rain: %s MM/s' % self.average['rain'])
                logger.debug('direction: %s' % self.direction[self.average['direction']])

                t_start = time.time()
                self.total_samples = 0

class WeatherStationReceiver:
    def __init__(self, tty, speed):
        self.tty = tty
        self.speed = speed
        self.total_pkts = 0
        self.good_pkts = 0
        self.bad_serial_failed = 0
        self.bad_extract = 0
        self.bad_host_id = 0
        self.bad_temperature = 0
        self.bad_pressure = 0
        self.bad_humidity = 0
        self.bad_lux = 0
        self.bad_anemo = 0
        self.bad_rain = 0
        self.bad_direction = 0
        self.bad_checksum = 0
        self.bad_corrupt = 0
        self.unknown_host_id = 0
        self.packet_loss = 100
        self.keep_running = True

        self.t_byte = -1

        self.q = Queue.Queue()
        self.wsd = WeatherStationDistributor(self)

    def __destroy__(self):
        self.wsa.keep_running = False

    def packet_warning(self, msg):
        if self.good_pkts == 0:
            self.packet_loss = 100.0
        else:
            self.packet_loss = (100.0 / self.total_pkts) * (self.bad_serial_failed + self.bad_extract + self.bad_host_id + self.bad_temperature + self.bad_pressure + self.bad_humidity + self.bad_lux + self.bad_anemo + self.bad_anemo + self.bad_rain + self.bad_direction + self.bad_checksum + self.bad_corrupt)
        logger.warning('%s (%.02f%% loss)' % (msg, self.packet_loss))

    def setup_serial(self):
        self.port = serial.Serial(self.tty, self.speed)
        self.fd = self.port.fileno()
        logger.debug('%s %s (%s%s%s)' % (self.tty, self.speed, self.port.bytesize, self.port.parity, self.port.stopbits))

    def parse_packet(self, raw_packet):
        self.total_pkts += 1

	raw_packet = raw_packet.split("#")[1]
	raw_packet = raw_packet.split("$")[0]

        try:
            (raw_host_id, raw_temperature, raw_pressure, raw_humidity, raw_light, raw_anemo, raw_direction, raw_rain, raw_checksum) = raw_packet.split(",")
        except:
            self.bad_extract += 1
            self.packet_warning('failed to extract packet data')
            logger.debug('received %s' % raw_packet)
            return

        try:
            host_id = int(raw_host_id)
        except ValueError:
            self.bad_host_id += 1
            self.packet_warning('host_id is not an integer')
            return

        try:
            temperature = float(raw_temperature)
        except ValueError:
            self.bad_temperature += 1
            self.packet_warning('temperature is not a float')
            return

        try:
            pressure = int(raw_pressure)
        except ValueError:
            self.bad_pressure += 1
            self.packet_warning('pressure is not an integer')
            return

        try:
            humidity = float(raw_humidity)
        except ValueError:
            self.bad_humidity += 1
            self.packet_warning('humidity is not a float')
            return

        try:
            lux = float(raw_light)
        except ValueError:
            self.bad_lux += 1
            self.packet_warning('lux is not an integer')
            return
        
        try:
            anemo = float(raw_anemo)
        except ValueError:
            self.bad_anemo += 1
            self.packet_warning('anemo is not a float')
            return

        try:
            rain = float(raw_rain)
        except ValueError:
            self.bad_rain += 1;
            self.packet_warning('rain is not a float')
            return

        try:
            direction = int(raw_direction)
        except ValueError:
            self.bad_direction += 1;
            self.packet_warning('direction is not an integer')

        try:
            packet_checksum = float(raw_checksum)
        except ValueError:
            self.bad_checksum += 1
            self.packet_warning('packet_checksum is not an integer')
            return

        checksum = host_id + temperature + pressure + humidity + lux + anemo + rain + direction
        if not int(checksum) == int(packet_checksum):
            self.bad_corrupt += 1
            self.packet_warning('packet checksum is invalid (%s %s)' % (packet_checksum, checksum))
            return

        if not int(host_id) in devices.keys():
            self.unknown_host_id += 1
            self.packet_warning('unknown host_id received')
            return

        self.good_pkts += 1
        return {
            'host': devices[host_id],
            'temperature': temperature,
            'pressure': pressure,
            'humidity': humidity,
            'lux': lux,
            'anemo': anemo,
            'rain': rain,
            'direction': direction
        }

    def process(self, byte):
        print("processing: %s" % byte)
        if self.t_byte == -1:
            self.t_byte = byte
            return

        tmp = ~self.t_byte
        if byte == tmp:
            print("received: %s (%s)" % (self.t_byte, hex(self.t_byte)))
            self.t_byte = -1
        else:
            self.t_byte = byte

    def run(self):
        while self.keep_running:
            raw_packet = None
            select.select([self.fd], [], [])

            try:
                raw_packet = self.port.readline().strip()
            except:
                self.bad_serial_failed += 1
                self.packet_warning('failed to read data from serial port')
                continue

            if raw_packet:
                packet = self.parse_packet(raw_packet)
            if packet:
                self.q.put(packet)

class GPIO:
    def __init__(self, *args, **kwargs):
        self._gpio = False
        self._gpio_pin = False

        if not 'device' in kwargs:
            logger.error('GPIO() requires a gpio device to work on')
        else:
            self._gpio = kwargs['gpio']

        if not 'pin' in kwargs:
            logger.error('GPIO() requires a gpio pin to work on')
        else:
            self._gpio_pin = kwargs['gpio_pin']

        if self._gpio and self._gpio_pin:
            logger.info('Reset working through pin %s on %s' % (self._gpio_pin, self._gpio))
        else:
            logger.info('Reset functionality disabled')

    def __destroy__(self, *args, **kwargs):
        self.pin_set(0)

    def pin_set(self, *args, **kwargs):
        if not self._gpio and not self._gpio_pin:
            logger.error('Failed to set gpio pin')
            return

        if len(args) == 0:
            logger.error('GPIO.pin_set() requires a value')
        elif not isinstance(args[0], int):
            logger.error('GPIO.pin_set() value needs to be int')

        cmd = ['/usr/sbin/gpioctl', '-q', self._gpio, self._gpio_pin, args[0]]
        gpioctl = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        gpioctl.wait(gpioctl.pid())

    def reset(self):
        self.pin_set(1)
        time.sleep(0.05)
        self.pin_set(0)

class Serial:
    def __init__(self, *args, **kwargs):
        if 'line' in kwargs:
            self._line = kwargs['line']
        else:
            logger.error('Serial() requires a line to work on')
            self._line = False

        if 'speed' in kwargs:
            self._speed = kwargs['speed']
        else:
            logger.error('Serial() requires a baudrate to work with')
            self._speed = False

        self._port = False
        self._fd = False

    def __destroy__(self):
        self.close()

    def connect(self):
        if self._port:
            logger.warning('%s is already opened' % self._port)
            return

        if self._port and self._speed:
            self._port = serial.Serial(self._port, self._speed)
            self._fd = self.port.fileno()
            logger.debug('%s %s (%s%s%s)' % (self._tty, self._speed, self.port.bytesize, self.port.parity, self.port.stopbits))
        else:
            logger.error('Unable to open serial port')

    def close(self):
        if self._port:
            logger.debug('closing serial port %s' % self._port)
            self._fd = False
            self._port.close()
            self._port = False
        else:
            logger.debug('serial port %s already closed' % self._port)

def setup_logging(opts):
    ## Setup logging
    loglevel = getattr(logging, opts.loglevel.upper(), None)
    if not isinstance(loglevel, int):
        raise ValueError('Invalid log level: %s' % loglevel)

    log_formatter = logging.Formatter('%(asctime)-6s: %(filename)s[%(process)d] %(funcName)s - %(message)s')

    console_logger = logging.StreamHandler()
    if opts.verbose:
        console_logger.setLevel(logging.DEBUG)
    else:
        console_logger.setLevel(logging.ERROR)
    console_logger.setFormatter(log_formatter)
    logging.getLogger('').addHandler(console_logger)

    file_logger = logging.handlers.RotatingFileHandler(filename=opts.logfile,
            maxBytes=1024*1024, backupCount=9)
    file_logger.setLevel(loglevel)
    file_logger.setFormatter(log_formatter)
    logging.getLogger('').addHandler(file_logger)

    logger = logging.getLogger('')
    logger.setLevel(logging.DEBUG)

    logger.debug('logging initialized')
    return logger

def setuid(opts):
    logger.debug('switching to user %s' % opts.user)
    try:
        user = pwd.getpwnam(opts.user)
    except KeyError:
        logger.error('getpwnam failed for %s' % opts.user)
    # os.setsid()
    os.setgid(user.pw_gid)
    os.setuid(user.pw_uid)
    os.umask(0)

def double_fork(opts):
    logger.debug('daemonizing')

    try:
        pid = os.fork()
        if pid > 0:
            sys.exit(0)
    except OSError, e:
        logger.error('fork#1 failed: %d (%s)' % (e.errno, e.strerror))

    logger.debug('chrooting to %s' % opts.chrootdir)
    setuid(opts)
    os.chroot(opts.chrootdir)
    os.chdir('/')

    try:
        pid = os.fork()
        if pid > 0:
            logger.debug('daemonized')
            sys.exit(0)
    except OSError, e:
        logger.error('fork #2 failed: %d (%s)' % (e.errno, e.strerror))
        sys.exit(1)

if __name__ == '__main__':
    # Parse arguments
    parser = argparse.ArgumentParser()

    service_opts = parser.add_argument_group('Service options')
    service_opts.add_argument('-d', dest='daemonize', type=str,
        nargs='?', default=_d_daemonize, const=True,
        help='Run as a daemon (%s)' % _d_daemonize)
    service_opts.add_argument('-v', dest='verbose', type=str,
        nargs='?', default=_d_verbose, const=True,
        help='Show verbose output (%s)' % _d_verbose)
    service_opts.add_argument('-u', dest='user', type=str,
        nargs=1, default=_d_user,
        help='User to run as (%s)' % _d_user)
    service_opts.add_argument('-c', dest='chroot', type=str,
        nargs=1, default=_d_chroot,
        help='Chroot to this directory (%s)' % _d_chroot)

    serial_opts = parser.add_argument_group('Serial options')
    serial_opts.add_argument('-l', dest='line', type=str,
        nargs=1, default=_d_line,
        help='Serial port to listen on (%s)' % _d_line)
    serial_opts.add_argument('-s', dest='speed', type=int,
        nargs=1, default=_d_speed,
        help='Baudrate to use (%s)' % _d_speed)
    serial_opts.add_argument('-r', dest='reset', type=str,
        nargs='?', default=_d_reset,
        help='Reset the serial port (%s)' % _d_reset)
    serial_opts.add_argument('--gpio', dest='gpio', type=str,
        nargs=1, default=_d_gpio,
        help='GPIO chip to use for reset (%s)' % _d_gpio)
    serial_opts.add_argument('--gpiopin', dest='gpiopin', type=str,
        nargs=1, default=_d_gpio,
        help='GPIO pin to use for reset (%s)' % _d_gpio_pin)

    logging_opts = parser.add_argument_group('Logging options')
    logging_opts.add_argument('-L', dest='logfile', type=str,
        nargs=1, default=_d_logfile,
        help='Log to this file (%s)' % _d_logfile)
    logging_opts.add_argument('--loglevel', dest='loglevel',
        type=str, nargs=1, default=_d_loglevel,
        help='Loglevel to use (%s)' % _d_loglevel)

    output_opts = parser.add_argument_group('Output options')
    output_opts.add_argument('--stdout', dest='stdout',
        type=str, nargs='?', default=_d_stdout,
        help='Send output to stdout (%s)' % _d_stdout)
    output_opts.add_argument('--packetlog', dest='packetlog',
        type=str, nargs='?', default=_d_packetlog,
        help='Log each individual packet (%s)' % _d_packetlog)
    output_opts.add_argument('--averagelog', dest='averagelog',
        type=str, nargs='?', default=_d_averagelog,
        help='Log the average values (%s)' % _d_averagelog)
    output_opts.add_argument('--graphite', dest='graphite',
        type=str, nargs='*', default=_d_graphite,
        help='Log to graphite (%s)' % _d_graphite)

    args = parser.parse_args()

    logger = setup_logging(args)

    gpio = GPIO(device=args.gpio, pin=args.gpio_pin)
    gpio.pin_set(0)

    serial_port = Serial(line=args.line, speed=args.speed)
    serial_port.connect()

    wsr = WeatherStationReceiver(args.line, args.speed)
    wsr.setup_serial()

    ## Double fork()
    logger.debug('initializing')
    if args.daemonize:
        logger.debug('daemonizing')
        double_fork(args)
    else:
        setuid(args)

    wsr.run()
