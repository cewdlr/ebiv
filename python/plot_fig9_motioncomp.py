# -*- coding: utf-8 -*-
"""

@author: cewdlr

Purpose:
    generate Fig.9 of EBIV ExiF-Paper
    showing motion-compensation method on actual data
"""

from pyebiv import EBIV
from ebiv_utils import EBIDataSet,CalcVarForShift,FindPeakInVarianceMap

import numpy as np
import os
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from mpl_toolkits.axes_grid1 import make_axes_locatable

useROI = 0      # plots small ROIs shown in Fig.8
savePlot = 0    # generate output

dataDir = 'Y:/Documents/GitHub/ebiv/sample_data/'
fileStub = 'wallflow4_dense_3'
fnRAW = dataDir + fileStub + '.raw'
plotDir = dataDir + 'plots/'

if not os.path.isfile(fnRAW):
    raise IOError('File not found: ' + fnRAW)
if not os.path.exists(plotDir):
    os.makedirs(plotDir)

#%% Processing parameters
t0 = 10000
duration = 10000
dt = duration

szSample = [40,40]  # size of sampling area
polarity = 'pos'
# sampling location near bottom wall
posX1 = 300
posY1 = 600
posX2 = 915
posY2 = 600
posSample1 = [posX1,posY1]
posSample2 = [posX2,posY2]
posSample = posSample2
timeSample = [t0,t0+dt]

# scan range for velocity in [pixel/ms]
VxMin = -4
VxMax = 4
VyMin = -4
VyMax = 4
velResolX = 0.25
velResolY = 0.25
VxIN = np.arange(VxMin,VxMax+velResolX,velResolX)
VyIN = np.arange(VyMin,VyMax+velResolY,velResolY)
fnSuffix = '_%dms_x%d_y%d' % (int(duration/1000), posSample[0], posSample[1])
fnSuffixResol = fnSuffix + '_res' + str(velResolX).replace('.','_')
bgcolor = '#CCCCCC'
cmap1 = 'hot'
cmap2 = 'bwr'

#%% Load events from file and compute variance map
print("Loading events from: " + fnRAW )
ev = EBIDataSet(fnRAW)
s = ev.sampleEvents(posSample, szSample, timeSample, polarity=polarity)
heatMap,cntsMap = CalcVarForShift(VxIN, VyIN, s)
[Vx,Vy,maxVar,evCnt] = FindPeakInVarianceMap(heatMap, cntsMap, VxIN, VyIN)
print('vx:%.4f vy:%.4f  var: %.4f  cnts: %d' %(-Vx, -Vy, maxVar, evCnt))

#%% Plot heat-map
fig, ax1 = plt.subplots(1,1,figsize=(4,3.5))# sharex=True, sharey=True)
imgPos = ax1.imshow(np.flipud(heatMap), cmap=cmap1, interpolation='nearest', origin='lower',
           extent=[VxIN.min(),VxIN.max(),VyIN.min(),VyIN.max()],
           vmin=0)
# mark origin
ax1.plot(0,0, '+', ms=10, mew=None, color='#00FF00', mec=None)
ax1.plot(Vx,-Vy, '+', ms=10, mew=None, color='#0000FF', mec=None)
ax1.set_xlabel('Velocity X [px/ms]')
ax1.set_ylabel('Velocity Y [px/ms]')
ax1.set_title('%g pixel/ms sampling' % (velResolX), fontsize=15)
divider = make_axes_locatable(ax1)
cax = divider.append_axes("right", size="4%", pad=0.05)
cbar = plt.colorbar(imgPos, cax=cax) 
ax1.text(VxMax+0.1*(VxMax-VxMin),VyMax+0.1*(VyMax-VyMin), r'$\sigma_I^2$', fontsize=15)

plt.tight_layout()
if savePlot:
    fnPlot = plotDir + 'varmap' + fnSuffixResol
    plt.savefig(fnPlot+'.png', dpi=200)
    print('plot saved in: ' + fnPlot+'.png')
plt.show()

#%% Plot samples
vx = Vx/1000
vy = Vy/1000
dt = (s.evData['roiTime'][1]-s.evData['roiTime'][0])
dx = Vx*dt/1000
dy = Vy*dt/1000
mean, var, eventsUsed = s.sumWarpedEvents(vx,vy)
print('vx:%.4f vy:%.4f  dx:%.2f dy:%.2f  mean: %.4f  var: %.4f  cnts: %d' \
      %(-vx*1000, -vy*1000, dx, dy, mean,var, evCnt))

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
fig, [ax0, ax1, ax2] = plt.subplots(1,3, sharex=True, sharey=True, figsize=(8,3))
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
divider2 = make_axes_locatable(ax2)
cax2 = divider2.append_axes("right", size="5%", pad=0.15)
#cbar1 = plt.colorbar(imgPos1, cax=cax1, orientation='vertical') 
cbar2 = plt.colorbar(imgPos2, cax=cax2, orientation='vertical') 
ax0.set_title('Sampled events')
ax1.set_title('Original events')
ax2.set_title('Warped events')
for ax in [ax0,ax1,ax2]:
    ax.set_xlim(-nc/2,nc/2)
    ax.set_ylim(-nr/2,nr/2)
    ax.set_xlabel('column [pixel]')
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

plt.tight_layout()
if savePlot:
    fnPlot = plotDir + 'samples' + fnSuffixResol
    plt.savefig(fnPlot+'.png', dpi=200)
    print('Plot saved in: ' + fnPlot+'.png')
plt.show()