# -*- coding: utf-8 -*-
"""

@author: C.Willert (cewdlr)

Purpose:
    generate Figs.7 and 8 of EBIV ExiF-Paper
"""
from pyebiv import EBIV

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

print("Loading events from: " + fnRAW )
evData = EBIV(fnRAW)
imgH,imgW = evData.sensorSize();
print('Number of events: ' + str(evData.eventCount()))
print('Sensor size: ' + str([imgH,imgW]))

# generate pseudo-image
t0 = 10000
duration = 10000
pseudoImg = np.array(evData.pseudoImage(t0,duration, 1)).reshape(imgH,imgW)
print('pseudo-image range: %g - %g' % (pseudoImg.min(),pseudoImg.max()))

#%% plot pseudo-image --------
bgcolor = '#CCCCCC'
cmap = 'bwr'
figsize=(8,4.5)
if useROI: figsize=(8,5.5)
fig, ax1 = plt.subplots(1,1, figsize=figsize)# sharex=True, sharey=True)
ax1.set_facecolor(bgcolor)   # change background to make streak mre visible
zeros = np.where(pseudoImg == 0)
pseudoImg = (pseudoImg - duration/2)/1000
pseudoImg[zeros] = np.nan
h,w = pseudoImg.shape

szSample = [40,40]  # size of sampling area
# sampling location near bottom wall
posX1 = 300
posY1 = 600
posX2 = 915
posY2 = 600
posSample1 = [posX1,posY1]
posSample2 = [posX2,posY2]
posSample = posSample2

if useROI:
    fnSuffix = '_%dms_x%d_y%d' % (int(duration/1000), posSample[0], posSample[1])
    fnPlot = plotDir + 'events' + fnSuffix + '_roi'
    x1 = posSample[0]-80
    y1 = posSample[1]-50
    imgROI = pseudoImg[y1:y1+150,x1:x1+200]
    extent = [x1,x1+200, imgH-(y1+150),imgH-y1]
else:
    fnPlot = plotDir + 'events_%dms' % int(duration/1000)
    imgROI = pseudoImg
    extent = [0,w,0,h]

imgPos = ax1.imshow(np.flipud(imgROI), cmap=cmap, interpolation='nearest', 
                    origin='lower', extent= extent, \
                    vmin=-duration/2000, vmax=duration/2000)
# add rectangle to indicate sample
for pos in [posSample1,posSample2]:
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
