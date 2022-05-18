# -*- coding: utf-8 -*-
"""
@author: Chris Willert (cewdlr)

CopyPolicy: 
    Released under the terms of the LGPLv2.1 or later, see LGPL.TXT

Purpose:
    generate Fig.9 of EBIV ExiF-Paper
    showing motion-compensation method on actual data
"""

from ebiv_utils import EBIDataSet,CalcObjectiveForShift,FindPeakInObjectiveMap
from ebiv_params import EBIV_Config
from plot_utils import SetTimesFont,PlacePlotLabel

import numpy as np
import os
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from mpl_toolkits.axes_grid1 import make_axes_locatable

useROI = 0      # plots small ROIs shown in Fig.8
savePlot = 0    # generate output

#%% Processing parameters
dataDir = '../sample_data/'
cfgFile = dataDir + 'ebiv_params.cfg'
cfg = EBIV_Config(cfgFile)

fnRAW = cfg.rawFile()
SetTimesFont()

if not os.path.isfile(fnRAW):
    raise IOError('File not found: ' + fnRAW)
if not os.path.exists(cfg.plotDir):
    os.makedirs(cfg.plotDir)

szSample = [cfg.sampleW,cfg.sampleH]  # size of sampling area
objectiveFxns = [
    'var',      # variance
    'SumSq',    # sum of squares, produces same result as 'var'
    'SumExp',   # sum of exponentials
    'MOA',      # max of accumulations 
    'isoa',     # as specified by Stoffregen et al., CVPR2019
    'R1',       # as specified by Stoffregen et al., CVPR2019
    ]
cfg.objectiveFxn = objectiveFxns[0]
#cfg.interpolation='nearest' # or 'bilinear'

# sampling locations near bottom wall
posSample = cfg.sampleLocation2
timeSample = [cfg.t_start,cfg.t_start+cfg.t_sample]

# scan range for velocity in [pixel/ms]
cfg.VxMin = -4
cfg.VxMax = 4
cfg.VyMin = -4
cfg.VyMax = 4
VxIN = np.arange(cfg.VxMin,cfg.VxMax+cfg.velResolX,cfg.velResolX)
VyIN = np.arange(cfg.VyMin,cfg.VyMax+cfg.velResolY,cfg.velResolY)
fnSuffix = '_%dms_x%d_y%d' % (int(cfg.t_sample/1000), posSample[0], posSample[1])
fnSuffixResol = fnSuffix + '_res' + str(cfg.velResolX).replace('.','_')
bgcolor = '#CCCCCC'
cmap1 = 'hot'
cmap2 = 'bwr'

#%% Load events from file and compute variance map
print("Loading events from: " + fnRAW )
ev = EBIDataSet(fnRAW)
s = ev.sampleEvents(posSample, szSample, timeSample, polarity=cfg.polarity)
heatMap,cntsMap = CalcObjectiveForShift(
    VxIN, VyIN, s, objectiveFxn=cfg.objectiveFxn, interpolation=cfg.interpolation)
[Vx,Vy,maxVar,evCnt] = FindPeakInObjectiveMap(heatMap, cntsMap, VxIN, VyIN,
                                             doExponentialFit=cfg.doGaussPeakFit)
print('vx:%.4f vy:%.4f  reward-value: %.4f  cnts: %d' %(-Vx, -Vy, maxVar, evCnt))

#%% Plot heat-map
fig, ax1 = plt.subplots(1,1,figsize=(4,3.5))# sharex=True, sharey=True)
extent=[VxIN.min(),VxIN.max(),VyIN.min(),VyIN.max()]
if cfg.objectiveFxn in ['SumExp']:
    imgPos = ax1.imshow(np.log(np.flipud(heatMap)), cmap=cmap1, interpolation='nearest', 
                        origin='lower', extent=extent,)
else:
    imgPos = ax1.imshow(np.flipud(heatMap), cmap=cmap1, interpolation='nearest', 
                        origin='lower', extent=extent, vmin=0)
    
