# -*- coding: utf-8 -*-
"""
@author: Chris Willert (cewdlr)

CopyPolicy: 
    Released under the terms of the LGPLv2.1 or later, see LGPL.TXT

Purpose:
    parameter management for EBIV processing,
    uses INI-file format
"""
import configparser
import os

class EBIV_Config:
    def __init__(self, config_file):
        self.parser = configparser.ConfigParser()
        # default parameters - todo...
        self.doSingleTimeStep = True
        self.evalMethods = ['motion_comp', 'corr_sum', 'piv']
        self.evalMethod = self.evalMethods[0]
        
        # sampling locations for Fig.9/Fig.10 near bottom wall
        self.sampleLocation1 = [300,600]
        self.sampleLocation2 = [915,600]
        
        self.dataDir = '../sample_data/'
        self.destDir = self.dataDir
        self.plotDir = self.dataDir + 'plots/'
        self.fileStub = 'wallflow4_dense_3'
        self.imgW = 1280
        self.imgH = 720

        if os.path.isfile(config_file):
            self.read_config(config_file)
        else:
            raise IOError('No such file: ' + config_file)
    
    def read_config(self, config_file):
        self.parser.read(config_file)
        self.config_file = config_file

        sec = self.parser['DEFAULT']
        # event sampling in time
        self.t_start  = sec.getint('SampleTimeStart', 10000)
        self.t_end    = sec.getint('SampleTimeEnd',  10000)
        self.t_step   = sec.getint('SampleTimeStep', 10000)
        self.t_sample = sec.getint('SampleDuration', 10000)
        #self.duration = self.t_sample

        # event sampling in space
        self.sampleW = sec.getint('SampleWidth',40)
        self.sampleH = sec.getint('SampleHeight',40)
        self.sampleStepX = sec.getint('SampleStepX',20)
        self.sampleStepY = sec.getint('SampleStepY',20)
        self.polarity = sec.get('EventPolarity')

        self.doGaussPeakFit = sec.getboolean('DoGaussPeakFit', True)
        
        # for sum-of-correlation approach
        sec = self.parser['CORR_SUM']
        self.t_sep = sec.getint('TimeOffset', 3000)  # separation in time for event sampling
        self.NT    = sec.getint('NT', 20)               # number of planes in sub-volumes

        # for motion compensation approach
        sec = self.parser['MOTION_COMP']
        # scan range for velocity in [pixel/ms]
        # may need to be tuned to specific data set
        self.VxMin = sec.getfloat('ScanRangeVxMin',-2)
        self.VxMax = sec.getfloat('ScanRangeVxMax', 5)
        self.VyMin = sec.getfloat('ScanRangeVyMin',-3)
        self.VyMax = sec.getfloat('ScanRangeVyMax', 3)
        self.velResolX = sec.getfloat('ScanResolX',0.25)
        self.velResolY = sec.getfloat('ScanResolY',0.25)
        self.objectiveFxn = sec.get('ObjectiveFxn')
        # event interpolation onto pixels: 'nearest' or 'bilinear'
        self.interpolation = sec.get('Interpolation')

        # parameters unique for PIV processing
        sec = self.parser['PIV']
        self.PIV_delay = sec.getfloat('PIV_Delay',1000)
        
    def save_config(self, config_file):
        # currently not working
        self.parser.write(config_file)
        self.config_file = config_file
        
    def rawFile(self, ext='.raw'):
        return self.dataDir + self.fileStub + ext
    
    def velocityDataFile(self): 
        fnDataOUT = 'veldata_t%.fms_w%d' % ((self.t_start/1000),self.sampleW)
        if self.evalMethod in ['corr', 'corr_sum']:
            fnDataOUT += '_corr_tau%.fms' % (self.t_sep/1000)
        else:
            fnDataOUT += '_motion_' + self.objectiveFxn.lower() + '_res' + str(self.velResolX).replace('.','_') 
        return fnDataOUT

if __name__ == '__main__':
    # for testing...
    cfgFile = '../sample_data/ebiv_params.cfg'
    
    cfg = EBIV_Config(cfgFile)
    #cfg.save_config('current.cfg')
    print('Velocity data file: ' + cfg.velocityDataFile() )
