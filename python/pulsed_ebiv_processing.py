# -*- coding: utf-8 -*-
"""
@author: Chris Willert (cewdlr)

CopyPolicy: 
    Released under the terms of the LGPLv2.1 or later, see LGPL.TXT

Purpose:
    Load raw event data of pulsed-illuminated scene 
    generate event-vs-time histogram 
    generate binary pseudo images using known pulse interval
    plot the images
"""

import numpy as np
import os
import matplotlib.pyplot as plt
from pyebiv import EBIV

# location of event data
dataDir = '../sample_data/'
fnRAW = dataDir + 'starting_jet_20230707_164949.raw'
if not os.path.exists(fnRAW):
    raise FileNotFoundError(fnRAW)

#%% load events
evtData = EBIV(fnRAW)
print('Number of events: ' + str(evtData.eventCount()))

evPol = np.array(evtData.p())
evX = np.array(evtData.x())
evY = np.array(evtData.y())
evT = np.array(evtData.time())
# only look at positive events
posEvents = np.where(evPol == 1)
posX = evX[posEvents]
posY = evY[posEvents]
times = evT[posEvents]   

#%% estimate time to beginning of first pulse interval
pulseFreq = 200 # in [Hz] this is normally known from the experiment
pulsePeriod = (1e6 / pulseFreq)
nBins = 100
binWidth = int(pulsePeriod / nBins)
pulseOffset = evtData.estimatePulseOffsetTime(pulseFreq, binWidth, 100, 0)
print('Estimated pulse offset time: %s usec  [binWidth=%d usec]' % (pulseOffset,binWidth))

#%% plot event histogram
t_max_bins = int(np.round(max(times)/binWidth))
ev_hist_cnts, ev_hist_bins = np.histogram(times, bins=t_max_bins, range=(0,t_max_bins*binWidth))
ev_hist_normcnts = ev_hist_cnts.astype(float) / binWidth

fig, [ax1,ax2,ax3 ] = plt.subplots(3,1, figsize=(8,7), )

nb = min(1000, ev_hist_cnts.size)
ax1.plot(ev_hist_bins[:nb], ev_hist_normcnts[:nb], color='k', )
ax1.set_ylabel(r'events/$\mu$s')
    
idx0 = 0
nb2 = min(300, ev_hist_cnts.size-idx0)
t0 = ev_hist_bins[idx0]
#ax2.plot(ev_hist_bins[idx0:idx0+nb2], ev_hist_cnts[idx0:idx0+nb2], 'k', ms=1)
ax2.bar(ev_hist_bins[idx0:idx0+nb2], ev_hist_normcnts[idx0:idx0+nb2], 
        width=binWidth, color='k', )
for t in np.arange(t0, ev_hist_bins[idx0+nb2], pulsePeriod):
    ax2.axvline(pulseOffset + t, color='r')
ax2.set_ylabel(r'events/$\mu$s')

# find mean pulse shape for entire record
timesRem = np.remainder(times, pulsePeriod)
nPulses = times.max() // pulsePeriod
binWidth2 = 10
t_max_bins = int(np.round(pulsePeriod/binWidth2))
mean_pulse_cnts, mean_pulse_bins = np.histogram(timesRem, bins=t_max_bins, range=(0,t_max_bins*binWidth2))
mean_pulse_normcnts = mean_pulse_cnts.astype(float) / (binWidth2 * nPulses)
ax3.bar(mean_pulse_bins[:-1], mean_pulse_normcnts, 
        width=binWidth, color='k', )
ax3.axvline(pulseOffset, color='r')

ax3.set_xlabel(r'Time [$\mu$s]')
ax3.set_ylabel(r'events/$\mu$s')

plt.show()

#%% generate some pseudo-images using the known offset
H,W = evtData.sensorSize()
imgs = []
startTime = 1.5e6 # [usec]
startFrame = startTime // pulsePeriod
for i in range(10):
    frameIdx = startFrame + i
    t0 = ((frameIdx*pulsePeriod) + pulseOffset)
    img = np.array(evtData.pseudoImage(int(t0),int(pulsePeriod), 1)).reshape(H,W)
    img[img > 0] = 1    # turn into binary image
    imgs.append(img)
    if i==0:
        sumImg = np.zeros_like(img)
    sumImg[img > 0] = (i+1)*pulsePeriod
    
imgs = np.stack(imgs)
 
#%% plot image of tracks
fig, ax1  = plt.subplots(1,1, figsize=(6,4), )
cmap = 'gray_r'

maxImg = np.sum(imgs, axis=0)
maxImg[maxImg > 0] = 1  # binary image for display

ax1.imshow(np.flipud(maxImg), cmap=cmap, origin='lower', )

plt.show()
   
#%% plot detail of images
fig, ax1  = plt.subplots(1,1, figsize=(6,4), )
bgcolor = '#AAAAAA'
cmap = 'bwr'
ax1.set_facecolor(bgcolor)   # change background to make streaks more visible

maskedImg = sumImg.copy()
maskedImg[sumImg == 0] = np.nan

ax1.imshow(np.flipud(maskedImg), cmap=cmap, interpolation='nearest', origin='lower', )
ax1.set_xlim(500,900)
ax1.set_ylim(400,700)

plt.show()
    
    