# mark origin
ax1.plot(0,0, '+', ms=10, mew=None, color='#00FF00', mec=None)
ax1.plot(Vx,-Vy, '+', ms=10, mew=None, color='#0000FF', mec=None)
ax1.set_xlabel('Velocity X [px/ms]')
ax1.set_ylabel('Velocity Y [px/ms]')
ax1.set_title('%g pixel/ms sampling' % (cfg.velResolX), fontsize=15)
divider = make_axes_locatable(ax1)
cax = divider.append_axes("right", size="4%", pad=0.05)
cbar = plt.colorbar(imgPos, cax=cax) 
ax1.text(cfg.VxMax+0.1*(cfg.VxMax-cfg.VxMin),cfg.VyMax+0.1*(cfg.VyMax-cfg.VyMin), \
         r'$\sigma_I^2$', fontsize=15)

plt.tight_layout()
if savePlot:
    fnPlot = cfg.plotDir + cfg.objectiveFxn.lower() + '_map' + fnSuffixResol
    plt.savefig(fnPlot+'.png', dpi=200)
    print('plot saved in: ' + fnPlot+'.png')
plt.show()

#%% Plot samples
vx = Vx/1000
vy = Vy/1000
dt = (s.evData['roiTime'][1]-s.evData['roiTime'][0])
dx = Vx*dt/1000
dy = Vy*dt/1000
mean, rewardValue, eventsUsed = s.sumWarpedEvents(
    vx,vy, objectiveFxn=cfg.objectiveFxn, interpolation=cfg.interpolation)
print('vx:%.4f vy:%.4f  dx:%.2f dy:%.2f  mean: %.4f  value: %.4f  cnts: %d' \
    % (-vx*1000, -vy*1000, dx, dy, mean, rewardValue, evCnt))

sumImg = np.flipud(s.renderSample())
#print('range of sum-image: %g,%g' %(sumImg.min(), sumImg.max()))
warpImg = np.flipud(s.warpedSample())
pixImg = np.ones_like(sumImg)
sumZeros = np.where(sumImg == 0)
warpZeros = np.where(warpImg == 0)
nr,nc = sumImg.shape
sumImg[sumZeros] = np.nan
warpImg[warpZeros] = np.nan
pixImg[sumZeros] = np.nan
cmap = 'hot'
fig, [ax0, ax1, ax2] = plt.subplots(1,3, sharex=True, sharey=True, figsize=(9.2,3))
for ax in [ax0, ax1, ax2]:
    ax.set_facecolor(bgcolor)
extent=[-nc/2,nc/2,-nr/2,nr/2]
#dt =10000
imgPos0 = ax0.imshow((sumImg-dt/2)/1000, cmap=cmap2, interpolation='nearest', 
                     extent=extent,
                     origin='lower',
                     vmin=-dt/(2*1000),vmax=dt/(2*1000))
imgPos1 = ax1.imshow(pixImg, cmap=cmap1, interpolation='nearest', origin='lower',
                     extent=extent, vmin=1, vmax=20)
imgPos2 = ax2.imshow(warpImg, cmap=cmap1, interpolation='nearest', origin='lower',
                     extent=extent, vmin=1, vmax=20)
divider1 = make_axes_locatable(ax1)
divider2 = make_axes_locatable(ax2)
cax2 = divider2.append_axes("right", size="5%", pad=0.15)

cbar2 = plt.colorbar(imgPos2, cax=cax2, orientation='vertical') 
ax0.set_title('Sampled events')
ax1.set_title('Original events')
ax2.set_title('Warped events')
for ax,lbl in zip ([ax0,ax1,ax2], ['a)','b)', 'c)']):
    ax.set_xlim(-nc/2,nc/2)
    ax.set_ylim(-nr/2,nr/2)
    ax.set_xlabel('column [pixel]')
    PlacePlotLabel(ax, [-0.1,1.1], lbl, fontsize=15)

ax0.set_ylabel('row [pixel]')
#cbar1.set_label(r'Time [ms]')
cbar2.set_label(r'Pixel count')

# add vector
dy = -dy    # image origin at top
xc = -dx/2
yc = -dy/2
arrow = mpatches.FancyArrowPatch((xc, yc), (xc+dx, yc+dy), ec='k', fc='k',
                                 #transform=ax2.transAxes,
                                 mutation_scale=15, mutation_aspect=1, )
ax0.add_patch(arrow)
#ax2.add_patch(arrow)

plt.tight_layout(pad=1.03)
if savePlot:
    fnPlot = cfg.plotDir + 'samples' + fnSuffixResol
    plt.savefig(fnPlot+'.png', dpi=200)
    print('Plot saved in: ' + fnPlot+'.png')
plt.show()