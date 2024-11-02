'''
Support library for ADS8684_fast_readout project

Francis Deck
'''

import serial
import json
import numpy as np

class Ads8684_fast_readout():
    def __init__(self, port):
        '''
        port = port name, 'COMx' on Windows or something like 'ttyUSB0 on Linux'
        '''
        self.ser = serial.Serial(port = port, timeout = 5)
        '''
        Scale and offset values assuming internal voltage reference
        '''
        vref = 4.096
        self.scales = np.array([5*vref, 2.5*vref, 1.25*vref, 2.5*vref, 1.25*vref])/65536
        self.offsets = np.array([-2.5*vref, -1.25*vref, -0.625*vref, 0, 0])
        
    def close(self):
        self.ser.close()
        
    def __transact(self, command):
        '''
        Basic transaction with Teensy
        command = dict that can be turned into JSON string
        returns: JSON string converted back into a dict
        '''
        self.ser.flushInput()
        s = json.dumps(command) + '\n'
        self.ser.write(s.encode())
        t = ''
        while True:
            c = self.ser.read(1).decode()
            if len(c) == 0:
                print('Timeout, already received', t)
            if c == '\n':
                break
            t = t + c
        return json.loads(t)
        
    def status(self):
        '''
        Get verbose status from device
        returns: Status dict
        '''
        command = {'status': 1}
        return self.__transact(command)
    
    def chans(self, chans):
        '''
        Set list of channels to use
        chans = list of channels, e.g., [0, 1, 2, 3]
        returns: Status dict
        '''
        command = {'chans': chans}
        return self.__transact(command)
    
    def ranges(self, ranges):
        '''
        Set list of ranges
        ranges = list of range numbers, see self.scales and self.offsets
        returns: Status dict
        '''
        command = {'ranges': ranges}
        return self.__transact(command)
    
    def readTextMode(self, npts):
        '''
        Read array in text mode, strictly for troubleshooting
        npts = number of points to read
        Returns: Status and result dict
        '''
        command = {'npts': npts, 'read': 1}
        result = self.__transact(command)
        command = {'print': 1}
        result['data'] = self.__transact(command)['data']
        return result
        
    def readBinaryMode(self, npts, volts = True):
        '''
        Read array, receive data in binary mode
        npts = number of points to read
        volts = True if output should be scaled in Volts
        returns: Array, broken into channels, scaled in Volts
        '''
        status = self.status()
        '''
        Transaction is a bit more complicated... the read followed by dump
        causes a JSON to be returned with some necessary stuff such as the
        number of binary bytes to expect
        '''
        nb = self.__transact({'npts': npts, 'read': 1, 'dump': 1})['bytes']
        '''
        Next, data are transmitted to us in a block of binary bytes of length
        as predicted by the device, which we then turn into an array of integers
        '''
        s = self.ser.read(nb)
        if len(s) < nb:
            print('Timeout')
        y = np.frombuffer(s, dtype = np.uint16)
        '''
        Finally we unpack the array into arrays from the individual channels
        that are turned on, and scale each one in Volts
        '''
        nchans = len(status['chans'])
        if len(y) % nchans != 0:
            print('Number of points must be divisible by channels') 
        scales = np.array([self.scales[i] for i in status['ranges']])
        offsets = np.array([self.offsets[i] for i in status['ranges']])
        if volts:
            ya = y.reshape((len(y)//nchans, nchans)).transpose()*scales[:, np.newaxis] + offsets[:, np.newaxis]
        else:
            ya = y.reshape((len(y)//nchans, nchans)).transpose()
        return ya
        
    def reset(self):
        return self.__transact({'reset': 1})