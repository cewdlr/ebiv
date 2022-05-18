# -*- coding: utf-8 -*-
"""
@author: Chris Willert (cewdlr)

CopyPolicy: 
    Released under the terms of the LGPLv2.1 or later, see LGPL.TXT

Purpose:
    Compute velocity field from event-data using sum-of-correlation 
    or motion-compensation methods on actual data.
    
    Not the fastest, but serves to show the concepts
"""

#from pyebiv import EBIV
from ebiv_utils import EBIDataSet,FFT_CrossCorr2D,FindCorrPeak2D, \
    CalcObjectiveForShift, FindPeakInObjectiveMap
from ebiv_params import EBIV_Config

import numpy as np
import os
from time import time

tim0=time()

# Processing parameters
dataDir = '../sample_data/'
cfgFile = dataDir + 'ebiv_params.cfg'
cfg = EBIV_Config(cfgFile)

#cfg.fileStub = 'wallflow4_dense_3'
fnRAW = cfg.rawFile()
fnEVT = cfg.rawFile(ext='.evt')
if os.path.isfile(fnEVT): fnRAW = fnEVT

evalMethods = ['motion_comp', 'corr_sum']
cfg.evalMethod = evalMethods[0]

if not os.path.isfile(fnRAW):
    raise IOError('File not found: ' + fnRAW)

VxIN = np.arange( cfg.VxMin, cfg.VxMax+cfg.velResolX, cfg.velResolX)
VyIN = np.arange( cfg.VyMin, cfg.VyMax+cfg.velResolY, cfg.velResolY)


#%% Load events from file and compute variance map
print("Loading events from: " + fnRAW )
ev = EBIDataSet(fnRAW)
imgH,imgW = ev.imageSize()
szSample = [cfg.sampleW,cfg.sampleH]

velData = []
cnt = 0
nt = 0
tim1=time()

for t_0 in range(cfg.t_start, cfg.t_end, cfg.t_step):
    timeSample = [t_0, t_0 + cfg.t_sample]
    # offset symmetrically in time backward and forward
    timeSample1 = [timeSample[0]-cfg.t_sep//2, timeSample[1]-cfg.t_sep//2]
    timeSample2 = [timeSample[0]+cfg.t_sep//2, timeSample[1]+cfg.t_sep//2]
    nt += 1
    ny = 0
    for iy in range(0,imgH-cfg.sampleH+1, cfg.sampleStepY):
        nx = 0
        ny += 1
        for ix in range(0,imgW-cfg.sampleW+1, cfg.sampleStepX):
            pos = [ix,iy]
            nx += 1
            if cfg.evalMethod in ['motion_comp', 'motion']:
                s1 = ev.sampleEvents(pos, szSample, timeSample, polarity=cfg.polarity)
                rewardMap, cntsMap = CalcObjectiveForShift(VxIN, VyIN, s1, \
                                                           objectiveFxn=cfg.objectiveFxn,
                                                           interpolation=cfg.interpolation)
                [Vx,Vy,maxVar,evCnt] = FindPeakInObjectiveMap(
                    rewardMap, cntsMap, VxIN, VyIN, doExponentialFit=cfg.doGaussPeakFit )
                dt = (s1.evData['roiTime'][1]-s1.evData['roiTime'][0])
                dx = Vx * dt/1000
                dy = Vy * dt/1000
                # print('[%4d %4d]  vx: %.4f vy:%.4f [px/ms]  shift [px]: %.2f  %.2f  (var=%.3f, cnt=%d)' \
                #       %(ix+sampleW/2,iy+sampleH/2, -Vx, -Vy, -dx, -dy, maxVar, evCnt))
    
            elif cfg.evalMethod in ['corr', 'corr_sum']:
                s1 = ev.sampleEvents(pos, szSample, timeSample1, polarity=cfg.polarity)
                s2 = ev.sampleEvents(pos, szSample, timeSample2, polarity=cfg.polarity)
                evCnt1 = s1.evData['time'].size
                evCnt2 = s2.evData['time'].size
                #evCnt1 = 0
                if evCnt1 == 0 or evCnt2 == 0:
                    # print("[%4d %4d]  no events"%(ix,iy))
                    Vx = 0
                    Vy = 0
                    dx = 0
                    dx = 0
                    maxVar = 0
                    evCnt = 0
                else:
                    evCnt = (evCnt1+ evCnt2)//2
                    vol1 = s1.asVolume(cfg.NT)
                    vol2 = s2.asVolume(cfg.NT)
                    corr = FFT_CrossCorr2D(vol1,vol2)
                    corr2D = corr.sum(axis=0)
                    corr_dx,corr_dy,maxCorr = FindCorrPeak2D(corr2D)
                    corr_vx = (corr_dx/cfg.t_sep)*1000
                    corr_vy = (corr_dy/cfg.t_sep)*1000
                    # print('[%4d %4d]  dx:%.4f dy:%.4f  vx:%.4f vy:%.4f  corr: %.4f' \
                    #       %(ix+sampleW/2,iy+sampleH/2, corr_dx, -corr_dy, corr_vx, -corr_vy, maxCorr))
                    Vx = corr_vx
                    Vy = corr_vy
                    dx = corr_dx
                    dx = corr_dy
                    maxVar = maxCorr
            print('[%4d %4d]  vx: %.4f vy:%.4f [px/ms]  (peakval=%.3f, evtcnt=%d)' \
                  %(ix+cfg.sampleW/2,iy+cfg.sampleH/2, -Vx, -Vy, maxVar, evCnt))
            velData.append([s1.centroid[0], imgH-s1.centroid[1], Vx,Vy, maxVar,evCnt])
            cnt+=1

    if cfg.doSingleTimeStep:
        break

if cnt == 0 or nt == 0:
    raise Exception('No data processed')
tim2=time()
print('Loading event data took %.1f s' % (tim1-tim0))
print('Processing %d positions took %.1f min' % (cnt,(tim2-tim1)/60))

# final velocity data, reshaped as one time-step
velDataOUT = np.array(velData).flatten().reshape(nt,ny,nx, 6)

fnDataOUT = cfg.velocityDataFile()
    
# save as numpy file
np.savez(cfg.destDir + fnDataOUT, velDataOUT)
print('Velocity data stored in: ' + cfg.destDir + fnDataOUT)
