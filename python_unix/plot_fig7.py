# -*- coding: utf-8 -*-
"""

@author: C.Willert (cewdlr)

Purpose:
    generate Figs.7 and 8 of EBIV ExiF-Paper
"""
from pyebiv import EBIV
from ebiv_params import EBIV_Config

import numpy as np
import os
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from mpl_toolkits.axes_grid1 import make_axes_locatable

useROI = 1      # plots small ROIs shown in Fig.8
savePlot = 0    # generate output

#%% Processing parameters
dataDir = '../sample_data/'
cfgFile = dataDir + 'ebiv_params.cfg'
cfg = EBIV_Config(cfgFile)

fnRAW = cfg.rawFile()
plotDir = dataDir + 'plots/'

if not os.path.isfile(fnRAW):
    raise IOError('File not found: ' + fnRAW)
if not os.path.exists(plotDir):
    os.makedirs(plotDir)

print("Loading events from: " + fnRAW )
evData = EBIV(fnRAW)
imgH,imgW = evData.sensorSize();
print('Number of events: ' + str(evData.eventCount()))
print('Sensor size: ' + str([imgH,imgW]))

# generate pseudo-image
#t0 = 10000
#duration = 10000
pseudoImg = np.array(evData.pseudoImage(cfg.t_start,cfg.t_sample, 1)).reshape(imgH,imgW)
print('pseudo-image range: %g - %g' % (pseudoImg.min(),pseudoImg.max()))

#%% plot pseudo-image --------
bgcolor = '#CCCCCC'
cmap = 'bwr'
figsize=(8,4.5)
if useROI: figsize=(8,5.5)
fig, ax1 = plt.subplots(1,1, figsize=figsize)# sharex=True, sharey=True)
ax1.set_facecolor(bgcolor)   # change background to make streak mre visible
zeros = np.where(pseudoImg == 0)
pseudoImg = (pseudoImg - cfg.t_sample/2)/1000
pseudoImg[zeros] = np.nan
h,w = pseudoImg.shape

szSample = [cfg.sampleW,cfg.sampleH]  # size of sampling area
# sampling location near bottom wall
posSample = cfg.sampleLocation1

if useROI:
    fnSuffix = '_%dms_x%d_y%d' % (int(cfg.t_sample/1000), posSample[0], posSample[1])
    fnPlot = plotDir + 'events' + fnSuffix + '_roi'
    x1 = posSample[0]-80
    y1 = posSample[1]-50
    imgROI = pseudoImg[y1:y1+150,x1:x1+200]
    extent = [x1,x1+200, imgH-(y1+150),imgH-y1]
else:
    fnPlot = plotDir + 'events_%dms' % int(cfg.t_sample/1000)
    imgROI = pseudoImg
    extent = [0,w,0,h]

imgPos = ax1.imshow(np.flipud(imgROI), 
                    cmap=cmap, interpolation='nearest', 
                    origin='lower', extent= extent, 
                    vmin=-cfg.t_sample/2000, 
                    vmax= cfg.t_sample/2000)
# add rectangle to indicate sample
for pos in [cfg.sampleLocation1,cfg.sampleLocation2]:
    rectSample = mpatches.Rectangle([pos[0],imgH-pos[1]-szSample[1]], \
                                    szSample[0], szSample[1], \
                                    lw=1, ec='k', fc='none')
    ax1.add_patch(rectSample)
# indicate wall
#if not useROI:
ax1.axhline(31, ls='--', color='k', lw=0.5)
ax1.set_xlabel('X [pixel]')
ax1.set_ylabel('Y [pixel]')
divider = make_axes_locatable(ax1)
cax = divider.append_axes("right", size="3%", pad=0.05)
cbar = plt.colorbar(imgPos, cax=cax)    
cbar.set_label(r'Time [ms]')
cbar.minorticks_on()
plt.tight_layout()

if savePlot:
    plt.savefig(fnPlot+'.png', dpi=200)
    #plt.savefig(fnPlot+'.pdf')
    print('plot saved in: ' + fnPlot+'.png')

plt.show()
