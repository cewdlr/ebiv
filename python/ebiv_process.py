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
    CalcVarForShift, FindPeakInVarianceMap

import numpy as np
import os
from time import time

tim0=time()

dataDir = '../sample_data/'
fileStub = 'wallflow4_dense_3'
fnRAW = dataDir + fileStub + '.raw'
plotDir = dataDir + 'plots/'
destDir = dataDir

if not os.path.isfile(fnRAW):
    raise IOError('File not found: ' + fnRAW)
if not os.path.exists(plotDir):
    os.makedirs(plotDir)

#%% Processing parameters
evalMethods = ['motion_comp', 'corr_sum']
evalMethod = evalMethods[1]
t0 = 10000
duration = 10000
dt = duration
t_sep = 3000    # separation in time for event sampling
NT = 20     # number of planes in sub-volumes

# Note: for sum-of-correlation base-2 sizes are faster
szSample = [40,40]  # size of sampling area [W,H]
#szSample = [32,32]  # size of sampling area [W,H]
polarity = 'pos'

# scan range for velocity in [pixel/ms]
# may need to be tuned to specific data set
VxMin = -2
VxMax = 5
VyMin = -3
VyMax = 3
velResolX = 0.25
velResolY = 0.25
VxIN = np.arange(VxMin,VxMax+velResolX,velResolX)
VyIN = np.arange(VyMin,VyMax+velResolY,velResolY)
timeSample = [t0,t0+dt]
# offset symmetrically in time backward and forward
timeSample1 = [timeSample[0]-t_sep/2, timeSample[1]-t_sep/2]
timeSample2 = [timeSample[0]+t_sep/2, timeSample[1]+t_sep/2]

#%% Load events from file and compute variance map
print("Loading events from: " + fnRAW )
ev = EBIDataSet(fnRAW)
imgH,imgW = ev.imageSize()

velData = []
cnt = 0
ny = 0
sampleW,sampleH = szSample
stepX = sampleW//2
stepY = sampleH//2
# stepX = sampleW
# stepY = sampleH
tim1=time()

for iy in range(0,imgH-sampleH+1, stepY):
    nx = 0
    ny += 1
    for ix in range(0,imgW-sampleW+1, stepX):
        pos = [ix,iy]
        nx += 1
        if evalMethod in ['motion_comp', 'motion']:
            s1 = ev.sampleEvents(pos, szSample, timeSample, polarity=polarity)
            heatMap, cntsMap = CalcVarForShift(VxIN, VyIN, s1)
            [Vx,Vy,maxVar,evCnt] = FindPeakInVarianceMap(heatMap, cntsMap, VxIN, VyIN )
            dt = (s1.evData['roiTime'][1]-s1.evData['roiTime'][0])
            dx = Vx * dt/1000
            dy = Vy * dt/1000
            # print('[%4d %4d]  vx: %.4f vy:%.4f [px/ms]  shift [px]: %.2f  %.2f  (var=%.3f, cnt=%d)' \
            #       %(ix+sampleW/2,iy+sampleH/2, -Vx, -Vy, -dx, -dy, maxVar, evCnt))

        elif evalMethod in ['corr', 'corr_sum']:
            s1 = ev.sampleEvents(pos, szSample, timeSample1, polarity=polarity)
            s2 = ev.sampleEvents(pos, szSample, timeSample2, polarity=polarity)
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
                vol1 = s1.asVolume(NT)
                vol2 = s2.asVolume(NT)
                corr = FFT_CrossCorr2D(vol1,vol2)
                corr2D = corr.sum(axis=0)
                corr_dx,corr_dy,maxCorr = FindCorrPeak2D(corr2D)
                corr_vx = (corr_dx/t_sep)*1000
                corr_vy = (corr_dy/t_sep)*1000
                # print('[%4d %4d]  dx:%.4f dy:%.4f  vx:%.4f vy:%.4f  corr: %.4f' \
                #       %(ix+sampleW/2,iy+sampleH/2, corr_dx, -corr_dy, corr_vx, -corr_vy, maxCorr))
                Vx = corr_vx
                Vy = corr_vy
                dx = corr_dx
                dx = corr_dy
                maxVar = maxCorr
        print('[%4d %4d]  vx: %.4f vy:%.4f [px/ms]  (corr=%.3f, cnt=%d)' \
              %(ix+sampleW/2,iy+sampleH/2, -Vx, -Vy, maxVar, evCnt))
        velData.append([s1.centroid[0], imgH-s1.centroid[1], Vx,Vy, maxVar,evCnt])
        cnt+=1
    #break

tim2=time()
print('Loading event data took %.1f s' % (tim1-tim0))
print('Processing %d positions took %.1f min' % (cnt,(tim2-tim1)/60))

# final velocity data, reshaped as one time-step
velDataOUT = np.array(velData).flatten().reshape(1,ny,nx, 6)

fnDataOUT = destDir + 'veldata_t%.fms_w%d' % ((t0/1000),sampleW)
if evalMethod in ['corr', 'corr_sum']:
    fnDataOUT += '_corr_tau%.fms' % (t_sep/1000)
else:
    fnDataOUT += '_motion_res' + str(velResolX).replace('.','_') 
    
# save as numpy file
np.savez(fnDataOUT, velDataOUT)
print('Velocity data stored in: ' + fnDataOUT)
